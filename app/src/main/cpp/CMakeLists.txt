cmake_minimum_required(VERSION 3.22.1)
project(picowalker C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

set(CORE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/core)

add_library(picowalker SHARED
    # ---- Android driver layer ----
    jni_bridge.c
    runner.c
    dr_screen.c
    dr_eeprom.c
    dr_time.c
    dr_buttons.c
    dr_accel.c
    dr_audio.c
    dr_power.c
    dr_ir.c
    dr_flash.c
    dr_log.c

    # ---- embedded ROM assets ----
    assets/flash_images_data.c
    assets/fancy_picowalker.c
    assets/fancy_border.c
    assets/fancy_battery.c
    assets/fancy_percent.c
    assets/fancy_color.c
    assets/fancy_grey.c

    # ---- picowalker-core (platform agnostic) ----
    ${CORE_DIR}/picowalker.c
    ${CORE_DIR}/buttons.c
    ${CORE_DIR}/states.c
    ${CORE_DIR}/menu.c
    ${CORE_DIR}/screen.c
    ${CORE_DIR}/audio.c
    ${CORE_DIR}/globals.c
    ${CORE_DIR}/utils.c
    ${CORE_DIR}/eeprom.c
    ${CORE_DIR}/rand.c
    ${CORE_DIR}/accel.c
    ${CORE_DIR}/power.c
    ${CORE_DIR}/timer.c
    ${CORE_DIR}/event_log.c
    ${CORE_DIR}/debug_log.c
    ${CORE_DIR}/ir/compression.c
    ${CORE_DIR}/ir/ir.c
    ${CORE_DIR}/ir/actions.c
    ${CORE_DIR}/apps/app_splash.c
    ${CORE_DIR}/apps/app_trainer_card.c
    ${CORE_DIR}/apps/app_inventory.c
    ${CORE_DIR}/apps/app_dowsing.c
    ${CORE_DIR}/apps/app_poke_radar.c
    ${CORE_DIR}/apps/app_battle.c
    ${CORE_DIR}/apps/app_comms.c
    ${CORE_DIR}/apps/app_settings.c
    ${CORE_DIR}/apps/app_switch.c
    ${CORE_DIR}/apps/app_picowalker.c
)

target_include_directories(picowalker PRIVATE
    ${CORE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_options(picowalker PRIVATE
    -Wall
    -Wextra
    -Wno-unused-parameter
    -fno-strict-aliasing
)

find_library(log-lib log)
find_library(aaudio-lib aaudio)

target_link_libraries(picowalker
    ${log-lib}
    ${aaudio-lib}
)
