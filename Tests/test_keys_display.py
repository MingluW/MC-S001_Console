"""执行生产ARM ELF：真实按键消抖/应用/段码/LCD调度，GPIO输入由测试提供。"""
from pathlib import Path
import re
import runpy
import struct
from functools import reduce


lcd_env = runpy.run_path(str(Path(__file__).with_name("test_lcd.py")))
from elftools.elf.elffile import ELFFile
from elftools.dwarf.dwarf_expr import DWARFExprParser

env = lcd_env["env"]
uc, call, symbols = env["uc"], env["call"], env["symbols"]
scratch = env["scratch"]
return_value = env["return_value"]
gpio_state = lcd_env["gpio_hook"].__globals__
gpio_state["force_send_failure"] = False
raw_keys = 0

# 从ELF调试信息读取事件字段布局，不能假定ARM枚举占4字节。
with (env["root"] / "Makefile/build/MC-S001_Console.elf").open("rb") as stream:
    dwarf = ELFFile(stream).get_dwarf_info()
    event_die = next(die for unit in dwarf.iter_CUs() for die in unit.iter_DIEs()
                     if die.tag == "DW_TAG_typedef" and
                     die.attributes.get("DW_AT_name") and
                     die.attributes["DW_AT_name"].value == b"KeysEvent")
    members = list(event_die.get_DIE_from_attribute("DW_AT_type").iter_children())
    def member_offset(member):
        location = member.attributes["DW_AT_data_member_location"].value
        return location if isinstance(location, int) else \
            DWARFExprParser(dwarf.structs).parse_expr(location)[0].args[0]

    state_die = next(die for unit in dwarf.iter_CUs() for die in unit.iter_DIEs()
                     if die.tag == "DW_TAG_typedef" and
                     die.attributes.get("DW_AT_name") and
                     die.attributes["DW_AT_name"].value == b"AppState")
    state_struct = state_die.get_DIE_from_attribute("DW_AT_type")
    state_members = list(state_struct.iter_children())
    state_size = state_struct.attributes["DW_AT_byte_size"].value
    def type_size(die):
        while "DW_AT_byte_size" not in die.attributes:
            die = die.get_DIE_from_attribute("DW_AT_type")
        return die.attributes["DW_AT_byte_size"].value
    state_fields = {member.attributes["DW_AT_name"].value.decode():
                    (member_offset(member), type_size(member.get_DIE_from_attribute("DW_AT_type")))
                    for member in state_members if member.tag == "DW_TAG_member"}
    unit_offset = next(member_offset(member) for member in state_members
                       if member.tag == "DW_TAG_member" and
                       member.attributes["DW_AT_name"].value == b"speed_unit")
    event_fields = [(member_offset(member),
                     member.get_DIE_from_attribute("DW_AT_type")
                     .get_DIE_from_attribute("DW_AT_type").attributes["DW_AT_byte_size"].value)
                    for member in members if member.tag == "DW_TAG_member"]


def input_hook(machine, address, size, context):
    if address == (symbols["GPIO_ReadInputData"] & ~1):
        assert machine.reg_read(env["UC_ARM_REG_R0"]) == lcd_env["gpio_b"]
        pins = [13, 14, 10, 15, 11, 12]
        low = sum(1 << pin for bit, pin in enumerate(pins) if raw_keys & (1 << bit))
        return_value(0xFFFF & ~low)


uc.hook_add(env["UC_HOOK_CODE"], input_hook)
call("Log_SetLevel", 0)  # 只测控制，不让测试期间的变化日志填满UART队列。
assert call("BoardKeys_Read") == 0
raw_keys = 1
assert call("BoardKeys_Read") == 1
raw_keys = 2
assert call("BoardKeys_Read") == 2
raw_keys = 3
assert call("BoardKeys_Read") == 3


def init_keys(t=0, raw=0):
    global raw_keys
    raw_keys = raw
    call("Keys_Init", t)


def scan(t, raw):
    global raw_keys
    raw_keys = raw
    call("Keys_Process", t & 0xFFFFFFFF)


def pop():
    if not call("Keys_PopEvent", scratch):
        return None
    return tuple(int.from_bytes(uc.mem_read(scratch + offset, size), "little")
                 for offset, size in event_fields)


init_keys()
scan(10, 2)
scan(20, 0)  # 抖动不足20ms，不产生事件。
scan(30, 2)
scan(49, 2)
assert pop() is None
scan(59, 2)
assert pop() == (1, 0) and pop() is None
scan(549, 2)
assert pop() is None
scan(559, 2)
assert pop() == (1, 1)
scan(659, 2)
assert pop() == (1, 1)
scan(759, 0)  # 释放候选立即暂停连发。
assert pop() is None
scan(779, 0)
assert call("Keys_IsPressed", 1) == 0
scan(789, 2)
scan(809, 2)
assert pop() == (1, 0)
scan(5000, 2)  # 错过很多周期也只补一条。
assert pop() == (1, 1) and pop() is None
assert call("Keys_IsPressed", 99) == 0
assert call("Keys_PopEvent", 0) == 0
print("PASS: GPIO polarity, debounce/bounce, press/repeat/release, no catch-up bursts")

