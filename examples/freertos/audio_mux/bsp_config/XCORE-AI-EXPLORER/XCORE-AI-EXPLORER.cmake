
## Create custom board targets for application
add_library(xcore_iot_example_audio_mux_board_support_xcore_ai_explorer INTERFACE)
target_sources(xcore_iot_example_audio_mux_board_support_xcore_ai_explorer
    INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/platform/dac_port.c
        ${CMAKE_CURRENT_LIST_DIR}/platform/app_pll_ctrl.c
        ${CMAKE_CURRENT_LIST_DIR}/platform/driver_instances.c
        ${CMAKE_CURRENT_LIST_DIR}/platform/platform_init.c
        ${CMAKE_CURRENT_LIST_DIR}/platform/platform_start.c
)
target_include_directories(xcore_iot_example_audio_mux_board_support_xcore_ai_explorer
    INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}
)
target_link_libraries(xcore_iot_example_audio_mux_board_support_xcore_ai_explorer
    INTERFACE
        core::general
        rtos::freertos
        rtos::drivers::general
        rtos::drivers::audio
        rtos::drivers::usb
        rtos::freertos_usb
        xcore_iot::example::audio_mux::dac::aic3204
)
target_compile_options(xcore_iot_example_audio_mux_board_support_xcore_ai_explorer
    INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/XCORE-AI-EXPLORER.xn
)
target_link_options(xcore_iot_example_audio_mux_board_support_xcore_ai_explorer
    INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/XCORE-AI-EXPLORER.xn
)
target_compile_definitions(xcore_iot_example_audio_mux_board_support_xcore_ai_explorer
    INTERFACE
        XCOREAI_EXPLORER=1
        PLATFORM_SUPPORTS_TILE_0=1
        PLATFORM_SUPPORTS_TILE_1=1
        PLATFORM_SUPPORTS_TILE_2=0
        PLATFORM_SUPPORTS_TILE_3=0
        USB_TILE_NO=0
        USB_TILE=tile[USB_TILE_NO]

        MIC_ARRAY_CONFIG_MCLK_FREQ=24576000
        MIC_ARRAY_CONFIG_PDM_FREQ=3072000
        MIC_ARRAY_CONFIG_SAMPLES_PER_FRAME=240
        MIC_ARRAY_CONFIG_USE_DDR=1
        MIC_ARRAY_CONFIG_CLOCK_BLOCK_A=XS1_CLKBLK_3
        MIC_ARRAY_CONFIG_CLOCK_BLOCK_B=XS1_CLKBLK_2
        MIC_ARRAY_CONFIG_PORT_MCLK=PORT_MCLK_IN_ALT
        MIC_ARRAY_CONFIG_PORT_PDM_CLK=PORT_PDM_CLK
        MIC_ARRAY_CONFIG_PORT_PDM_DATA=PORT_PDM_DATA

        # When changing MIC count, update PORT_PDM_DATA (in XCORE-AI-EXPLORER.cmake) to match
        # 2 MIC (DDR) = XS1_PORT_1F (only 1v8 compatible)
        # 8 MIC (DDR) = XS1_PORT_4C or XS1_PORT_4D
        # 16 MIC (DDR) = XS1_PORT_8B

         MIC_ARRAY_CONFIG_MIC_COUNT=2
        # MIC_ARRAY_CONFIG_MIC_COUNT=8
        # MIC_ARRAY_CONFIG_MIC_COUNT=16
)

## Create an alias
add_library(xcore_iot::example::audio_mux::xcore_ai_explorer ALIAS xcore_iot_example_audio_mux_board_support_xcore_ai_explorer)
