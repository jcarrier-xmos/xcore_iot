// Copyright 2021 XMOS LIMITED.
// This Software is subject to the terms of the XMOS Public Licence: Version 1.

/* App headers */
#include "app_conf.h"
#include "app_demos.h"
#include "burn.h"
#include "audio_pipeline.h"
#include "platform_init.h"

#include "xud.h"
#include "xud_device.h"

DECLARE_JOB(_XUD_Main, (chanend_t*, int, chanend_t*, int, chanend_t, XUD_EpType*, XUD_EpType*, XUD_BusSpeed_t, XUD_PwrConfig));
void _XUD_Main(chanend_t *c_epOut, int noEpOut, chanend_t *c_epIn, int noEpIn, chanend_t c_sof, XUD_EpType *epTypeTableOut, XUD_EpType *epTypeTableIn, XUD_BusSpeed_t desiredSpeed, XUD_PwrConfig pwrConfig);

DECLARE_JOB(hid_mouse, (chanend_t));
void hid_mouse(chanend_t chan_ep_hid);

DECLARE_JOB(Endpoint0, (chanend_t, chanend_t));
void Endpoint0(chanend_t chan_ep0_out, chanend_t chan_ep0_in);

/* Endpoint type tables - informs XUD what the transfer types for each Endpoint in use and also
 * if the endpoint wishes to be informed of USB bus resets
 */
XUD_EpType epTypeTableOut[EP_COUNT_OUT] = { XUD_EPTYPE_CTL | XUD_STATUS_ENABLE };
XUD_EpType epTypeTableIn[EP_COUNT_IN]   = { XUD_EPTYPE_CTL | XUD_STATUS_ENABLE, XUD_EPTYPE_BUL };

void main_tile0(chanend_t c0, chanend_t c1, chanend_t c2, chanend_t c3)
{
    (void)c0;
    (void)c2;
    (void)c3;

    platform_init_tile_0(c1);

    channel_t channel_ep_out[EP_COUNT_OUT];
    channel_t channel_ep_in[EP_COUNT_IN];

    for(int i = 0; i < sizeof(channel_ep_out) / sizeof(*channel_ep_out); ++i) {
        channel_ep_out[i] = chan_alloc();
    }
    for(int i = 0; i < sizeof(channel_ep_in) / sizeof(*channel_ep_in); ++i) {
        channel_ep_in[i] = chan_alloc();
    }

    chanend_t c_ep_out[EP_COUNT_OUT];
    chanend_t c_ep_in[EP_COUNT_IN];

    for(int i = 0; i < EP_COUNT_OUT; ++i) {
        c_ep_out[i] = channel_ep_out[i].end_a;
    }
    for(int i = 0; i < EP_COUNT_IN; ++i) {
        c_ep_in[i] = channel_ep_in[i].end_a;
    }

    PAR_JOBS (
        PJOB(_XUD_Main, (c_ep_out, EP_COUNT_OUT, c_ep_in, EP_COUNT_IN, 0, epTypeTableOut, epTypeTableIn, XUD_SPEED_FS, XUD_PWR_BUS)),
        PJOB(Endpoint0, (channel_ep_out[0].end_b, channel_ep_in[0].end_b)),
        PJOB(hid_mouse, (channel_ep_in[1].end_b))
    );
}

void main_tile1(chanend_t c0, chanend_t c1, chanend_t c2, chanend_t c3)
{
    (void)c1;
    (void)c2;
    (void)c3;

    platform_init_tile_1(c0);

    PAR_JOBS (
        PJOB(burn, ())
    );
}