init_keys(raw=2)
scan(1000, 2)
assert pop() is None  # 上电按住不自动调数值。
scan(1010, 0)
scan(1030, 0)
scan(1040, 2)
scan(1060, 2)
assert pop() == (1, 0)

init_keys(0xFFFFFFE0)
scan(0xFFFFFFF0, 1)
scan(0x100000004, 1)
assert pop() == (0, 0)
scan(0x1000001F8, 1)
assert pop() == (0, 1)

init_keys()
scan(10, 2)
scan(30, 2)
for t in range(530, 1431, 100):
    scan(t, 2)
assert call("Keys_GetDroppedCount") == 3  # 11条生产，容量8，丢新3。
assert pop() == (1, 0)
assert all(pop() == (1, 1) for _ in range(7))
assert pop() is None
print("PASS: held-at-boot suppression, timer wrap, queue capacity/drop accounting")

init_keys()
call("AppControl_Init")


def speed():
    return struct.unpack("<H", uc.mem_read(call("AppControl_GetState"), 2))[0]


time_ms = 0


def control_scan(t, raw):
    scan(t, raw)
    call("AppControl_Process", t & 0xFFFFFFFF, 1)


def tap(raw):
    global time_ms
    control_scan(time_ms + 10, raw)
    control_scan(time_ms + 30, raw)
    control_scan(time_ms + 40, 0)
    control_scan(time_ms + 60, 0)
    time_ms += 60


# 本测试的参数加减部分在完整开机/开始流程之后执行；帧完成信号在此由替身提供。
for phase in range(5):
    call("AppControl_Process", phase * 501, 1)
    call("AppControl_Process", phase * 501 + 500, 1)
time_ms = 2504
tap(4)  # START
control_scan(time_ms + 500, 0)
time_ms += 500

tap(1)
assert speed() == 0
tap(2)
assert speed() == 1
tap(1)
assert speed() == 0
tap(3)
assert speed() == 0  # 两键同时按下时不调数值。
control_scan(time_ms + 10, 2)
control_scan(time_ms + 30, 2)
assert speed() == 1
for t in range(time_ms + 530, time_ms + 31031, 100):
    control_scan(t, 2)
assert speed() == 300  # 长按到顶再继续，不能溢出。
control_scan(time_ms + 31100, 0)
control_scan(time_ms + 31120, 0)
time_ms += 31120
tap(1)
assert speed() == 299
state_pointer = call("AppControl_GetState")
assert uc.mem_read(state_pointer + unit_offset, 1) == b"\0"
assert call("AppControl_SetSpeedUnit", 1) == 1
assert uc.mem_read(state_pointer + unit_offset, 1) == b"\1" and speed() == 299
assert call("AppControl_SetSpeedUnit", 99) == 0
assert uc.mem_read(state_pointer + unit_offset, 1) == b"\1"
assert call("AppControl_SetSpeedUnit", 0) == 1
print("PASS: default KPH, runtime MPH/KPH selection, invalid-unit rejection, demo value retained")
print("PASS: actual key-to-AppControl events, 0.0/30.0 limits, simultaneous keys")

# 独立从已运行的Cypress段码表取期望，不引用新文件中的常量。
reference = Path(r"C:/Users/mingluw/Documents/cypress/260824/Q008_0926.cydsn/lcdfont.c")
source = reference.read_bytes().decode("gb18030", errors="replace")
source = re.sub(r"/\*.*?\*/|//[^\n]*", "", source, flags=re.S)


def table(name):
    body = re.search(r"\b" + name + r"\s*\[\]\[16\]\s*=\s*\{(.*?)\};",
                     source, re.S).group(1)
    return [bytes(int(x, 16) for x in re.findall(r"0x[0-9a-fA-F]+", row))
            for row in re.findall(r"\{(.*?)\}", body, re.S)]


digits = [table(f"Tube{i}_code") for i in range(1, 5)]
misc = table("misc_code")


