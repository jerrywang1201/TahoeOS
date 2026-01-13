cmake_minimum_required(VERSION 3.20)
project(IAP_F411 C ASM)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(LINKER_SCRIPT ${CMAKE_CURRENT_LIST_DIR}/linker/STM32F411CEUx_BOOT.ld)
set(STARTUP_FILE ${CMAKE_CURRENT_LIST_DIR}/startup_stm32f411xe_gcc.s)

file(GLOB_RECURSE APP_SOURCES CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_LIST_DIR}/Core/Src/*.c
  ${CMAKE_CURRENT_LIST_DIR}/BSP/**/*.c
  ${CMAKE_CURRENT_LIST_DIR}/SYSTEM/*.c
  ${CMAKE_CURRENT_LIST_DIR}/Ymodem/*.c
  ${CMAKE_CURRENT_LIST_DIR}/Drivers/STM32F4xx_HAL_Driver/Src/*.c
)

add_executable(${PROJECT_NAME}.elf
  ${APP_SOURCES}
  ${STARTUP_FILE}
)

target_include_directories(${PROJECT_NAME}.elf PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/Core/Inc
  ${CMAKE_CURRENT_LIST_DIR}/BSP
  ${CMAKE_CURRENT_LIST_DIR}/BSP/LCD
  ${CMAKE_CURRENT_LIST_DIR}/BSP/KEY
  ${CMAKE_CURRENT_LIST_DIR}/BSP/POWER
  ${CMAKE_CURRENT_LIST_DIR}/BSP/KT6328
  ${CMAKE_CURRENT_LIST_DIR}/SYSTEM
  ${CMAKE_CURRENT_LIST_DIR}/Ymodem
  ${CMAKE_CURRENT_LIST_DIR}/Drivers/STM32F4xx_HAL_Driver/Inc
  ${CMAKE_CURRENT_LIST_DIR}/Drivers/CMSIS/Include
  ${CMAKE_CURRENT_LIST_DIR}/Drivers/CMSIS/Device/ST/STM32F4xx/Include
)

target_compile_definitions(${PROJECT_NAME}.elf PRIVATE
  USE_HAL_DRIVER
  STM32F411xE
)

target_compile_options(${PROJECT_NAME}.elf PRIVATE
  -mcpu=cortex-m4
  -mthumb
  -mfpu=fpv4-sp-d16
  -mfloat-abi=hard
  -ffunction-sections
  -fdata-sections
  -Wall
  -Wextra
  -Wno-unused-parameter
)

target_link_options(${PROJECT_NAME}.elf PRIVATE
  -T${LINKER_SCRIPT}
  -mcpu=cortex-m4
  -mthumb
  -mfpu=fpv4-sp-d16
  -mfloat-abi=hard
  -Wl,--gc-sections
  -Wl,-Map=${PROJECT_NAME}.map
  -specs=nosys.specs
  -specs=nano.specs
)

add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
  COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:${PROJECT_NAME}.elf> ${PROJECT_NAME}.hex
  COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${PROJECT_NAME}.elf> ${PROJECT_NAME}.bin
  COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${PROJECT_NAME}.elf>
)
