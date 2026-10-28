"""开机/等待/开始/切页/停止流程：真实ARM控制、显示和LCD，替代GPIO输入。"""
from pathlib import Path
import runpy

test = runpy.run_path(str(Path(__file__).with_name("test_keys_display.py")))
call, uc, scratch = test["call"], test["uc"], test["scratch"]
fields = test["state_fields"]
gpio = test["gpio_state"]
inputs = test["scan"].__globals__
digits, misc = test["digits"], test["misc"]
header = test["lcd_env"]["header"]
FULL, COUNT, WAIT, STARTING, SPEED, INCLINE, STOPPED = range(7)
now = 0
raw = 0


def state():
    address = call("AppControl_GetState")
    return {name: int.from_bytes(uc.mem_read(address + offset, size), "little")
            for name, (offset, size) in fields.items()}


def tick(t=None, keys=None):
    global now, raw
    if t is not None:
        now = t
    if keys is not None:
        raw = keys
    inputs["raw_keys"] = raw
    gpio["now"] = now
    call("Keys_Process", now & 0xFFFFFFFF)
    complete = call("Lcd_IsReady") and not call("Lcd_IsBusy")
    call("AppControl_Process", now & 0xFFFFFFFF, int(complete))
    call("AppDisplay_Process", now & 0xFFFFFFFF, call("AppControl_GetState"))
    before = len(gpio["words"])
    call("Lcd_Process", now & 0xFFFFFFFF)
    assert len(gpio["words"]) - before <= 1


def settle(t=None, keys=None):
    tick(t, keys)
    for _ in range(45):
        tick()
    assert call("Lcd_IsReady") and not call("Lcd_IsBusy")


def frame():
    words = [word for _, word in gpio["words"][-20:]]
    assert words[:4] == header
    assert all(0x100 <= word <= 0x1FF for word in words[4:])
    return bytes(word & 255 for word in words[4:])


def merge(rows):
    result = bytearray(16)
    for row in rows:
        for i, value in enumerate(row):
            result[i] |= value
    return bytes(result)


def repeated(value):
    return merge([digit[value] for digit in digits])


