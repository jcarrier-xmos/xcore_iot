// Copyright (c) 2022 XMOS LIMITED. This Software is subject to the terms of the
// XMOS Public License: Version 1

#include <platform.h>
#include <xs1.h>
#include <xcore/channel.h>

/* FreeRTOS headers */
#include "FreeRTOS.h"
#include "task.h"
#include "stream_buffer.h"
#include "queue.h"

/* Library headers */
#include "src.h"

/* App headers */
#include "app_conf.h"
#include "platform/platform_init.h"
#include "platform/driver_instances.h"
#include "usb_support.h"
#include "usb_audio.h"
#include "audio_pipeline.h"

//#define BLOCKS_TO_BUFFER 34 // ~0.5 sec @ 16KHz
#define BLOCKS_TO_BUFFER 67 // ~1 sec @ 16KHz
#define CHANNELS_TO_BUFFER 2 //appconfAUDIO_PIPELINE_CHANNELS

#define TOTAL_SAMPLES_PER_CHANNEL (appconfAUDIO_PIPELINE_FRAME_ADVANCE * BLOCKS_TO_BUFFER)
#define TOTAL_SAMPLES (TOTAL_SAMPLES_PER_CHANNEL * CHANNELS_TO_BUFFER)

static int mic_assertion_disabled = 0;
static int sample_buffer_full = 0;
static int num_samples_buffered = 0;
static int sample_read_index = 0;
int buffering_channel_a; // Cycle 0..3
int buffering_channel_b; // Cycle 4..7

#include "rtos_mic_array.h"
#if SAMPLE_FORMAT == 0//RTOS_MIC_ARRAY_CHANNEL_SAMPLE
SAMPLE_TYPE sample_buffer[CHANNELS_TO_BUFFER + 1][TOTAL_SAMPLES_PER_CHANNEL];
#else // RTOS_MIC_ARRAY_SAMPLE_CHANNEL
SAMPLE_TYPE sample_buffer[TOTAL_SAMPLES_PER_CHANNEL][CHANNELS_TO_BUFFER + 1];
#endif

#include <math.h>

void GenerateWaveform(void)
{
    for (int ch = 0; ch < CHANNELS_TO_BUFFER; ch++)
    {
        uint32_t freq = (1000 * ((buffering_channel_a << 1) + ch + 1));
        rtos_printf("Freq: %d\n", freq);

        for (int s = 0; s < TOTAL_SAMPLES_PER_CHANNEL; s++)
        {
#if SAMPLE_FORMAT == 0 // RTOS_MIC_ARRAY_CHANNEL_SAMPLE
            sample_buffer[ch][s] = ((SAMPLE_TYPE)(16384 * sin((2 * 3.14f * freq * s) / TOTAL_SAMPLES_PER_CHANNEL)));
            //SAMPLE_TYPE* dst = (SAMPLE_TYPE*)sample_buffer + (s * CHANNELS_TO_BUFFER) + ch;
            //*dst = ((SAMPLE_TYPE)(16384 * sin((2 * 3.14f * freq * s) / TOTAL_SAMPLES_PER_CHANNEL)));
#else
            sample_buffer[s][ch] = ((SAMPLE_TYPE)(16384 * sin((2 * 3.14f * freq * s) / TOTAL_SAMPLES_PER_CHANNEL)));
#endif
        }
    }
}

void audio_pipeline_input(void *input_app_data,
                        int32_t **input_audio_frames,
                        size_t ch_count,
                        size_t frame_count)
{
    (void) input_app_data;

#if appconfMIC_INPUT
    static int flushed;
    while (!flushed) {
        size_t received;
        received = rtos_mic_array_rx(mic_array_ctx,
                                     input_audio_frames,
                                     frame_count,
                                     0);
        if (received == 0) {
            rtos_mic_array_rx(mic_array_ctx,
                              input_audio_frames,
                              frame_count,
                              portMAX_DELAY);
            flushed = 1;
        }
    }

    /*
     * NOTE: ALWAYS receive the next frame from the PDM mics,
     * even if USB is the current mic source. The controls the
     * timing since usb_audio_recv() does not block and will
     * receive all zeros if no frame is available yet.
     */
    rtos_mic_array_rx(mic_array_ctx,
                      input_audio_frames,
                      frame_count,
                      portMAX_DELAY);
#endif

    if (!sample_buffer_full && mic_assertion_disabled)
    {
        mic_assertion_disabled = 0;
        //rtos_mic_array_assertion_enable(); // TODO enable after fixing 2-mic issue
    }
}

