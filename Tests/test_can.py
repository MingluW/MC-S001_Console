"""真实ARM ELF的FP2、CAN SDK消息RAM、收发状态机和串口日志；不连接实板。"""
from pathlib import Path
import runpy
import struct

env = runpy.run_path(str(Path(__file__).with_name("test_uart_log.py")))
from unicorn import UC_HOOK_MEM_WRITE
from elftools.elf.elffile import ELFFile
from elftools.dwarf.dwarf_expr import DWARFExprParser

call, uc, symbols = env["call"], env["uc"], env["symbols"]
scratch = env["scratch"]
uc.mem_map(0x40020000, 0x10000)
can, rcc = 0x40005000, 0x40021000
requests = []
cancel_requests = 0
cancel_immediately = True
dlc_bytes = [0, 1, 2, 3, 4, 5, 6, 7, 8, 12, 16, 20, 24, 32, 48, 64]


def read32(address):
    return int.from_bytes(uc.mem_read(address, 4), "little")


def write32(address, value):
    uc.mem_write(address, struct.pack("<I", value))


with (env["root"] / "Makefile/build/MC-S001_Console.elf").open("rb") as stream:
    dwarf = ELFFile(stream).get_dwarf_info()
    def layout(name):
        typedef = next(d for unit in dwarf.iter_CUs() for d in unit.iter_DIEs()
                       if d.tag == "DW_TAG_typedef" and d.attributes.get("DW_AT_name") and
                       d.attributes["DW_AT_name"].value == name.encode())
        die = typedef.get_DIE_from_attribute("DW_AT_type")
        fields = {}
        for member in die.iter_children():
            if member.tag != "DW_TAG_member":
                continue
            location = member.attributes["DW_AT_data_member_location"].value
            offset = location if isinstance(location, int) else \
                DWARFExprParser(dwarf.structs).parse_expr(location)[0].args[0]
            fields[member.attributes["DW_AT_name"].value.decode()] = offset
        return die.attributes["DW_AT_byte_size"].value, fields
    frame_size, frame_fields = layout("BoardCanFrame")
    message_size, message_fields = layout("Fp2Message")
    app_size, app_fields = layout("AppState")
    stats_size, stats_fields = layout("BoardCanStats")


def memory_hook(machine, access, address, size, value, context):
    global cancel_requests
    if address == can + 0xD0:  # TXBAR：硬件接受新请求，清除该缓冲旧完成状态。
        write32(can + 0xCC, read32(can + 0xCC) | value)
        write32(can + 0xD8, 0)
        write32(can + 0xDC, 0)
        address = read32(rcc + 0x90) + (read32(can + 0xC0) & 0xFFFC)
        word0, word1 = struct.unpack("<II", uc.mem_read(address, 8))
        length = dlc_bytes[(word1 >> 16) & 15]
        requests.append((word0 >> 18 & 0x7FF, bytes(uc.mem_read(address + 8, length))))
    elif address == can + 0xD4:  # TXBCR
        cancel_requests += 1
        if cancel_immediately:
            write32(can + 0xCC, read32(can + 0xCC) & ~value)
            write32(can + 0xDC, value)
    elif address == can + 0xA8:  # RXF0A：本测试每次向FIFO注入一帧。
        write32(can + 0xA4, 0)


uc.hook_add(UC_HOOK_MEM_WRITE, memory_hook)


def reset_registers():
    uc.mem_write(can, bytes(0x200))
    # 与生成初始化对应：HSI8M/2*60=240MHz，SYSCLK=PLL，PLL输出不分频。
    write32(rcc + 4, 0x020000E4)
    write32(rcc + 0x40, 0)


def frame_pointer(payload, identifier=0x201):
    data = bytearray(frame_size)
    data[frame_fields["id"]:frame_fields["id"] + 4] = struct.pack("<I", identifier)
    data[frame_fields["length"]] = len(payload)
    data[frame_fields["is_fd"]] = 1
    data[frame_fields["has_brs"]] = 1
    offset = frame_fields["data"]
    data[offset:offset + len(payload)] = payload
    uc.mem_write(scratch, bytes(data))
    return scratch


def complete_tx(success=True):
    write32(can + 0xCC, 0)
    write32(can + 0xD8, int(success))


