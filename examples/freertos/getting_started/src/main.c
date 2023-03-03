// Copyright 2021 XMOS LIMITED.
// This Software is subject to the terms of the XMOS Public Licence: Version 1.

#include <platform.h>

#include "FreeRTOS.h"
#include "task.h"

#include "rtos_printf.h"

#include "platform/platform_init.h"
#include "platform/driver_instances.h"

#if ON_TILE(0)
#include "usb_demo.h"
#endif

void tile0_blinky_task(void *arg) {
  rtos_printf("Blinky task running from tile %d on core %d\n", THIS_XCORE_TILE,
              portGET_CORE_ID());

  uint32_t gpio_port = rtos_gpio_port(XS1_PORT_4C);

  rtos_gpio_port_enable(gpio_ctx_t0, gpio_port);

  for (;;) {
    rtos_gpio_port_out(gpio_ctx_t0, gpio_port, 0x000F);
    vTaskDelay(pdMS_TO_TICKS(500));
    rtos_gpio_port_out(gpio_ctx_t0, gpio_port, 0x0000);
    vTaskDelay(pdMS_TO_TICKS(500));
  }
}

void startup_task(void *arg)
{
    rtos_printf("Startup task running from tile %d on core %d\n", THIS_XCORE_TILE, portGET_CORE_ID());

#if ON_TILE(0)
    usb_demo_start();
#endif

    //for (;;) {
    //  rtos_printf("Tile[%d]:\n\tMinimum heap free: %d\n\tCurrent heap free: %d\n", THIS_XCORE_TILE, xPortGetMinimumEverFreeHeapSize(), xPortGetFreeHeapSize());
    //  vTaskDelay(pdMS_TO_TICKS(5000));
    //}

    /* Done */
    vTaskDelete(NULL);
}

static void tile_common_init(chanend_t c)
{
  platform_init(c);

#if ON_TILE(0)
  usb_demo_init();

  xTaskCreate((TaskFunction_t)tile0_blinky_task, "tile0_blinky_task",
              RTOS_THREAD_STACK_SIZE(tile0_blinky_task), NULL,
              configMAX_PRIORITIES - 1, NULL);
#endif

  xTaskCreate((TaskFunction_t) startup_task,
              "startup_task",
              RTOS_THREAD_STACK_SIZE(startup_task),
              NULL,
              configMAX_PRIORITIES - 1,
              NULL);

  rtos_printf("Start scheduler on tile %d\n", THIS_XCORE_TILE);
  vTaskStartScheduler();
}

#if ON_TILE(0)
void main_tile0(chanend_t c0, chanend_t c1, chanend_t c2, chanend_t c3)
{
  (void)c0;
  (void)c2;
  (void)c3;

  tile_common_init(c1);
}
#endif

#if ON_TILE(1)
void main_tile1(chanend_t c0, chanend_t c1, chanend_t c2, chanend_t c3)
{
  (void)c1;
  (void)c2;
  (void)c3;

  tile_common_init(c0);
}
#endif