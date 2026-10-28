"""执行实际ARM ELF的UART/printf/log逻辑，不连接实板。

依赖：unicorn、pyelftools，可用 --deps 指定临时安装目录。
UART硬件发送和时间由测试替身提供；不能代替引脚/波特率实测。
"""
import argparse
from pathlib import Path
import sys
import tempfile
import struct

parser = argparse.ArgumentParser()
parser.add_argument("--deps", default=str(Path(tempfile.gettempdir()) / "mcs001-testdeps"))
args = parser.parse_args()
sys.path.insert(0, args.deps)

from elftools.elf.elffile import ELFFile
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_MODE_MCLASS, UC_HOOK_CODE
from unicorn.arm_const import (
    UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3,
    UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_PC,
)

root = Path(__file__).resolve().parents[1]
uc = Uc(UC_ARCH_ARM, UC_MODE_THUMB | UC_MODE_MCLASS)
uc.mem_map(0x08000000, 0x40000)
uc.mem_map(0x20000000, 0x24000)
uc.mem_map(0x40000000, 0x10000)
with (root / "Makefile/build/MC-S001_Console.elf").open("rb") as stream:
    elf = ELFFile(stream)
    symbols = {s.name: s["st_value"] for s in elf.get_section_by_name(".symtab").iter_symbols()}
    for segment in elf.iter_segments():
        if segment["p_type"] == "PT_LOAD" and segment["p_filesz"]:
            uc.mem_write(segment["p_vaddr"], segment.data())

sentinel = 0x0803F000
scratch = 0x20020000
wire = bytearray()
now_ms = 1234
hardware_ready = True


def return_value(value):
    uc.reg_write(UC_ARM_REG_R0, value)
    uc.reg_write(UC_ARM_REG_PC, uc.reg_read(UC_ARM_REG_LR))


def hook(machine, address, size, context):
    if address == sentinel:
        machine.emu_stop()
    elif address == (symbols["BoardTime_Millis"] & ~1):
        return_value(now_ms)
    elif address == (symbols["USART_GetFlagStatus"] & ~1):
        return_value(int(hardware_ready))
    elif address == (symbols["USART_SendData"] & ~1):
        wire.append(machine.reg_read(UC_ARM_REG_R1) & 0xFF)
        return_value(0)


uc.hook_add(UC_HOOK_CODE, hook)


def call(name, *values):
    for reg, value in zip([UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3], values):
        uc.reg_write(reg, value)
    uc.reg_write(UC_ARM_REG_SP, 0x20024000)
    uc.reg_write(UC_ARM_REG_LR, sentinel | 1)
    uc.emu_start(symbols[name] | 1, sentinel + 2, count=1000000)
    assert uc.reg_read(UC_ARM_REG_PC) == sentinel, f"{name} did not return"
    return uc.reg_read(UC_ARM_REG_R0)


def put(data):
    uc.mem_write(scratch, data + b"\0")
    return scratch


def drain():
    wire.clear()
    for _ in range(34):
        call("BoardUart_Process")
    return bytes(wire)


# 仅将初始化状态置好，硬件初始化需实板验证；其余使用实际驱动/库代码。
assert call("BoardUart_TryWrite", put(b"x"), 1) == 1  # NOT_READY
uc.mem_write(symbols["is_ready"], b"\1")
assert call("BoardUart_TryWrite", 0, 1) == 2  # INVALID
assert call("BoardUart_TryWrite", put(b"A" * 1023), 1023) == 0
assert call("BoardUart_TryWrite", put(b"B"), 1) == 3  # FULL，不能部分写入
hardware_ready = False
assert drain() == b""  # 硬件忙时立即返回
hardware_ready = True
assert drain() == b"A" * 1023
for i in range(20):  # 多次跨过环形缓冲边界
    payload = bytes([65 + i]) * 87
    assert call("BoardUart_TryWrite", put(payload), len(payload)) == 0
    assert drain() == payload
print("PASS: TX not-ready/invalid/full, hardware-busy, ring wraparound")