def inject(payload, identifier=0x241, malformed_prefix=None):
    if malformed_prefix is None:
        payload = bytes([len(payload)]) + payload
    else:
        payload = bytes([malformed_prefix]) + payload
    dlc = next(i for i, size in enumerate(dlc_bytes) if size >= len(payload))
    payload = payload.ljust(dlc_bytes[dlc], b"\0")
    address = read32(rcc + 0x90) + (read32(can + 0xA0) & 0xFFFC)
    uc.mem_write(address, struct.pack("<II", identifier << 18,
                                    dlc << 16 | 1 << 20 | 1 << 21) + payload)
    write32(can + 0xA4, 1)


reset_registers()
assert call("BoardCan_TrySend", frame_pointer(b"123456789012"), 0) == 3
write32(rcc + 4, 0)  # HSI直连不是本版240M时钟配置。
assert call("BoardCan_Init") == 0
reset_registers()
assert call("BoardCan_Init") == 1
# RX必须为输入并保留AF26路由；不能与收发器RXD争用。
assert (read32(0x4002C400) >> 6) & 3 == 0
assert (read32(0x4002C40C) >> 24) & 31 == 26
assert (read32(0x4002C404) >> 6) & 3 == 1
assert (read32(0x4002C000) >> 30) & 3 == 2
assert read32(rcc + 0x0C) & 0xF0C00000 == 0x60000000
nbtp, dbtp = read32(can + 0x1C), read32(can + 0x0C)
assert (nbtp >> 25) + 1 == 20
assert (nbtp >> 8 & 255) + 1 == 139 and (nbtp & 127) + 1 == 20
assert (dbtp & 15) + 1 == 4
assert (dbtp >> 8 & 31) + 1 == 14 and (dbtp >> 4 & 15) + 1 == 5
assert read32(can + 0x18) & 0x340 == 0x340  # FD/BRS和关闭自动重发。
assert 240000000 // 6 // (1 + 139 + 20) == 250000
assert 240000000 // 6 // (1 + 14 + 5) == 2000000
ram = read32(rcc + 0x90)
assert ram == symbols["can_message_ram"] and ram % 4 == 0
assert read32(can + 0xA0) >> 16 & 127 == 16
assert read32(can + 0xC0) >> 16 & 63 == 1
assert call("BoardCan_TrySend", 0, 0) == 4
assert call("BoardCan_TrySend", frame_pointer(bytes(9)), 0) == 4
payload = b"\x08\x12\x06\x2D\x01\x9A\x99\x99\x3E\0\0\0"
assert call("BoardCan_TrySend", frame_pointer(payload), 10) == 0
assert requests[-1] == (0x201, payload)
assert call("BoardCan_GetTxState") == 1
assert call("BoardCan_TrySend", frame_pointer(payload), 11) == 2
complete_tx()
call("BoardCan_Process", 12)
assert call("BoardCan_GetTxState") == 2
assert call("BoardCan_TrySend", frame_pointer(payload), 20) == 0
complete_tx(False)
call("BoardCan_Process", 21)
assert call("BoardCan_GetTxState") == 3
assert call("BoardCan_TrySend", frame_pointer(payload), 0xFFFFFFF0) == 0
cancel_immediately = False
call("BoardCan_Process", 0xB7)  # 跨回绕199ms
assert cancel_requests == 0
call("BoardCan_Process", 0xB8)  # 200ms
assert cancel_requests == 1 and call("BoardCan_GetTxState") == 1
assert call("BoardCan_TrySend", frame_pointer(payload), 0xB9) == 2
complete_tx(False)
call("BoardCan_Process", 0xB9)
assert call("BoardCan_GetTxState") == 3
cancel_immediately = True
inject(b"\x23\x01\x12")
assert call("BoardCan_Read", scratch) == 0
assert uc.mem_read(scratch + frame_fields["length"], 1) == b"\x04"
assert bytes(uc.mem_read(scratch + frame_fields["data"], 4)) == b"\x03\x23\x01\x12"
assert call("BoardCan_Read", scratch) == 1
write32(can + 0x44, 0x707)
write32(0x4002C42C, 8)
call("BoardCan_Process", 900)
call("BoardCan_GetStats", scratch)
assert read32(scratch + stats_fields["last_data_error"]) == 7
assert read32(scratch + stats_fields["activity"]) == 0
assert read32(scratch + stats_fields["controller_control"]) & 0x341 == 0x340
assert uc.mem_read(scratch + stats_fields["is_rx_high"], 1) == b"\1"
write32(can + 0x44, 0x80)  # PSR.BO
call("BoardCan_Process", 1000)
assert call("BoardCan_TrySend", frame_pointer(payload), 1001) == 3
print("PASS: real SDK init/RAM, 250k/2M timing, DLC, TX busy/success/failure, cancel + wrap, RX")

