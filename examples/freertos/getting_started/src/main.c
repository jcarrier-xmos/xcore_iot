// Copyright 2021 XMOS LIMITED.
// This Software is subject to the terms of the XMOS Public Licence: Version 1.

#include <platform.h>

#include "FreeRTOS.h"
#include "task.h"

#include "rtos_printf.h"

#include "platform/platform_init.h"
#include "platform/driver_instances.h"

#define HELLO_TASK_MS       ( 5000 )
#define HBEAT_TIMER_MS      ( 500 )

#define PWM_TIMER_MS        ( 1 )             // TODO convert to some expression based on PWM_PHASE_TICKS and XS1_TIMER_HZ
#define PWM_PHASE_TICKS     ( 100000 )        // Relates to the timer specified for tmr_pwm
#define PWM_RTOS_TICKS_DIV  ( XS1_TIMER_MHZ ) // "Ticks" in this application are related to the 100MHz timer of XCORE instead of the FreeRTOS ticks.

#define PWM_QUANTAS         ( 64 )
#define PWM_QUANTA_TICKS    ( 12500 )                           // This is the time factor for converting the PWM input to ticks used by delay_ticks().
                                                                // In this case, 64 time quantas are used. 64 quantas of 12.5us = 8ms ==> 125Hz (well above flicker fusion)
#define PWM_PERIOD_TICKS    ( PWM_QUANTAS * PWM_QUANTA_TICKS )  // 64 quantas * 12500 ticks/quanta = 800000 ticks

//#define LED_MASK_A        0x01
//#define LED_MASK_B        0x02
#define LED_MASK_PWM        0x07
#define LED_MASK_HBEAT      0x08

#define LED_ASSERT(buf, mask)       buf |= mask;
#define LED_DEASSERT(buf, mask)     buf &= ~mask;
#define LED_TOGGLE(buf, mask)       buf ^= mask;
#define LED_UPDATE(cond, buf, mask)     \
    do {                                \
        if (cond) {                     \
            LED_ASSERT(buf, mask);      \
        } else {                        \
            LED_DEASSERT(buf, mask);    \
        }                               \
    } while (0)

#define GPIO_TASK_NOTIF_MASK_PWM_TIMER    0x80000000
#define GPIO_TASK_NOTIF_MASK_HBEAT_TIMER  0x40000000
#define GPIO_TASK_NOTIF_MASK_PWM_EVENT    0x00010000
#define GPIO_TASK_NOTIF_MASK_PWM_INPUT    0x0000FFFF

static TaskHandle_t gpio_task_handle = NULL;

void print_pwm_info(uint32_t pwm_quantas, uint32_t quanta_time_ticks, uint8_t pwm_input)
{
    uint32_t pwm_period_ticks = pwm_quantas * quanta_time_ticks;
    uint32_t on_time = (100 * pwm_input * quanta_time_ticks) / pwm_period_ticks;
    uint32_t off_time = (100 * (pwm_period_ticks - (pwm_input * quanta_time_ticks))) / pwm_period_ticks;
    debug_printf("PWM Input: %d (ON/OFF: %d/%d)\n", pwm_input, on_time, off_time);
}

RTOS_GPIO_ISR_CALLBACK_ATTR
static void button_callback(rtos_gpio_t *ctx, void *app_data, rtos_gpio_port_id_t port_id, uint32_t value)
{
    const uint32_t btn0Mask = 0x01;
    const uint32_t btn1Mask = 0x02;
    TaskHandle_t task = app_data;
    BaseType_t xYieldRequired = pdFALSE;

    // Convert active-low logic to active-high
    value = (~value) & (btn0Mask | btn1Mask);

    // Only notify on button press.
    if (value)
    {
      xTaskNotifyFromISR(task, (value | GPIO_TASK_NOTIF_MASK_PWM_EVENT), eSetValueWithOverwrite, &xYieldRequired);
      portYIELD_FROM_ISR(xYieldRequired);
    }
}

