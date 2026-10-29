#include "can_trace.h"

#include <stddef.h>

#include "log.h"

#define CAN_TRACE_CAPACITY 16u

typedef struct
{
    CanTraceType type;
    uint32_t sequence;
    BoardCanFrame frame;
} TraceRecord;

static TraceRecord records[CAN_TRACE_CAPACITY];
static uint8_t head;
static uint8_t count;
static uint8_t offset;
static uint32_t dropped;
static uint32_t reported_dropped;
static uint32_t last_report_ms;

void CanTrace_Init(void)
{
    head = count = offset = 0u;
    dropped = reported_dropped = last_report_ms = 0u;
}

bool CanTrace_Push(CanTraceType type, uint32_t sequence, const BoardCanFrame *frame)
{
    if ((frame == NULL) || (frame->length > 64u) || ((uint32_t)type > CAN_TRACE_RX))
    {
        return false;
    }
    if (count == CAN_TRACE_CAPACITY)
    {
        ++dropped;
        return false;
    }
    TraceRecord *record = &records[(head + count) % CAN_TRACE_CAPACITY];
    record->type = type;
    record->sequence = sequence;
    record->frame = *frame;
    ++count;
    return true;
}

void CanTrace_Process(uint32_t now_ms)
{
    static const char hex[] = "0123456789ABCDEF";
    static const char *const labels[] = {"TX_REQ", "TX_SENT", "RX"};
    char bytes[97];

    if ((dropped != reported_dropped) &&
        ((uint32_t)(now_ms - last_report_ms) >= 1000u))
    {
        last_report_ms = now_ms;
        if (WARN("[CAN TRACE] queue overflow, dropped=%lu", (unsigned long)dropped))
        {
            reported_dropped = dropped;
        }
    }
    if (count == 0u)
    {
        return;
    }
    TraceRecord *record = &records[head];
    uint8_t length = record->frame.length - offset;
    if (length > 32u)
    {
        length = 32u;
    }
    for (uint8_t i = 0u; i < length; ++i)
    {
        uint8_t value = record->frame.data[offset + i];
        bytes[3u * i] = hex[value >> 4];
        bytes[3u * i + 1u] = hex[value & 15u];
        bytes[3u * i + 2u] = ' ';
    }
    bytes[3u * length] = '\0';
    if (!DBG("[CAN %s] #%lu id=%03lX fd=%u brs=%u len=%u @%u %s",
             labels[record->type], (unsigned long)record->sequence,
             (unsigned long)record->frame.id, record->frame.is_fd, record->frame.has_brs,
             record->frame.length, offset, bytes))
    {
        return;
    }
    offset += length;
    if (offset == record->frame.length)
    {
        head = (head + 1u) % CAN_TRACE_CAPACITY;
        --count;
        offset = 0u;
    }
}
