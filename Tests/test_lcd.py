"""复用UART测试的ARM模拟环境，执行真实LCD状态机和GPIO逐位发送代码。"""
from pathlib import Path
import re
import runpy

env = runpy.run_path(str(Path(__file__).with_name("test_uart_log.py")))
uc = env["uc"]
symbols = env["symbols"]
call = env["call"]
return_value = env["return_value"]
R0 = env["UC_ARM_REG_R0"]
R1 = env["UC_ARM_REG_R1"]

gpio_a, gpio_b, gpio_c = 0x4002C000, 0x4002C400, 0x4002C800
cs_pin, clock_pin, data_pin = 1 << 4, 1 << 5, 1 << 7
levels = {}
bits = []
words = []
reset_edges = []
now = 0
force_send_failure = False


def gpio_hook(machine, address, size, context):
    if address == (symbols["BoardLcdPort_Write9"] & ~1) and force_send_failure:
        return_value(0)
        return
    if address in [symbols[x] & ~1 for x in ["RCC_EnableAHB1PeriphClk", "GPIO_InitPeripheral"]]:
        return_value(0)
        return
    is_set = address == (symbols["GPIO_SetBits"] & ~1)
    if not is_set and address != (symbols["GPIO_ResetBits"] & ~1):
        return
    port, mask = machine.reg_read(R0), machine.reg_read(R1)
    before = levels.get(port, 0)
    after = (before | mask) if is_set else (before & ~mask)
    levels[port] = after
    if port == gpio_c and ((before ^ after) & (1 << 15)):
        reset_edges.append((now, bool(after & (1 << 15))))
    if port == gpio_a:
        if (before & cs_pin) and not (after & cs_pin):
            bits.clear()
        if not (after & cs_pin) and not (before & clock_pin) and (after & clock_pin):
            bits.append(int(bool(after & data_pin)))
        if not (before & cs_pin) and (after & cs_pin):
            if bits:
                assert len(bits) == 9, f"expected 9 clocks, got {len(bits)}"
                value = 0
                for bit in bits:
                    value = (value << 1) | bit
                words.append((now, value))
            assert after & clock_pin, "clock must return to idle-high"
    return_value(0)


def set_all(is_on):
    # 通过生产版本使用的完整帧接口验证全亮/全灭；旧SetAll可能被链接器裁剪。
    uc.mem_write(env["scratch"], bytes([255 if is_on else 0]) * 16)
    assert call("Lcd_SetFrame", env["scratch"]) == 1


uc.hook_add(env["UC_HOOK_CODE"], gpio_hook)
assert call("BoardLcdPort_Write9", 0x1FF) == 0  # 尚未初始化
call("Lcd_Init", 0)
set_all(True)
assert levels[gpio_a] & (cs_pin | clock_pin) == cs_pin | clock_pin
assert levels[gpio_b] & (1 << 5)  # 背光高有效
for now in [0, 99]:
    call("Lcd_Process", now)
assert not words and reset_edges == [(0, True)]
now = 100
call("Lcd_Process", now)
assert reset_edges[-1] == (100, False)
now = 299
call("Lcd_Process", now)
assert reset_edges[-1] == (100, False)
now = 300
call("Lcd_Process", now)
assert reset_edges[-1] == (300, True)
now = 499
call("Lcd_Process", now)
assert not words

for now in range(500, 900):
    before = len(words)
    call("Lcd_Process", now)
    assert len(words) - before <= 1, "one invocation must send at most one word"

# 独立从旧Cypress源码读取序列，避免只复制新实现的期望值。
reference = Path(r"C:/Users/mingluw/Documents/cypress/260824/Q008_0926.cydsn/LCD.c")
source = reference.read_bytes().decode("gb18030", errors="replace")
source = re.sub(r"/\*.*?\*/|//[^\n]*", "", source, flags=re.S)
source = source[source.index("void lcd_initial("):source.index("void show_dot(")]
expected = []
delays = {}
for kind, value, delay in re.findall(
        r"write_(command|data)\s*\(\s*(0x[0-9A-Fa-f]+|Contrast)\s*\)|CyDelay\((\d+)\)", source):
    if delay:
        if expected:
            delays[len(expected) - 1] = int(delay)
    else:
        expected.append((0x100 if kind == "data" else 0) |
                        (0x19 if value == "Contrast" else int(value, 16)))
header = [0x2A, 0x100, 0x103, 0x2C]
assert [word for _, word in words] == expected + header + [0x1FF] * 16
for index, delay in delays.items():
    assert words[index + 1][0] - words[index][0] >= delay
assert call("Lcd_IsReady") == 1 and call("Lcd_IsBusy") == 0
print("PASS: old Cypress command/data sequence, reset/20ms delays, mode3/MSB/9 clocks, full-ON")

before = len(words)
set_all(True)
call("Lcd_Process", 1000)
assert len(words) == before  # 相同内容不重复刷新
set_all(False)
for now in range(1001, 1021):
    call("Lcd_Process", now)
assert [word for _, word in words[before:]] == header + [0x100] * 16
assert call("Lcd_IsBusy") == 0

before = len(words)
set_all(True)
for now in range(1100, 1107):
    call("Lcd_Process", now)
set_all(False)  # 传输中改目标，不得破坏本次快照
for now in range(1107, 1140):
    call("Lcd_Process", now)
assert [word for _, word in words[before:]] == header + [0x1FF] * 16 + header + [0x100] * 16
print("PASS: full-OFF, unchanged-frame suppression, mid-transfer snapshot")

assert call("BoardLcdPort_Write9", 0x200) == 0
set_all(True)
force_send_failure = True
call("Lcd_Process", 2000)
assert call("Lcd_HasError") == 1 and call("Lcd_IsBusy") == 0
assert not (levels[gpio_b] & (1 << 5))
print("PASS: invalid 9-bit value and local failure/backlight-off handling")