for speed in range(301):
    assert call("Fp2_BuildSpeed", speed, scratch) == 1
    assert bytes(uc.mem_read(scratch, 8)) == b"\x12\x06\x2D\x01" + struct.pack("<f", speed / 10)
assert call("Fp2_BuildSpeed", 301, scratch) == 0
assert call("Fp2_BuildSpeed", 3, 0) == 0
assert call("Fp2_BuildSubscription", scratch) == 1
assert bytes(uc.mem_read(scratch, 8)) == b"\x11\x06\x2E\x01\x3C\x08\x4C\x09"


def decode(data):
    uc.mem_write(scratch, data)
    return call("Fp2_Decode", scratch, len(data), scratch + 512)


assert decode(b"\x23\x01\x12") == 1
assert decode(b"\x24\x02\x12\x05") == 1
event = b"\x25\x06\x2E\x01" + struct.pack("<f", .3)
assert decode(event) == 1
for bad in [b"", b"\x23", b"\x23\x00", b"\x25\x06\x2E\x01\0\0\x80\x7F",
            b"\x21\x01\x01", b"\x2E\x00", b"\x24\x01\x12"]:
    assert decode(bad) == 0
print("PASS: all 0.0..30.0 FP2 float32 values, 0.3=9A99993E, subscriptions, malformed/Inf rejection")


def process(t, loops=1):
    for _ in range(loops):
        call("Powerboard_Process", t & 0xFFFFFFFF)
        call("BoardUart_Process")


def start_service():
    reset_registers()
    assert call("Powerboard_Init") == 1
    env["drain"]()
    requests.clear()


start_service()
process(0, 5)
assert not requests  # 未按开始，不发送任何目标/心跳。
assert call("Powerboard_RequestSpeed", 3) == 0
assert call("Powerboard_RequestStart", 3) == 1
process(1)
assert requests == [(0x201, payload)]
complete_tx()
inject(b"\x23\x01\x12")
process(2, 6)
assert len(requests) == 2 and requests[-1][1][1] == 0x11  # ACK后订阅。
complete_tx()
inject(b"\x23\x01\x11")
process(3, 6)
inject(event)
process(4, 4)
log = env["drain"]()
# 日志可能在process期间已发出，用wire全量检查见下面单独的日志测试。
assert call("Powerboard_RequestSpeed", 4) == 1
assert call("Powerboard_RequestSpeed", 5) == 1
process(5)
assert requests[-1][1][5:9] == struct.pack("<f", .5)  # 尚未提交目标合并为最新。
assert call("Powerboard_RequestStop") == 1
process(6, 3)
assert requests[-1][1][1] == 0x12 and requests[-1][1][5:9] == bytes(4)
assert call("Powerboard_RequestSpeed", 6) == 0
complete_tx()
inject(b"\x23\x01\x12")
process(7, 6)
count = len(requests)
process(10000, 8)
assert len(requests) == count  # 停止应答后没有周期目标/心跳。
print("PASS: no boot control/heartbeat, start0.3, ACK, subscription/event, coalescing, STOP preemption")

start_service()
assert call("Powerboard_RequestStart", 3) == 1
process(0)
complete_tx(False)
process(1, 4)
process(1000, 4)
assert len(requests) == 1  # 总线发送失败后不能继续提交订阅。
assert call("Powerboard_RequestSpeed", 4) == 0
print("PASS: RX input mode/AF route, normalized DLEC=7, RX/ACT/CCCR diagnostics, no subscribe after TX fail")

start_service()
assert call("Powerboard_RequestStart", 3) == 1
process(0)
complete_tx()
process(1)
count = len(requests)
process(500)
assert call("Powerboard_RequestSpeed", 4) == 1
process(501)
assert call("Powerboard_RequestSpeed", 5) == 0  # 应用应答超时锁存，不自动重发非零目标。
process(502, 5)
assert not any(frame[1][1] == 0x12 for frame in requests[count:])
assert call("Powerboard_RequestStop") == 1
process(503)
assert requests[-1][1][5:9] == bytes(4)
complete_tx(False)
process(504)
count = len(requests)
process(1003)
assert len(requests) == count
process(1004)
assert len(requests) == count + 1 and requests[-1][1][5:9] == bytes(4)
print("PASS: 500ms application timeout, nonzero target not retried, failed STOP retained/retried")

