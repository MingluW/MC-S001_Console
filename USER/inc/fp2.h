#ifndef FP2_H
#define FP2_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define FP2_TARGET_KPH 301u
#define FP2_CURRENT_KPH 302u
#define FP2_CONTROLLER_ERROR 2108u
#define FP2_MOTOR_ERROR 2380u
#define FP2_MAX_MESSAGE_BYTES 257u

typedef struct
{
    uint8_t sender;
    uint8_t command;
    uint8_t replied_command;
    uint8_t error;
    uint16_t feature;
    uint32_t value_bits;
    float value;
} Fp2Message;

/** 编码写目标速度，speed_x10为KPH的10倍；0~300，输出固定8字节FP2。
 * 不含iCANt长度和CAN DLC填充；NULL/越界返回false。不保留指针，不等待。
 */
bool Fp2_BuildSpeed(uint16_t speed_x10, uint8_t output[8]);

/** 编码订阅302/2108/2380，固定8字节FP2；NULL返回false，约定同上。 */
bool Fp2_BuildSubscription(uint8_t output[8]);

/** 校验完整FP2帧并解析ACK/Error/Event/Features/Extended。
 * length包含两字节FP2头；拒绝长度不符、非法结构和非有限Event浮点值。
 * 输入只读，输出复制，不保留指针，无硬件依赖，均仅主循环调用。
 */
bool Fp2_Decode(const uint8_t *data, size_t length, Fp2Message *message);

#endif
