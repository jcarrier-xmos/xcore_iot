// Copyright (c) 2022 XMOS LIMITED. This Software is subject to the terms of the
// XMOS Public License: Version 1

#ifndef APP_CONF_CHECK_H_
#define APP_CONF_CHECK_H_

#if !XCOREAI_EXPLORER
#error Only the XCORE-AI-EXPLORER Board is supported
#endif

#if appconfI2S_MODE != appconfI2S_MODE_MASTER
#error I2S mode other than master is not currently supported
#endif

#if appconfUSB_OUTPUT == 0
#error appconfUSB_OUTPUT: Must be selected
#endif

#if appconfMIC_INPUT == 0
#error appconfMIC_INPUT: Must be selected
#endif

#if appconfUSB_INPUT
#error appconfUSB_INPUT: No supported
#endif

#if appconfI2S_INPUT
#error appconfI2S_INPUT: No supported
#endif

#endif /* APP_CONF_CHECK_H_ */