int audio_pipeline_output(void *output_app_data,
                        int32_t **output_audio_frames,
                        size_t ch_count,
                        size_t frame_count)
{
    (void) output_app_data;

    // Switch between filling up a buffer and sending that buffer to USB
    // While transferring to USB the PDM RX ISR assertion is disabled.
    if (sample_buffer_full)
    {
#if appconfUSB_OUTPUT
        usb_audio_send(
            intertile_ctx,
            appconfAUDIO_PIPELINE_FRAME_ADVANCE,
            (SAMPLE_TYPE **)&((SAMPLE_TYPE *)sample_buffer)[sample_read_index],
            CHANNELS_TO_BUFFER); //2); //appconfAUDIO_PIPELINE_CHANNELS);
#endif
        if (num_samples_buffered <= appconfAUDIO_PIPELINE_FRAME_ADVANCE) {
            rtos_printf("Done.\n");
            GenerateWaveform();
            num_samples_buffered = 0;
            sample_buffer_full = 0;
        } else {
            sample_read_index += appconfAUDIO_PIPELINE_FRAME_ADVANCE;
            num_samples_buffered -= appconfAUDIO_PIPELINE_FRAME_ADVANCE;
        }
    } else {
        int num_samples = CHANNELS_TO_BUFFER * frame_count;
        if ((num_samples + num_samples_buffered) >= TOTAL_SAMPLES) {
            num_samples -= (TOTAL_SAMPLES - num_samples_buffered);
            sample_buffer_full = 1;
            rtos_mic_array_assertion_disable();
            rtos_printf("Capture complete (Mic: %d and %d)!\n", buffering_channel_a, buffering_channel_b);
            rtos_printf("Dumping to Samples...\n");

#if 0
            // Rotate which pair of mics to buffer.
            buffering_channel_a = (buffering_channel_a + 1) % (MIC_ARRAY_CONFIG_MIC_COUNT >> 1);
            buffering_channel_b++;
            if (buffering_channel_b > (MIC_ARRAY_CONFIG_MIC_COUNT - 1)) {
                buffering_channel_b = (MIC_ARRAY_CONFIG_MIC_COUNT >> 1);
            }
#endif

            sample_read_index = 0;
        }

        if ((num_samples + num_samples_buffered) > TOTAL_SAMPLES) {
            rtos_printf("BAD!\n");
        }

#if 1 // ENABLE TO CAPTURE MIC DATA.
        //rtos_mic_array_assertion_disable(); // TODO: Remove
        //rtos_printf("\n\n===========================\n\n");
        for (int s = 0; s < num_samples; s++)
        {
            //if ((s != 0) && (s % 8 == 0)) {
            //    rtos_printf("\n");
            //}
            //rtos_printf("%8d, ", ((int32_t *)output_audio_frames)[s] >> 16);
#if 1
            int dest_sample = num_samples_buffered + s;
#if SAMPLE_FORMAT == 0 //RTOS_MIC_ARRAY_CHANNEL_SAMPLE
            sample_buffer[0][dest_sample] = (SAMPLE_TYPE)(((int32_t *)output_audio_frames)[s + (buffering_channel_a * appconfAUDIO_PIPELINE_FRAME_ADVANCE)]);// >> 16);
            sample_buffer[1][dest_sample] = (SAMPLE_TYPE)(((int32_t *)output_audio_frames)[s + (buffering_channel_b * appconfAUDIO_PIPELINE_FRAME_ADVANCE)]);// >> 16);
#else
            #error NOT FUNCTIONAL
#endif
#endif
        }
        //rtos_printf("\n\n===========================\n\n");
#endif

        num_samples_buffered += num_samples;
    }

    return AUDIO_PIPELINE_FREE_FRAME;
}

void vApplicationMallocFailedHook(void)
{
    rtos_printf("Malloc Failed on tile %d!\n", THIS_XCORE_TILE);
    xassert(0);
    for(;;);
}

#define MEM_ANALYSIS_ENABLED 0
static void mem_analysis(void)
{
	for (;;) {
#if MEM_ANALYSIS_ENABLED
		rtos_printf("Tile[%d]:\n\tMinimum heap free: %d\n\tCurrent heap free: %d\n", THIS_XCORE_TILE, xPortGetMinimumEverFreeHeapSize(), xPortGetFreeHeapSize());
#endif
		vTaskDelay(pdMS_TO_TICKS(5000));
	}
}

void startup_task(void *arg)
{
    rtos_printf("Startup task running from tile %d on core %d\n", THIS_XCORE_TILE, portGET_CORE_ID());

#if ON_TILE(0)
    GenerateWaveform();

    for (int s = 0; s < TOTAL_SAMPLES_PER_CHANNEL; s++)
    {
#if SAMPLE_FORMAT == 0 // RTOS_MIC_ARRAY_CHANNEL_SAMPLE
            sample_buffer[2][s] = 0x5555;
#else
            sample_buffer[s][2] = 0x5555;
#endif
    }

    buffering_channel_a = 0;
    buffering_channel_b = MIC_ARRAY_CONFIG_MIC_COUNT >> 1;
#endif

    platform_start();

    audio_pipeline_init(NULL, NULL);

    mem_analysis();
    /*
     * TODO: Watchdog?
     */
}

void vApplicationMinimalIdleHook(void)
{
    rtos_printf("idle hook on tile %d core %d\n", THIS_XCORE_TILE, rtos_core_id_get());
    asm volatile("waiteu");
}

static void tile_common_init(chanend_t c)
{
    platform_init(c);
#if ON_TILE(USB_TILE_NO)
    rtos_mic_array_assertion_disable(); // TODO: Remove
#endif
    chanend_free(c);

#if appconfUSB_ENABLED && ON_TILE(USB_TILE_NO)
    usb_audio_init(intertile_ctx, appconfUSB_AUDIO_TASK_PRIORITY);
#endif

    xTaskCreate((TaskFunction_t) startup_task,
                "startup_task",
                RTOS_THREAD_STACK_SIZE(startup_task),
                NULL,
                appconfSTARTUP_TASK_PRIORITY,
                NULL);

    rtos_printf("start scheduler on tile %d\n", THIS_XCORE_TILE);
    vTaskStartScheduler();
}

#if ON_TILE(0)
void main_tile0(chanend_t c0, chanend_t c1, chanend_t c2, chanend_t c3)
{
    (void) c0;
    (void) c2;
    (void) c3;

    tile_common_init(c1);
}
#endif

#if ON_TILE(1)
void main_tile1(chanend_t c0, chanend_t c1, chanend_t c2, chanend_t c3)
{
    (void) c1;
    (void) c2;
    (void) c3;

    tile_common_init(c0);
}
#endif