call("BoardStdio_Init")
assert call("printf", put(b"printf value=%lu\r\n"), 42) > 0
assert drain() == b"printf value=42\r\n"
print("PASS: actual newlib printf -> _write -> UART queue")

for level, label in [(1, b"ERR"), (2, b"WARN"), (3, b"DBG")]:
    assert call("Log_Write", level, put(b"number=%lu\n"), 42) == 1
    assert drain() == b"[0d 00:00:01.234][" + label + b"] number=42\r\n"
call("Log_SetLevel", 2)
assert call("Log_Write", 3, put(b"hidden")) == 1
assert drain() == b""
call("Log_SetLevel", 0)
assert call("Log_Write", 1, put(b"hidden error")) == 1
assert drain() == b""
call("Log_SetLevel", 3)
assert call("Log_Write", 1, put(b"x" * 192)) == 0
assert drain() == b""
assert call("Log_Write", 1, 0) == 0
assert call("BoardUart_TryWrite", put(b"Q" * 1023), 1023) == 0
assert call("Log_Write", 1, put(b"must not be partially queued")) == 0
assert drain() == b"Q" * 1023
print("PASS: log levels, filtering, CRLF, oversized/null/full rejection")

# 时间持续前进，覆盖进位、毫秒计数回绕，以及关闭输出期间仍更新计时。
for total_ms in [59999, 60000, 3599999, 3600000, 86399999, 86400000,
                 0xFFFFFFFF, 0x100000000, 0x1000004D2]:
    now_ms = total_ms & 0xFFFFFFFF
    call("Log_SetLevel", 0)
    call("Log_Process", now_ms)
    call("Log_SetLevel", 3)
    assert call("Log_Write", 3, put(b"time test")) == 1
    days, rem = divmod(total_ms, 86400000)
    hours, rem = divmod(rem, 3600000)
    minutes, rem = divmod(rem, 60000)
    seconds, milliseconds = divmod(rem, 1000)
    expected = f"[{days}d {hours:02}:{minutes:02}:{seconds:02}.{milliseconds:03}][DBG] time test\r\n"
    assert drain() == expected.encode()
print("PASS: timestamp minute/hour/day carry, uint32 wrap, logging-off continuity")


def receive(byte, status=0x200):
    # USART2 STS/DAT模拟；不模拟物理串口或NVIC，仅验证ISR缓冲处理。
    uc.mem_write(0x40000C0C, struct.pack('<II', status, byte))
    call("BoardUart_IrqHandler")


if "BoardUart_ReadByte" not in symbols:
    print("SKIP: RX read API was removed by linker (echo task disabled)")
else:
    assert call("BoardUart_ReadByte", scratch) == 4  # EMPTY
    assert call("BoardUart_ReadByte", 0) == 2  # INVALID
    for i in range(127):
        receive(i)
    receive(255)  # 缓冲满，丢新数据并上报错误
    assert call("BoardUart_ReadByte", scratch) == 5
    for i in range(127):
        assert call("BoardUart_ReadByte", scratch) == 0
        assert uc.mem_read(scratch, 1) == bytes([i])
    assert call("BoardUart_ReadByte", scratch) == 4
    for i in range(300):
        receive(i & 255)
        assert call("BoardUart_ReadByte", scratch) == 0
        assert uc.mem_read(scratch, 1) == bytes([i & 255])
    receive(0x55, 0x2200)  # RXDNE+OREF：不能把错误字节交给应用
    assert call("BoardUart_ReadByte", scratch) == 5
    assert call("BoardUart_ReadByte", scratch) == 4
    print("PASS: RX empty/invalid, overflow, ring wraparound, damaged-byte rejection")

heap_current = call("_sbrk", 0)
heap_end = symbols["__heap_end__"]
assert call("_sbrk", heap_end - heap_current + 1) == 0xFFFFFFFF
assert call("_sbrk", 0) == heap_current
print("PASS: bounded newlib heap rejects out-of-range allocation")
