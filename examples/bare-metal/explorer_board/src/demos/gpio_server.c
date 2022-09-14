// Copyright 2021 XMOS LIMITED.
// This Software is subject to the terms of the XMOS Public Licence: Version 1.

/* System headers */
#include <string.h>
#include <xcore/port.h>
#include <xcore/hwtimer.h>
#include <xcore/triggerable.h>

/* App headers */
#include "app_demos.h"

#define HEARTBEAT_TICKS 50000000
#define PWM_PERIOD_TICKS 800000
#define PWM_QUANTA_TICKS 12500 // 64 quantas of 12.5us = 8ms ==> 125Hz (well above flicker fusion)
#define PWM_QUANTAS 128

//#define LED_MASK_A       0x01
//#define LED_MASK_B       0x02
#define LED_MASK_PWM     0x07
#define LED_MASK_HBEAT   0x08

#define LED_ASSERT(buf, mask) buf |= mask;
#define LED_DEASSERT(buf, mask) buf &= ~mask;
#define LED_UPDATE(cond, buf, mask)     \
    do                                  \
    {                                   \
        if (cond)                       \
        {                               \
            LED_ASSERT(buf, mask);      \
        }                               \
        else                            \
        {                               \
            LED_DEASSERT(buf, mask);    \
        }                               \
    }                                   \
    while (0)

#if 1
void print_pwm_info(uint32_t pwm_period_ticks, uint32_t quanta_time_ticks, uint8_t pwm_in)
{
  uint32_t on_time = (100 * pwm_in * quanta_time_ticks) / pwm_period_ticks;
  uint32_t off_time = (100 * (pwm_period_ticks - (pwm_in * quanta_time_ticks))) / pwm_period_ticks;
  debug_printf("PWM Input: %d (ON/OFF: %d/%d)\n", pwm_in, on_time, off_time);
}

void gpio_server(chanend_t c_from_gpio, chanend_t c_to_gpio)
{
    //const uint32_t quanta_time = 12500;
    //const uint32_t input_time_remaining = 500;
    //const uint32_t pwm_period = 800000;
    uint32_t pwm_cycle_time;
    uint8_t current_quanta = 0;
    uint8_t pwm_input = 0;

    port_t p_leds = PORT_LEDS;
    port_t p_btns = PORT_BUTTONS;
    hwtimer_t tmr_hbeat = hwtimer_alloc();
    hwtimer_t tmr_pwm = hwtimer_alloc();

    port_enable(p_leds);
    port_enable(p_btns);

    uint32_t led_val = 0;
    uint32_t btn_val = port_in(p_btns);

    triggerable_disable_all();

    TRIGGERABLE_SETUP_EVENT_VECTOR(p_btns, event_btn);
    TRIGGERABLE_SETUP_EVENT_VECTOR(c_to_gpio, event_chan);
    TRIGGERABLE_SETUP_EVENT_VECTOR(tmr_hbeat, event_hbeat_timer);
    TRIGGERABLE_SETUP_EVENT_VECTOR(tmr_pwm, event_pwm_timer);

    port_set_trigger_in_not_equal(p_btns, btn_val);
    pwm_cycle_time = hwtimer_get_time(tmr_pwm) + PWM_QUANTA_TICKS;
    hwtimer_set_trigger_time(tmr_pwm, pwm_cycle_time);
    hwtimer_set_trigger_time(tmr_hbeat, hwtimer_get_time(tmr_hbeat) + HEARTBEAT_TICKS);

    triggerable_enable_trigger(p_btns);
    triggerable_enable_trigger(c_to_gpio);
    triggerable_enable_trigger(tmr_pwm);
    triggerable_enable_trigger(tmr_hbeat);

    while(1)
    {
        TRIGGERABLE_WAIT_EVENT(event_btn, event_chan, event_timer);
        {
            event_btn:
            {
                btn_val = port_in(p_btns);
                port_set_trigger_value(p_btns, btn_val);
                int btn0 = ( btn_val >> 0 ) & 0x01;
                int btn1 = ( btn_val >> 1 ) & 0x01;

                if (btn0 == 0)
                {
                    chanend_out_byte(c_from_gpio, 0x01);
                    //if (pwm_input < (PWM_QUANTAS - 1)) pwm_input++;
                    //print_pwm_info(pwm_period, quanta_time, pwm_input);
                }

                if (btn1 == 0)
                {
                    chanend_out_byte(c_from_gpio, 0x02);
                    //if (pwm_input > 0) pwm_input--;
                    //print_pwm_info(pwm_period, quanta_time, pwm_input);
                }
            }
            continue;
        }
        {
            event_chan:
            {
                // TODO use raw "power" level as the PWM input for the LED.
                pwm_input = chanend_in_byte(c_to_gpio);
            }
            continue;
        }
        {
            event_hbeat_timer:
            {
                hwtimer_set_trigger_time(tmr_hbeat, hwtimer_get_time(tmr_hbeat) + HEARTBEAT_TICKS);
                led_val ^= LED_MASK_HBEAT; // TODO: code below should adopt this.
                port_out(p_leds, led_val);
            }
            continue;
        }
        {
            event_pwm_timer:
            {
                //TODO which method is better/more correct?
                //pwm_cycle_time = hwtimer_get_time(tmr_pwm) + PWM_QUANTA_TICKS;
                pwm_cycle_time += PWM_QUANTA_TICKS;
                hwtimer_set_trigger_time(tmr_pwm, pwm_cycle_time);
                LED_UPDATE((pwm_input > current_quanta), led_val, LED_MASK_PWM);
                port_out(p_leds, led_val);
                current_quanta = (current_quanta + 1) % PWM_QUANTAS;
            }
            continue;
        }
    }
}