def incline(value):
    rows = [misc[9], digits[3][value % 10]]
    if value >= 10:
        rows.append(digits[2][value // 10])
    return merge(rows)


def press(mask):
    tick(now + 10, mask)
    tick(now + 20, mask)  # 20ms稳定，确认事件


def release():
    tick(now + 10, 0)
    tick(now + 20, 0)


def tap(mask):
    press(mask)
    settle()
    release()
    settle()


def init(t=0):
    global now, raw
    now, raw = t, 0
    gpio["force_send_failure"] = False
    test["init_keys"](t & 0xFFFFFFFF)
    call("Lcd_Init", t & 0xFFFFFFFF)
    call("AppControl_Init")
    call("AppDisplay_Init", t & 0xFFFFFFFF)
    tick()


init()
# LCD尚未初始化时，即使时间已经过去，开机状态也不能先走到倒计时。
tick(100)
tick(300)
tick(500)
assert state()["screen"] == FULL
for t in range(501, 701):
    tick(t)
assert frame() == b"\xFF" * 16 and state()["screen"] == FULL
# 强制以明确的帧完成时间重启控制状态，测试500ms边界。
call("AppControl_Init")
tick(1000)
tick(1499)
assert state()["screen"] == FULL
settle(1500)
assert state()["screen"] == COUNT and frame() == repeated(3)
for value in [2, 1, 0]:
    settle(now + 500)
    assert state()["countdown_digit"] == value and frame() == repeated(value)
settle(now + 500)
assert state()["screen"] == WAIT and frame() == repeated(8)
wait_start = now
tap(2)  # 等待阶段加速无效。
assert state()["screen"] == WAIT and state()["demo_speed_x10"] == 0
settle(wait_start + 999)
assert frame() == repeated(8)
settle(wait_start + 1000)
assert frame() == bytes(16)
settle(now + 1000)
assert frame() == repeated(8)
print("PASS: frame-complete boot timing, 3333/2222/1111/0000, 8888 1s blink, wait ignores +/-")

press(4)
assert state()["screen"] == STARTING and state()["demo_incline"] == 0
settle()
start_frame_ms = now
assert frame() == incline(0)
settle(start_frame_ms + 499)
assert state()["screen"] == STARTING
settle(start_frame_ms + 500)
assert state()["screen"] == SPEED
assert frame() == test["expected_frame"](0)
# 按住START不产生连发，也不再次启动。
settle(now + 1000, 4)
assert state()["screen"] == SPEED
release()
settle()
tap(32)  # 坡度加
assert state()["screen"] == INCLINE and state()["demo_incline"] == 1
assert frame() == incline(1)
settle(now + 3000)
assert state()["screen"] == INCLINE  # 手动坡度页不自动切回。
tap(2)
assert state()["screen"] == SPEED and state()["demo_speed_x10"] == 1
assert frame() == test["expected_frame"](1)
tap(16)
assert state()["screen"] == INCLINE and state()["demo_incline"] == 0
tap(16)  # 下限仍可以进入并保持坡度页。
assert state()["screen"] == INCLINE and state()["demo_incline"] == 0
press(32)
settle()
for _ in range(18):
    settle(now + 500, 32)
assert state()["demo_incline"] == 15
release()
settle()
print("PASS: START grade=0 for 500ms, speed default KPH, manual page retention, incline limits")

# 停止优先于START，同时按下不会马上被START覆盖；STOP期间加减无效。
press(4 | 8)
assert state()["screen"] == STOPPED
settle()
stopped = merge([digit[16] for digit in digits])
assert frame() == stopped
release()
settle()
tap(2)
assert state()["screen"] == STOPPED and frame() == stopped
press(4)
settle()
assert state()["screen"] == STARTING and frame() == incline(0)
release()
settle()
settle(now + 500)
assert state()["screen"] == SPEED and state()["demo_speed_x10"] == 0
# 刚进入速度页不到50ms，STOP也必须立即提交，不被刷新限速推迟。
press(8)
assert state()["screen"] == STOPPED
settle()
assert frame() == stopped
release()
settle()
press(4)
settle()
assert state()["screen"] == STARTING
release()
press(8)  # 在500ms启动停留中停止，不能等计时结束。
assert state()["screen"] == STOPPED
settle()
assert frame() == stopped
print("PASS: STOP priority, ---- immediate submission, stopped ignores +/- and START restarts flow")

# 普通队列已满时仍须得到STOP；START/STOP都不长按连发。
test["init_keys"](0)
test["scan"](10, 2)
test["scan"](30, 2)
for t in range(530, 1431, 100):
    test["scan"](t, 2)
assert call("Keys_GetDroppedCount") == 3
test["scan"](1440, 2 | 8)
test["scan"](1460, 2 | 8)
assert test["pop"]() == (3, 0)
assert test["pop"]() is None
test["init_keys"](0)
test["scan"](10, 4)
test["scan"](30, 4)
assert test["pop"]() == (2, 0)
test["scan"](2030, 4)
assert test["pop"]() is None
print("PASS: STOP bypasses full queue, discards stale events, START is press-only")

# 开机时段与启动停留跨越32位回绕，仍按时间差推进。
test["init_keys"](0xFFFFFF00)
call("AppControl_Init")
call("AppControl_Process", 0xFFFFFF00, 1)
call("AppControl_Process", 0xF3, 1)
assert state()["screen"] == FULL
call("AppControl_Process", 0xF4, 1)
assert state()["screen"] == COUNT and state()["countdown_digit"] == 3
# 动画中STOP也可立即打断。
test["scan"](0x104, 8)
test["scan"](0x118, 8)
call("AppControl_Process", 0x118, 1)
assert state()["screen"] == STOPPED
print("PASS: screen timer uint32 wrap, STOP interrupts boot animation")
