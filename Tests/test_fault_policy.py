"""Actual Console ARM ELF: explicit communication retry and first-fault stop/latch.
Runs CAN regressions first. No board/flash access.
"""
from pathlib import Path
import runpy
import struct

e = runpy.run_path(str(Path(__file__).with_name('test_can.py')))
call, uc, symbols = e['call'], e['uc'], e['symbols']
requests, complete, inject = e['requests'], e['complete_tx'], e['inject']
from unicorn import UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_R0


def setup():
    e['reset_registers']()
    call('AppControl_Init')
    call('AppPowerboard_Init')
    requests.clear()


def step(t):
    call('AppPowerboard_Process', t, call('AppControl_GetState'))
    call('BoardUart_Process')


def field(name, size=1):
    return int.from_bytes(uc.mem_read(call('AppControl_GetState') + e['app_fields'][name], size), 'little')


def event(code):
    return b'\x25\x06\x3c\x08' + struct.pack('<f', code)


keys = []
def key_hook(machine, address, size, context):
    if address == (symbols['Keys_PopEvent'] & ~1):
        if keys:
            machine.mem_write(machine.reg_read(UC_ARM_REG_R0), struct.pack('<II', keys.pop(0), 0))
            e['env']['return_value'](1)
        else:
            e['env']['return_value'](0)
    elif address == (symbols['Keys_IsPressed'] & ~1):
        e['env']['return_value'](0)
uc.hook_add(UC_HOOK_CODE, key_hook)


def key(id, t):
    keys.append(id)
    call('AppControl_Process', t, 1)
    step(t)


def start_ui():
    # Advance real animation with completed-frame signals to WAIT.
    for t in [0, 500, 501, 1001, 1002, 1502, 1503, 2003, 2004, 2504]:
        call('AppControl_Process', t, 1)
    assert field('screen') == 2
    key(2, 2505)
    complete(); inject(b'\x23\x01\x12'); step(2506)
    complete(); inject(b'\x23\x01\x11'); step(2507)
    call('AppControl_Process', 2508, 1)
    call('AppControl_Process', 3508, 1)
    assert field('screen') == 4


# Actual speed key -> failed hardware TX -> explicit key retry -> ACK -> resubscribe.
setup(); start_ui()
key(1, 3509)
assert requests[-1][1][5:9] == struct.pack('<f', .4)
complete(False); step(3510)
count = len(requests)
step(5000)
assert len(requests) == count and field('error_code') == 1
key(1, 5001)
assert len(requests) == count + 1 and requests[-1][1][5:9] == struct.pack('<f', .5)
complete(); inject(b'\x23\x01\x12'); step(5002)
assert field('error_code') == 0 and requests[-1][1][1] == 0x11
complete(); inject(b'\x23\x01\x11'); step(5003)
# Business timeout also accepts next speed key, no automatic failed-target resend.
key(0, 5004)
complete(); step(5005); step(5505)
count = len(requests)
step(7000)
assert len(requests) == count and field('error_code') == 1
key(0, 7001)
assert requests[-1][1][5:9] == struct.pack('<f', .3)
complete(); inject(b'\x23\x01\x12'); step(7002)
assert field('error_code') == 0
print('PASS: real UI speed +/- retries TX failure and FP2 timeout; ACK clears E-01 and restores subscription')

# Fault arrives while target is in hardware, newer nonzero target is queued.
setup(); start_ui()
key(1, 3509)
assert call('Powerboard_RequestSpeed', 10) == 1
# Hardware cancellation deliberately not acknowledged yet.
e['memory_hook'].__globals__['cancel_immediately'] = False
inject(event(3) + event(6))  # Two complete messages in the same iCANt payload.
step(3510)
assert call('Powerboard_GetControllerError') == field('error_code') == 3
assert field('demo_speed_x10', 2) == 0 and field('screen') == 6
count = len(requests)
for t in [3511, 3600, 4000]:
    step(t)
assert len(requests) == count  # Cannot reuse pending hardware buffer.
assert call('Powerboard_RequestStart', 3) == call('Powerboard_RequestSpeed', 10) == 0
# Once cancellation is confirmed, only zero may follow, never the queued 1.0 KPH.
complete(False); step(4001)
assert len(requests) == count + 1 and requests[-1][1][5:9] == bytes(4)
complete(False); step(4002)
count = len(requests)
step(4501)
assert len(requests) == count
step(4502)
assert len(requests) == count + 1 and requests[-1][1][5:9] == bytes(4)
complete(); inject(b'\x23\x01\x12'); step(4503)
assert field('error_code') == 3
# User keys and later errors/zero/communications cannot replace first cause.
count = len(requests)
for id in [2, 1, 0, 5, 4, 3, 2, 1]:
    key(id, 5000 + id)