def expected_frame(value, unit=0):
    unit_icon = bytearray(misc[20])
    # 旧逐段表：K=08、M=04、PH=02、速度图标=01。
    unit_icon[12] &= ~(0x04 if unit == 0 else 0x08)
    rows = [misc[6], unit_icon, digits[2][value // 10 % 10], digits[3][value % 10]]
    if value >= 100:
        rows.append(digits[1][value // 100 % 10])
    if value >= 1000:
        rows.append(digits[0][value // 1000])
    return bytes([reduce(int.__or__, col) for col in zip(*rows)])


for value in list(range(301)) + [999, 1000, 9999]:
    for unit in [0, 1]:
        assert call("LcdSegments_BuildSpeed", value, unit, scratch) == 1
        assert bytes(uc.mem_read(scratch, 16)) == expected_frame(value, unit)
assert expected_frame(0, 0)[12] & 0x0F == 0x0B
assert expected_frame(0, 1)[12] & 0x0F == 0x07
before = bytes(uc.mem_read(scratch, 16))
assert call("LcdSegments_BuildSpeed", 10000, 0, scratch) == 0
assert bytes(uc.mem_read(scratch, 16)) == before
assert call("LcdSegments_BuildSpeed", 0, 0, 0) == 0
assert call("LcdSegments_BuildSpeed", 0, 99, scratch) == 0
assert bytes(uc.mem_read(scratch, 16)) == before
assert call("Lcd_SetFrame", 0) == 0
print("PASS: numeric frames vs old tables, decimal point/icons/leading blanks, invalid inputs")

call("Lcd_Init", 0)
call("AppDisplay_Init", 0)


def display(t, value, unit=0):
    data = bytearray(state_size)
    for name, field_value in [("demo_speed_x10", value), ("speed_unit", unit), ("screen", 4)]:
        offset, size = state_fields[name]
        data[offset:offset + size] = field_value.to_bytes(size, "little")
    uc.mem_write(scratch, bytes(data))
    call("AppDisplay_Process", t & 0xFFFFFFFF, scratch)


def lcd_steps(start, count):
    for t in range(start, start + count):
        gpio_state["now"] = t
        call("Lcd_Process", t & 0xFFFFFFFF)


display(0, 0)
lcd_steps(0, 900)
words = gpio_state["words"]
assert [word for _, word in words[-20:]] == lcd_env["header"] + [0x100 | x for x in expected_frame(0)]
before = len(words)
display(1000, 1)
lcd_steps(1000, 20)
assert [word for _, word in words[before:]] == lcd_env["header"] + [0x100 | x for x in expected_frame(1)]
before = len(words)
display(1010, 2)
display(1020, 3)
lcd_steps(1020, 20)
assert len(words) == before  # 50ms内只保留状态，尚未提交。
display(1050, 3)
lcd_steps(1050, 20)
assert [word for _, word in words[before:]] == lcd_env["header"] + [0x100 | x for x in expected_frame(3)]
before = len(words)
display(1100, 3)
lcd_steps(1100, 20)
assert len(words) == before  # 不变不刷。
display(1150, 3, 1)  # 数字相同，仅单位变化也必须刷新。
lcd_steps(1150, 20)
assert [word for _, word in words[before:]] == lcd_env["header"] + [0x100 | x for x in expected_frame(3, 1)]
before = len(words)
display(1200, 3, 99)  # 无效应用单位不应提交或清除当前显示。
lcd_steps(1200, 20)
assert len(words) == before
print("PASS: unit-only change refreshes MPH, invalid AppState unit ignored")
call("AppDisplay_Init", 0xFFFFFFE0)
display(0xFFFFFFE0, 4)
lcd_steps(1200, 20)
before = len(words)
display(0x100000008, 5)  # 经过40ms
lcd_steps(1220, 20)
assert len(words) == before
display(0x100000012, 5)  # 经过50ms
lcd_steps(1240, 20)
assert [word for _, word in words[before:]] == lcd_env["header"] + [0x100 | x for x in expected_frame(5)]
print("PASS: AppDisplay initial frame, 50ms coalescing, unchanged suppression, timer wrap")

# 帧接口必须复制，不持有调用方的栈/临时缓冲。
uc.mem_write(scratch, expected_frame(6))
assert call("Lcd_SetFrame", scratch) == 1
uc.mem_write(scratch, b"\xFF" * 16)
before = len(words)
lcd_steps(1300, 20)
assert [word for _, word in words[before:]] == lcd_env["header"] + [0x100 | x for x in expected_frame(6)]
gpio_state["force_send_failure"] = True
display(1400, 7)
lcd_steps(1400, 1)
assert call("Lcd_HasError") == 1
assert call("Lcd_SetFrame", scratch) == 0
call("AppDisplay_Init", 1500)
display(1500, 8)  # 失败不能记作已提交，否则恢复后相同状态无法重试。
gpio_state["force_send_failure"] = False
call("Lcd_Init", 1500)
display(1501, 8)
lcd_steps(1500, 900)
assert [word for _, word in words[-20:]] == lcd_env["header"] + [0x100 | x for x in expected_frame(8)]
print("PASS: frame ownership/copy, failed display submission remains pending for retry")
