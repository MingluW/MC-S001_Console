"""Actual production ARM ELF: safety lock, STOP preemption/retry and warning frames.
Includes CAN/fault-policy regressions; no board or flash access.
"""
from pathlib import Path
import runpy
import struct

p = runpy.run_path(str(Path(__file__).with_name('test_fault_policy.py')))
e = p['e']
call, uc, symbols = p['call'], p['uc'], p['symbols']
setup, start, step = p['setup'], p['start_ui'], p['step']
field, frames, render = p['field'], p['frames'], p['render']
requests, complete, inject = p['requests'], p['complete'], p['inject']
keys = p['keys']
from unicorn import UC_HOOK_CODE, UC_HOOK_MEM_WRITE
from unicorn.arm_const import UC_ARM_REG_R0

held = 0

def raw_hook(machine, address, size, context):
    if address == (symbols['BoardKeys_Read'] & ~1):
        e['env']['return_value'](held)
uc.hook_add(UC_HOOK_CODE, raw_hook)


def control(t, disconnected=None, frame_complete=1, events=()):
    if disconnected is not None:
        call('AppControl_UpdateSafety', t & 0xFFFFFFFF, disconnected)
    keys.extend(events)
    call('AppControl_Process', t & 0xFFFFFFFF, frame_complete)
    render(t & 0xFFFFFFFF)


def locked():
    assert field('is_safety_active') == 1
    assert field('screen') == 6 and field('demo_speed_x10', 2) == 0
    assert field('is_speed_preview') == 0


# Verify every lit segment against the original reference's 5/A/F/E rows.
import re
ref = Path('C:/Users/mingluw/Documents/cypress/260824/Q008_0926.cydsn/lcdfont.c').read_text(encoding='utf-8', errors='replace')
rows = []
for position, character in [(1, 5), (2, 10), (3, 15), (4, 14)]:
    block = ref.split('Tube%d_code' % position, 1)[1].split('};', 1)[0]
    block = re.sub(r'/\*.*?\*/|//[^\n]*', '', block, flags=re.S)
    values = [int(x, 16) for x in re.findall(r'0[xX]([0-9a-fA-F]+)', block)]
    rows.append(values[character * 16:(character + 1) * 16])
assert all(len(row) == 16 for row in rows)
safe_frame = bytes(a | b | c | d for a, b, c, d in zip(*rows))
ptr = e['scratch'] + 2048
assert call('LcdSegments_BuildSafety', 0) == 0
assert call('LcdSegments_BuildSafety', ptr) == 1
assert bytes(uc.mem_read(ptr, 16)) == safe_frame
print('PASS: 5AFE segments equal original Cypress 5/A/F/E, without icons or decimal point')

# Actual SDK GPIO configuration and active levels, without electrical hardware model.
pa, pb = 0x4002C000, 0x4002C400
gpio_writes = []
def gpio_write(machine, access, address, size, value, context):
    if pb <= address < pb + 0x40:
        gpio_writes.append((address, value))
uc.hook_add(UC_HOOK_MEM_WRITE, gpio_write)
call('BoardSafety_Init')
assert gpio_writes[0] == (pb + 0x24, 1 << 6)  # preload inhibit before output mode
assert (e['read32'](pa) >> 18) & 3 == 0  # PA9 input
assert (e['read32'](pb) >> 12) & 3 == 1  # PB6 output
for level in [0, 1]:
    e['write32'](pa + 0x2C, level << 9)
    assert call('BoardSafety_IsDisconnected') == level
call('BoardSafety_SetInhibited', 0)
assert gpio_writes[-1] == (pb + 0x28, 1 << 6)
print('PASS: real SDK PA9 input/high=disconnected; PB6 inhibit preloaded before output, active high')

# Old target already in hardware, newer target queued, cancel not yet confirmed.
setup(); start(); call('AppDisplay_Init', 0)
p['key'](1, 3509)
assert call('Powerboard_RequestSpeed', 12) == 1
e['memory_hook'].__globals__['cancel_immediately'] = False
control(3510, 1, events=(2, 1, 4, 3))
locked(); assert frames[-1] == safe_frame
step(3510)
count = len(requests)
assert call('Powerboard_RequestStart', 3) == 0
assert call('Powerboard_RequestSpeed', 12) == 0
for t in [3511, 3600, 4000]:
    control(t, 1, frame_complete=0, events=(2, 1, 0, 4, 5, 3))
    step(t); locked()
    assert len(requests) == count
