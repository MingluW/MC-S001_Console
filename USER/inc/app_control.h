#ifndef APP_CONTROL_H
#define APP_CONTROL_H

#include <stdbool.h>
#include <stdint.h>

/* 首版仅显示演示；范围和步长不是已经确认的产品参数。 */
#define APP_DEMO_SPEED_MAX_X10 300u
#define APP_DEMO_SPEED_STEP_X10 1u
#define APP_DEMO_INCLINE_MAX 15u
#define APP_BOOT_STEP_MS 500u
#define APP_WAIT_BLINK_MS 1000u
#define APP_START_INCLINE_MS 1000u

typedef enum
{
    APP_SCREEN_BOOT_ALL = 0,
    APP_SCREEN_BOOT_COUNTDOWN,
    APP_SCREEN_WAIT_START,
    APP_SCREEN_START_INCLINE,
    APP_SCREEN_SPEED,
    APP_SCREEN_INCLINE,
    APP_SCREEN_STOPPED
} AppScreen;

typedef enum
{
    APP_SPEED_UNIT_KPH = 0,
    APP_SPEED_UNIT_MPH
} AppSpeedUnit;

#define APP_DEFAULT_SPEED_UNIT APP_SPEED_UNIT_KPH

typedef struct
{
    uint16_t demo_speed_x10; /* 显示值的10倍，不代表电机实际或目标速度。 */
    AppSpeedUnit speed_unit;
    uint16_t demo_incline; /* 整数档位，仅显示演示。 */
    AppScreen screen;
    uint8_t countdown_digit;
    bool is_wait_visible;
} AppState;

/** 初始化为开机全亮，速度/坡度为0；仅主循环，不等待。 */
void AppControl_Init(void);

/** 选择显示单位；无效枚举返回false且不改状态。仅主循环，不等待。
 * 当前仅演示：保留显示数值，不进行km/h与mi/h换算、不保存至Flash。
 */
bool AppControl_SetSpeedUnit(AppSpeedUnit unit);

/** 推进界面时序并最多消费9条输入（1停止+8普通）；不发送下控命令。
 * now_ms单位毫秒；is_frame_complete表示最新目标帧已提交完成，初始化中/出错为false。
 * 动画停留从对应帧发送完成后计时；不等待，仅主循环。
 */
void AppControl_Process(uint32_t now_ms, bool is_frame_complete);

/** 返回模块持有的只读状态，直到下次Process前有效；不得写入，仅主循环。 */
const AppState *AppControl_GetState(void);

#endif
