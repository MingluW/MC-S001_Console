#ifndef LCD_SEGMENTS_H
#define LCD_SEGMENTS_H

#include "lcd.h"

typedef enum
{
    LCD_SPEED_UNIT_KPH = 0,
    LCD_SPEED_UNIT_MPH
} LcdSpeedUnit;

/** 纯段码转换：0~9999代表0.0~999.9，附速度图标，整数高位消隐。
 * unit选择KPH或MPH，PH共用，K/M互斥；不换算数值。
 * 调用方提供16字节输出，立即填充，不保留指针；NULL/越界/无效单位返回false且不改输出。
 * 不访问硬件，仅主循环使用，不等待。不校验产品速度范围。
 */
bool LcdSegments_BuildSpeed(uint16_t speed_x10, LcdSpeedUnit unit,
                           uint8_t frame[LCD_FRAME_BYTES]);

/** 四位整数0~9999，pad_zero决定前导补0或消隐，无图标/小数点。
 * 16字节输出立即填充，不保留指针；NULL/越界返回false且不改输出。
 * 以下接口均不访问硬件、不等待，仅主循环使用。
 */
bool LcdSegments_BuildDigits(uint16_t value, bool pad_zero,
                            uint8_t frame[LCD_FRAME_BYTES]);

/** 整数坡度0~9999，高位消隐，附GRADE图标；缓冲及错误约定同BuildDigits。 */
bool LcdSegments_BuildIncline(uint16_t value, uint8_t frame[LCD_FRAME_BYTES]);

/** 构建----，无图标；NULL返回false，否则填满16字节，不保留指针。 */
bool LcdSegments_BuildStopped(uint8_t frame[LCD_FRAME_BYTES]);

#endif