void hbeat_callback( TimerHandle_t pxTimer )
{
    xTaskNotify(gpio_task_handle, GPIO_TASK_NOTIF_MASK_HBEAT_TIMER, eSetBits);
}

void pwm_callback( TimerHandle_t pxTimer )
{
    xTaskNotify(gpio_task_handle, GPIO_TASK_NOTIF_MASK_PWM_TIMER, eSetBits);
}

void gpio_task(void *arg)
{
    uint32_t led_val = 0;
    uint32_t pwm_input = 32;////0;
    uint32_t status;
    uint8_t pwm_phase = 0;
    const uint8_t pwm_phases = 8;
    uint32_t pwm_on_ticks;
    TimerHandle_t tmr_hbeat;
    TimerHandle_t tmr_pwm;
    rtos_gpio_port_id_t p_leds = rtos_gpio_port(PORT_LEDS);
    rtos_gpio_port_id_t p_btns = rtos_gpio_port(PORT_BUTTONS);

    rtos_gpio_port_enable(gpio_ctx_t0, p_leds);
    rtos_gpio_port_enable(gpio_ctx_t0, p_btns);
    rtos_gpio_isr_callback_set(gpio_ctx_t0, p_btns, button_callback, xTaskGetCurrentTaskHandle());
    rtos_gpio_interrupt_enable(gpio_ctx_t0, p_btns);

    tmr_hbeat = xTimerCreate(
                      "tmr_hbeat",
                      pdMS_TO_TICKS(HBEAT_TIMER_MS),
                      pdTRUE,
                      NULL,
                      hbeat_callback );
    tmr_pwm = xTimerCreate(
                      "tmr_pwm",
                      pdMS_TO_TICKS(PWM_TIMER_MS),
                      pdTRUE,
                      NULL,
                      pwm_callback );

    xTimerStart(tmr_hbeat, 0);
    xTimerStart(tmr_pwm, 0);

    // The PWM process offloads the majority of the timing into the PWM timer
    // only submillisecond delays are invoked via delay_ticks() in order to
    // reduce the amount of blocking performed.
    while (1)
    {
        // PWM Timer event   = 0x8XXXXXXX
        // PWM Input event   = 0xXXXXnnnn
        xTaskNotifyWait(
                0x00000000UL,    /* Don't clear notification bits on entry */
                0xFFFFFFFFUL,    /* Reset full notification value on exit */
                &status,         /* Pass out notification value into status */
                portMAX_DELAY ); /* Wait indefinitely until next notification */

        if (status & GPIO_TASK_NOTIF_MASK_PWM_TIMER)
        {
            status &= ~GPIO_TASK_NOTIF_MASK_PWM_TIMER;

            if (pwm_phase == 0)
            {
                // Initialize PWM ON ticks time which will be used to
                // determine during which timer event phase the PWM output state
                // should switch OFF.
                pwm_on_ticks = pwm_input * PWM_QUANTA_TICKS;

                if ((pwm_on_ticks > 0) && (pwm_on_ticks < PWM_PHASE_TICKS))
                {
                    LED_ASSERT(led_val, LED_MASK_PWM);
                    rtos_gpio_port_out(gpio_ctx_t0, p_leds, led_val);
                }
            }

            if (pwm_input == 0)
            {
                // Always ON
                LED_DEASSERT(led_val, LED_MASK_PWM);
            }
            else if (pwm_input == PWM_QUANTAS - 1)
            {
                // Always OFF
                LED_ASSERT(led_val, LED_MASK_PWM);
            }
            else
            {
                if (pwm_on_ticks >= PWM_PHASE_TICKS)
                {
                    // The PWM output will remain on through the entirety of this phase.
                    LED_ASSERT(led_val, LED_MASK_PWM);
                    pwm_on_ticks -= PWM_PHASE_TICKS;
                }
                else if (pwm_on_ticks > 0)
                {
                    delay_ticks( pwm_on_ticks % PWM_PHASE_TICKS ); // Ticks in this context are 100MHz ticks
                    //vTaskDelay( pwm_on_ticks / PWM_PHASE_TICKS / PWM_RTOS_TICKS_DIV ); // Ticks in this context are 1KHz ticks
                    LED_DEASSERT(led_val, LED_MASK_PWM);
                }
                else
                {
                    // The PWM output will remain off through the entirety of this phase.
                    LED_DEASSERT(led_val, LED_MASK_PWM);
                }

                pwm_phase = (pwm_phase + 1) % pwm_phases;
            }
        }

        if (status & GPIO_TASK_NOTIF_MASK_HBEAT_TIMER)
        {
            LED_TOGGLE(led_val, LED_MASK_HBEAT);
        }

        rtos_gpio_port_out(gpio_ctx_t0, p_leds, led_val);

        if ( status & GPIO_TASK_NOTIF_MASK_PWM_EVENT )
        {
            uint8_t btn0 = ( (status & GPIO_TASK_NOTIF_MASK_PWM_INPUT) >> 0 ) & 0x01;
            uint8_t btn1 = ( (status & GPIO_TASK_NOTIF_MASK_PWM_INPUT) >> 1 ) & 0x01;

            if (btn0 == 1)
            {
                if (pwm_input > 0) pwm_input--;
            }

            if (btn1 == 1)
            {
                if (pwm_input < (PWM_QUANTAS - 1)) pwm_input++;
            }

            pwm_phase = 0;
            print_pwm_info(PWM_QUANTAS, PWM_QUANTA_TICKS, pwm_input);
        }
    }
}

