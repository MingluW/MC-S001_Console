#include "fp2.h"

#include <string.h>

static uint16_t read_u16(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

bool Fp2_BuildSpeed(uint16_t speed_x10, uint8_t output[8])
{
    uint32_t bits;
    float value;

    if ((output == NULL) || (speed_x10 > 300u))
    {
        return false;
    }
    value = (float)speed_x10 / 10.0f;
    memcpy(&bits, &value, sizeof(bits));
    output[0] = 0x12u; /* sender=Console Brainboard, command=Write */
    output[1] = 6u;
    output[2] = (uint8_t)FP2_TARGET_KPH;
    output[3] = (uint8_t)(FP2_TARGET_KPH >> 8);
    for (size_t i = 0u; i < 4u; ++i)
    {
        output[4u + i] = (uint8_t)(bits >> (8u * i));
    }
    return true;
}

bool Fp2_BuildSubscription(uint8_t output[8])
{
    static const uint16_t features[3] = {
        FP2_CURRENT_KPH, FP2_CONTROLLER_ERROR, FP2_MOTOR_ERROR
    };
    if (output == NULL)
    {
        return false;
    }
    output[0] = 0x11u;
    output[1] = 6u;
    for (size_t i = 0u; i < 3u; ++i)
    {
        output[2u + 2u * i] = (uint8_t)features[i];
        output[3u + 2u * i] = (uint8_t)(features[i] >> 8);
    }
    return true;
}

bool Fp2_Decode(const uint8_t *data, size_t length, Fp2Message *message)
{
    Fp2Message decoded = {0};

    if ((data == NULL) || (message == NULL) || (length < 2u) ||
        (length != (size_t)data[1] + 2u))
    {
        return false;
    }
    decoded.sender = data[0] >> 4;
    decoded.command = data[0] & 15u;
    switch (decoded.command)
    {
        case 3u: /* ACK */
            if (data[1] != 1u)
            {
                return false;
            }
            decoded.replied_command = data[2];
            break;
        case 4u: /* Error */
            if (data[1] < 2u)
            {
                return false;
            }
            decoded.replied_command = data[2];
            decoded.error = data[3];
            break;
        case 5u: /* Event */
            if (data[1] != 6u)
            {
                return false;
            }
            decoded.feature = read_u16(data + 2u);
            for (size_t i = 0u; i < 4u; ++i)
            {
                decoded.value_bits |= (uint32_t)data[4u + i] << (8u * i);
            }
            if ((decoded.value_bits & 0x7F800000u) == 0x7F800000u)
            {
                return false;
            }
            memcpy(&decoded.value, &decoded.value_bits, sizeof(decoded.value));
            break;
        case 1u: /* Features */
            if ((data[1] & 1u) != 0u)
            {
                return false;
            }
            break;
        case 14u: /* Extended */
            if (data[1] == 0u)
            {
                return false;
            }
            break;
        default:
            return false;
    }
    *message = decoded;
    return true;
}
