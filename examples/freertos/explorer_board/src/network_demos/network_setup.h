// Copyright (c) 2021, XMOS Ltd, All rights reserved

#ifndef NETWORK_SETUP_H_
#define NETWORK_SETUP_H_

/* Initalize WiFi */
void wifi_start(rtos_spi_master_device_t *wifi_device_ctx, rtos_gpio_t *gpio_ctx);

#endif /* NETWORK_SETUP_H_ */