void tile0_hello_task(void *arg) {
  rtos_printf("Hello task running from tile %d on core %d\n", THIS_XCORE_TILE,
              portGET_CORE_ID());

  for (;;) {
    rtos_printf("Hello from tile %d\n", THIS_XCORE_TILE);
    vTaskDelay(pdMS_TO_TICKS(HELLO_TASK_MS));
  }
}

void tile1_hello_task(void *arg) {
  rtos_printf("Hello task running from tile %d on core %d\n", THIS_XCORE_TILE,
              portGET_CORE_ID());

  for (;;) {
    rtos_printf("Hello from tile %d\n", THIS_XCORE_TILE);
    vTaskDelay(pdMS_TO_TICKS(HELLO_TASK_MS));
  }
}

static void tile_common_init(chanend_t c) {
  platform_init(c);

#if ON_TILE(0)
  xTaskCreate((TaskFunction_t)tile0_hello_task, "tile0_hello_task",
              RTOS_THREAD_STACK_SIZE(tile0_hello_task), NULL,
              configMAX_PRIORITIES - 1, NULL);
  xTaskCreate((TaskFunction_t)gpio_task, "gpio_task",
              RTOS_THREAD_STACK_SIZE(gpio_task), NULL,
              configMAX_PRIORITIES - 1, &gpio_task_handle);
#endif

#if ON_TILE(1)
  xTaskCreate((TaskFunction_t)tile1_hello_task, "tile1_hello_task",
              RTOS_THREAD_STACK_SIZE(tile1_hello_task), NULL,
              configMAX_PRIORITIES - 1, NULL);

#endif

  rtos_printf("Start scheduler on tile %d\n", THIS_XCORE_TILE);
  vTaskStartScheduler();
}

#if ON_TILE(0)
void main_tile0(chanend_t c0, chanend_t c1, chanend_t c2, chanend_t c3) {

  (void)c0;
  (void)c2;
  (void)c3;

  tile_common_init(c1);
}
#endif

#if ON_TILE(1)
void main_tile1(chanend_t c0, chanend_t c1, chanend_t c2, chanend_t c3) {
  (void)c1;
  (void)c2;
  (void)c3;

  tile_common_init(c0);
}
#endif