#else
void gpio_server(chanend_t c_from_gpio, chanend_t c_to_gpio)
{
    port_t p_leds = PORT_LEDS;
    port_t p_btns = PORT_BUTTONS;
    hwtimer_t tmr = hwtimer_alloc();

    port_enable(p_leds);
    port_enable(p_btns);

    uint32_t led_val = 0;
    uint32_t heartbeat_val = 0;
    uint32_t btn_val = port_in(p_btns);

    triggerable_disable_all();

    TRIGGERABLE_SETUP_EVENT_VECTOR(p_btns, event_btn);
    TRIGGERABLE_SETUP_EVENT_VECTOR(c_to_gpio, event_chan);
    TRIGGERABLE_SETUP_EVENT_VECTOR(tmr, event_timer);

    port_set_trigger_in_not_equal(p_btns, btn_val);
    hwtimer_set_trigger_time(tmr, hwtimer_get_time(tmr) + HEARTBEAT_TICKS);

    triggerable_enable_trigger(p_btns);
    triggerable_enable_trigger(c_to_gpio);
    triggerable_enable_trigger(tmr);

    while(1)
    {
        TRIGGERABLE_WAIT_EVENT(event_btn, event_chan, event_timer);
        {
            event_btn:
            {
                btn_val = port_in(p_btns);
                port_set_trigger_value(p_btns, btn_val);
                int btn0 = ( btn_val >> 0 ) & 0x01;
                int btn1 = ( btn_val >> 1 ) & 0x01;
                if (btn0 == 0)
                {
                    debug_printf("Button A pressed\n");
                    chanend_out_byte(c_from_gpio, 0x01);
                    led_val |= 0x01;
                } else {
                    led_val &= ~0x01;
                }

                if (btn1 == 0)
                {
                    debug_printf("Button B pressed\n");
                    chanend_out_byte(c_from_gpio, 0x02);
                    led_val |= 0x02;
                } else {
                    led_val &= ~0x02;
                }
                port_out(p_leds, led_val);
            }
            continue;
        }
        {
            event_chan:
            {
                char req_led_val = chanend_in_byte(c_to_gpio);
                if (req_led_val != 0) {
                    led_val |= 0x04;
                } else {
                    led_val &= ~0x04;
                }
                port_out(p_leds, led_val);
            }
            continue;
        }
        {
            event_timer:
            {
                hwtimer_set_trigger_time(tmr, hwtimer_get_time(tmr) + HEARTBEAT_TICKS);
                led_val ^= 0x08;
                port_out(p_leds, led_val);
            }
            continue;
        }
    }
}
#endif
