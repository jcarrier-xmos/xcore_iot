// Copyright (c) 2021-2022 XMOS LIMITED. This Software is subject to the terms of the
// XMOS Public License: Version 1

/* App headers */
#include "../app_conf.h"
#include "platform_init.h"

static void tile1_setup_dac(void);
static void tile1_i2s_init(void);
static void tile1_mic_init(void);
static void tile1_uart_init(void);

void platform_init_tile_1(chanend_t c_other_tile)
{
    memset(tile1_ctx, 0, sizeof(tile1_ctx_t));

    tile1_ctx->c_from_gpio = soc_channel_establish(c_other_tile, soc_channel_input);
    tile1_ctx->c_to_gpio = soc_channel_establish(c_other_tile, soc_channel_output);

    /* Reset CODEC */
    tile1_setup_dac();

    /* Signal codec reset complete */
    chanend_out_byte(c_other_tile, 0x00);

    /* Wait for DAC initialization to be complete */
    char ret_char = chanend_in_byte(c_other_tile);
    if (ret_char != 0) {
        debug_printf("DAC init failed on other tile\n");
    }

    app_pll_init();

    //tile1_mic_init();
    // Wait for other tile to init mic array
    ret_char = chanend_in_byte(c_other_tile);
    debug_printf("Starting I2S\n");

    tile1_i2s_init();
    tile1_uart_init();
}

static void tile1_setup_dac(void)
{
    const port_t codec_rst_port = PORT_CODEC_RST_N;
    port_enable(codec_rst_port);
    port_out(codec_rst_port, 0xF);
}

static int i2s_mclk_bclk_ratio(
        const unsigned audio_clock_frequency,
        const unsigned sample_rate)
{
    return audio_clock_frequency / (sample_rate * (8 * sizeof(int32_t)) * I2S_CHANS_PER_FRAME);
}

I2S_CALLBACK_ATTR
static void i2s_init(chanend_t *input_c, i2s_config_t *i2s_config)
{
    i2s_config->mode = I2S_MODE_I2S;
    i2s_config->mclk_bclk_ratio =  i2s_mclk_bclk_ratio(appconfAUDIO_CLOCK_FREQUENCY, appconfPIPELINE_AUDIO_SAMPLE_RATE);
}

I2S_CALLBACK_ATTR
static i2s_restart_t i2s_restart_check(chanend_t *input_c)
{
    return I2S_NO_RESTART;
}

I2S_CALLBACK_ATTR
static void i2s_receive(chanend_t *input_c, size_t num_in, const int32_t *i2s_sample_buf)
{
    return;
}

I2S_CALLBACK_ATTR
static void i2s_send(chanend_t *input_c, size_t num_out, int32_t *i2s_sample_buf)
{
    s_chan_in_buf_word(*input_c, (uint32_t*)i2s_sample_buf, MIC_ARRAY_CONFIG_MIC_COUNT);
}

static void tile1_i2s_init(void)
{
    tile1_ctx->i2s_cb_group.init = (i2s_init_t) i2s_init;
    tile1_ctx->i2s_cb_group.restart_check = (i2s_restart_check_t) i2s_restart_check;
    tile1_ctx->i2s_cb_group.receive = (i2s_receive_t) i2s_receive;
    tile1_ctx->i2s_cb_group.send = (i2s_send_t) i2s_send;
    tile1_ctx->i2s_cb_group.app_data = &tile1_ctx->c_i2s_to_dac;

    tile1_ctx->p_i2s_dout[0] = PORT_I2S_DAC_DATA;
    tile1_ctx->p_bclk = PORT_I2S_BCLK;
    tile1_ctx->p_lrclk = PORT_I2S_LRCLK;
    tile1_ctx->p_mclk = PORT_MCLK_IN;
    tile1_ctx->bclk = I2S_CLKBLK;
}

static void tile1_mic_init(void)
{
    ma_vanilla_init();
}

HIL_UART_RX_CALLBACK_ATTR
void uart_rx_error_callback(uart_callback_code_t callback_code, void *app_data){
    //debug_printf("uart_rx_error: 0x%x\n", callback_code);
}

static void tile1_uart_init(void)
{
    const unsigned baud_rate = 921600;

    hwtimer_t tmr_tx = hwtimer_alloc();
    uart_tx_init(
        &tile1_ctx->uart_tx_ctx,
        XS1_PORT_1P,  //X1D39
        baud_rate,
        8,
        UART_PARITY_NONE,
        1,
        tmr_tx,
        NULL,
        0,
        NULL,
        NULL
        );

    hwtimer_t tmr_rx = hwtimer_alloc();
    uart_rx_init(
        &tile1_ctx->uart_rx_ctx,
        XS1_PORT_1M, //X1D36
        baud_rate,
        8,
        UART_PARITY_NONE,
        1,
        tmr_rx,
        NULL, // No buffer
        0,
        NULL, // No rx complete callback
        uart_rx_error_callback,
        &tile1_ctx->uart_rx_ctx
        );
}