start_service()
env["wire"].clear()
assert call("Powerboard_RequestStart", 3) == 1
process(0, 4)
complete_tx()
inject(b"\x23\x01\x12")
process(1, 4)
complete_tx()
inject(b"\x23\x01\x11")
process(2, 4)
inject(event[:3])
process(3, 3)
inject(event[3:])
process(4, 4)
inject(b"\x23\x01\x12", malformed_prefix=60)
process(5, 3)
# 读取TRACE长帧和负载填充，接收未知标准ID也保留原始帧。
inject(bytes(range(63)), identifier=0x001)
process(6, 6)
for _ in range(40):
    call("BoardUart_Process")
text = bytes(env["wire"])
assert b"[CAN TX_REQ]" in text and b"[CAN TX_SENT]" in text and b"[CAN RX]" in text
assert b"9A 99 99 3E" in text and b"[FP2 ACK]" in text and b"current speed=0.3 KPH" in text
assert b"bad length prefix" in text and b"len=64 @32" in text
print("PASS: actual UART output raw TX/RX, TX completion, ACK, fragmented event, bad prefix, full64 bytes")

start_service()
assert call("Powerboard_RequestStart", 3) == 1
process(0)
complete_tx()
inject(b"\x23\x01\x12", identifier=0x242)  # 其他节点不能确认本请求。
process(1)
assert len(requests) == 1
inject(b"\x13\x01\x12")  # 上控sender不能冒充下控响应。
process(2)
assert len(requests) == 1
inject(b"\x24\x02\x12\x05")  # 拒绝写入后锁存运行请求，不能宣称已执行。
process(3)
assert call("Powerboard_RequestSpeed", 4) == 0
count = len(requests)
inject(event[:3])
process(10)
process(210)  # 半包超时丢弃；后续完整帧应仍可解析。
env["drain"]()
env["wire"].clear()
inject(event)
process(211, 3)
for _ in range(35):
    call("BoardUart_Process")
assert b"current speed=0.3 KPH" in bytes(env["wire"])
print("PASS: wrong CAN ID/sender ignored, Error locks run target, fragment timeout/resynchronization")

reset_registers()
assert call("AppPowerboard_Init") is not None
requests.clear()
def app_state(screen, speed):
    data = bytearray(app_size)
    data[app_fields["screen"]] = screen
    data[app_fields["demo_speed_x10"]:app_fields["demo_speed_x10"] + 2] = struct.pack("<H", speed)
    uc.mem_write(scratch + 1024, bytes(data))
    return scratch + 1024

call("AppPowerboard_Process", 0, app_state(0, 0))
assert not requests
call("AppPowerboard_Process", 1, app_state(3, 3))
assert requests[-1] == (0x201, payload)
complete_tx()
inject(b"\x23\x01\x12")
call("AppPowerboard_Process", 2, app_state(4, 3))
complete_tx()
inject(b"\x23\x01\x11")
call("AppPowerboard_Process", 3, app_state(4, 3))
call("AppPowerboard_Process", 4, app_state(4, 4))
assert requests[-1][1][5:9] == struct.pack("<f", .4)
call("AppPowerboard_Process", 5, app_state(6, 4))
call("AppPowerboard_Process", 6, app_state(6, 4))
assert requests[-1][1][5:9] == bytes(4)
print("PASS: App screen START->CAN0.3, speed change->CAN0.4, STOP->zero, boot remains silent")

# 原始帧日志在串口堵塞时不阻塞CAN。队列满丢新可见，恢复后完整输出。
env["hook"].__globals__["hardware_ready"] = False
env["drain"]()
call("CanTrace_Init")
assert call("CanTrace_Push", 99, 0, frame_pointer(bytes(12), 1)) == 0
for i in range(16):
    assert call("CanTrace_Push", 2, i, frame_pointer(bytes(64), 1)) == 1
assert call("CanTrace_Push", 2, 17, frame_pointer(bytes(64), 1)) == 0
for t in range(50):
    call("CanTrace_Process", t)
env["hook"].__globals__["hardware_ready"] = True
env["drain"]()
env["wire"].clear()
for t in range(1000, 1100):
    call("CanTrace_Process", t)
    call("BoardUart_Process")
for _ in range(35):
    call("BoardUart_Process")
assert b"queue overflow, dropped=1" in bytes(env["wire"])
print("PASS: trace queue full is counted, stalled UART is nonblocking, log resumes with overflow report")
