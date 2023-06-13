// Copyright (c) 2021-2022 XMOS LIMITED. This Software is subject to the terms of the
// XMOS Public License: Version 1

#ifndef USB_AUDIO_H_
#define USB_AUDIO_H_

#include "app_conf.h"

/*
 * frame_buffers format assumes:
 *   raw_audio_frame
 */
void usb_audio_send(rtos_intertile_t *intertile_ctx,
                    size_t frame_count,
                    SAMPLE_TYPE **frame_buffers,
                    size_t num_chans);

/*
* frame_buffers format assumes:
*   raw_audio_frame
*/
void usb_audio_recv(rtos_intertile_t *intertile_ctx,
                    size_t frame_count,
                    int32_t **frame_buffers,
                    size_t num_chans);

void usb_audio_recv_blocking(rtos_intertile_t *intertile_ctx,
                             size_t frame_count,
                             int32_t **frame_buffers,
                             size_t num_chans);

void usb_audio_init(rtos_intertile_t *intertile_ctx, unsigned priority);


#endif /* USB_AUDIO_H_ */
