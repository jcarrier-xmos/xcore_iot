/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "FreeRTOS.h"
#include "demo_main.h"
#include "tusb.h"
#include "rtos_printf.h"

// echo to either Serial0 or Serial1
// with Serial0 as all lower case, Serial1 as all upper case
static void echo_serial_port(uint8_t itf, uint8_t buf[], uint32_t count)
{
  for(uint32_t i=0; i<count; i++)
  {
    if (itf == 0)
    {
      // echo back 1st port as lower case
      if (isupper(buf[i])) buf[i] += 'a' - 'A';
    }
    else
    {
      // echo back 2nd port as upper case
      if (islower(buf[i])) buf[i] -= 'a' - 'A';
    }

    tud_cdc_n_write_char(itf, buf[i]);
  }
  tud_cdc_n_write_flush(itf);
}

//--------------------------------------------------------------------+
// USB CDC
//--------------------------------------------------------------------+
static void cdc_task(void)
{
  uint8_t itf;

  for (itf = 0; itf < CFG_TUD_CDC; itf++)
  {
    // connected() check for DTR bit
    // Most but not all terminal client set this when making connection
    // if ( tud_cdc_n_connected(itf) )
    {
      if ( tud_cdc_n_available(itf) )
      {
        uint8_t buf[64];

        uint32_t count = tud_cdc_n_read(itf, buf, sizeof(buf));

        // echo back to both serial ports
        //echo_serial_port(0, buf, count);
        //echo_serial_port(1, buf, count);
        rtos_printf("RX %i: %.*s\n", itf, count, buf);
      }
    }
  }
}

static void cdc_task_wrapper_rx(void *arg) {
  rtos_printf("Entered cdc rx task\n");
    while(1) {
        cdc_task();
    }
}

static void cdc_task_wrapper_tx(void *arg) {
    while(1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        //tud_cdc_n_write_char(0, 'a');
        //tud_cdc_n_write_flush(0);
        //tud_cdc_n_write_char(1, 'b');
        //tud_cdc_n_write_flush(1);
    }
}

void create_tinyusb_demo(rtos_gpio_t *ctx, unsigned priority)
{
    xTaskCreate((TaskFunction_t) cdc_task_wrapper_rx,
                "cdc_rx_task",
                portTASK_STACK_DEPTH(cdc_task_wrapper_rx),
                NULL,
                priority,
                NULL);
    xTaskCreate((TaskFunction_t) cdc_task_wrapper_tx,
                "cdc_tx_task",
                portTASK_STACK_DEPTH(cdc_task_wrapper_tx),
                NULL,
                priority,
                NULL);
}