assert field('error_code') == 3 and field('demo_speed_x10', 2) == 0
for code in [6, 0, 15, 2.5, float('nan')]:
    inject(event(code)); step(5100)
    assert field('error_code') == call('Powerboard_GetControllerError') == 3
for _, data in requests[count:]:
    if data[1] == 0x12:
        assert data[5:9] == bytes(4)
# Module and app must both protect the first cause.
call('AppControl_SetErrors', 1, 12)
assert field('error_code') == 3
# Only reinitialization clears latch and allows next normal session.
e['memory_hook'].__globals__['cancel_immediately'] = True
setup(); start_ui()
assert field('error_code') == 0
print('PASS: first fault wins within one RX batch; queued speed discarded; pending TX cancel/STOP/retry nonblocking')
print('PASS: normal fault locks START/+/- and survives later faults/zero/communication recovery until reset')


# Communication-error speed preview: actual rendered frames and timing boundaries.
frames = []
def lcd_hook(machine, address, size, context):
    if address == (symbols['Lcd_SetFrame'] & ~1):
        frames.append(bytes(machine.mem_read(machine.reg_read(UC_ARM_REG_R0), 16)))
        e['env']['return_value'](1)
uc.hook_add(UC_HOOK_CODE, lcd_hook)

def render(t):
    call('AppDisplay_Process', t, call('AppControl_GetState'))

def expect_frame(builder, *args):
    ptr = e['scratch'] + 2048
    assert call(builder, *args, ptr) == 1
    assert frames[-1] == bytes(uc.mem_read(ptr, 16))

setup(); start_ui(); call('AppDisplay_Init', 0)
key(1, 3509); complete(False); step(3510)
render(3510); expect_frame('LcdSegments_BuildError', 1)
key(1, 3511); render(3511)
assert field('error_code') == 1 and field('is_speed_preview') == 1
expect_frame('LcdSegments_BuildSpeed', 5, 0)
# LCD busy: don't consume the preview duration before its frame completes.
call('AppControl_Process', 5000, 0)
assert field('is_speed_preview') == 1
call('AppControl_Process', 5001, 1)
call('AppControl_Process', 5500, 1); render(5500)
assert field('is_speed_preview') == 1
call('AppControl_Process', 5501, 1); render(5501)
assert field('is_speed_preview') == 0 and field('error_code') == 1
expect_frame('LcdSegments_BuildError', 1)
# Repeat/next key refreshes 500ms, even when the timer wraps uint32.
keys.append(0); call('AppControl_Process', 0xFFFFFFF0, 1); render(0xFFFFFFF0)
call('AppControl_Process', 0xFFFFFFF1, 1)
keys.append(1); call('AppControl_Process', 0x00000020, 1); render(0x20)
call('AppControl_Process', 0x00000021, 1)
call('AppControl_Process', 0x00000214, 1)
assert field('is_speed_preview') == 1
call('AppControl_Process', 0x00000215, 1); render(0x215)
assert field('is_speed_preview') == 0
expect_frame('LcdSegments_BuildError', 1)
# An ACK immediately restores normal speed UI; preview expiry must not revive E-01.
keys.append(1); call('AppControl_Process', 6000, 1); render(6000)
complete(); inject(b'\x23\x01\x12'); step(6001); render(6001)
assert field('error_code') == field('is_speed_preview') == 0
expect_frame('LcdSegments_BuildSpeed', field('demo_speed_x10', 2), 0)
# Persistent fault always overrides preview, stops targets, and keeps its first code.
call('AppControl_SetErrors', 1, 0)
keys.append(0); call('AppControl_Process', 6100, 1); render(6100)
assert field('is_speed_preview') == 1
inject(event(3)); step(6101); render(6101)
assert field('is_speed_preview') == 0 and field('error_code') == 3
expect_frame('LcdSegments_BuildError', 3)
key(1, 6102); render(6102)
assert field('is_speed_preview') == 0 and field('demo_speed_x10', 2) == 0
expect_frame('LcdSegments_BuildError', 3)
print('PASS: E-01 speed-key preview renders target immediately for 500ms; LCD wait/repeat/wrap/ACK/fault priority')