# STOP only follows cancellation; a failed STOP retries at 500ms despite repeated locks.
complete(False); step(4001)
assert len(requests) == count + 1 and requests[-1][1][5:9] == bytes(4)
complete(False); step(4002)
for t in [4003, 4501]:
    control(t, 1, frame_complete=0); step(t)
    assert len(requests) == count + 1
control(4502, 1, frame_complete=0); step(4502)
assert len(requests) == count + 2 and requests[-1][1][5:9] == bytes(4)
complete(); inject(b'\x23\x01\x12'); step(4503)
locked()  # ACK does not clear physical lock
assert field('error_code') == 0
print('PASS: lock preempts queued/in-flight nonzero target, waits for cancellation; STOP retry/ACK stays locked')

# Stable text while LCD busy or time advances: no blinking or redundant submissions.
control(5000, 1, frame_complete=0)
assert frames[-1] == safe_frame
frame_count = len(frames)
for t in [5001, 5500, 5501, 7000, 7001, 7500, 7501]:
    control(t, 1, frame_complete=0)
    assert frames[-1] == safe_frame and len(frames) == frame_count
# Reconnect bounce, then held START must be released before accepting a fresh press.
control(7600, 0); control(7699, 0); locked()
control(7700, 1); control(7701, 0); control(7800, 0); locked()
held = 1 << 2
control(7801, 0, events=(2, 1)); step(7801)
assert field('is_safety_active') == 0 and field('screen') == 6
p['expect_frame']('LcdSegments_BuildStopped')
control(9000, 0, events=(2,)); assert field('screen') == 6
held = 0
control(9001, 0); control(9030, 0, events=(2,)); assert field('screen') == 6
control(9031, 0, events=(2,)); assert field('screen') == 6
control(9032, 0, events=(2,)); step(9032)
assert field('screen') == 3
assert requests[-1][1][5:9] == struct.pack('<f', .3)
print('PASS: steady 5AFE frame, no blinking/redundant writes; bounce/held-key recovery requires new START')

# Fault received during warning remains first-cause latched after reconnect.
e['memory_hook'].__globals__['cancel_immediately'] = True
setup(); start(); call('AppDisplay_Init', 0)
control(3510, 1); step(3510)
inject(p['event'](3)); step(3511); render(3511)
assert field('error_code') == 3 and frames[-1] == safe_frame
control(3520, 0); control(3620, 0); step(3620); render(3620)
p['expect_frame']('LcdSegments_BuildError', 3)
control(3650, 0); control(3651, 0, events=(2, 1, 4)); step(3651)
assert field('screen') == 6 and field('demo_speed_x10', 2) == 0
assert call('Powerboard_RequestStart', 3) == 0
inject(p['event'](6)); step(3652)
assert field('error_code') == call('Powerboard_GetControllerError') == 3

# Boot-disconnected bypasses boot animation; timing crosses uint32 rollover.
setup(); call('AppDisplay_Init', 0)
control(0xFFFFFF00, 1); step(0xFFFFFF00); locked()
control(0xFFFFFF01, 1)
control(0x000000F4, 1); assert frames[-1] == safe_frame
control(0x000000F5, 1); assert frames[-1] == safe_frame
control(0xFFFFFFF0, 0); control(0x53, 0); locked()
control(0x54, 0); assert field('is_safety_active') == 0 and field('screen') == 6
print('PASS: ordinary first fault survives safety reconnect; boot-disconnected/wraparound remain stopped')

# Real App_Process wiring: raw GPIO detection precedes keys/CAN; UART/LED keep running.
setup(); e['write32'](pa + 0x2C, 1 << 9)
e['env']['hook'].__globals__['now_ms'] = 20000
call('App_Process')
locked()
assert gpio_writes[-1] == (pb + 0x24, 1 << 6) or (pb + 0x24, 1 << 6) in gpio_writes[-3:]
assert requests[-1][1][5:9] == bytes(4)
print('PASS: real App_Process detects raw lock before requests and returns while servicing CAN/UART/LED/LCD')
