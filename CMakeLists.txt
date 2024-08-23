cmake_minimum_required(VERSION 3.5)

if("${RG_BUILD_APP}" STREQUAL "gwenesis")
set(COMPONENTS "main retro-go gwenesis app_trace bootloader esptool_py")
elseif("${RG_BUILD_APP}" STREQUAL "fmsx")
set(COMPONENTS "main retro-go fmsx app_trace bootloader esptool_py")
elseif("${RG_BUILD_APP}" STREQUAL "prboom-go")
set(COMPONENTS "main retro-go prboom app_trace bootloader esptool_py")
elseif("${RG_BUILD_APP}" STREQUAL "retro-core")
set(COMPONENTS "main retro-go nofrendo gnuboy pce-go gw-emulator handy smsplus snes9x app_trace bootloader esptool_py")
else()
set(RG_BUILD_APP "launcher")
set(COMPONENTS "main retro-go app_trace bootloader wifi esp_http_server esptool_py")
endif()

macro(rg_setup_compile_options)
    set(RG_TARGET "RG_TARGET_${RG_BUILD_TARGET}")
    message("Target: ${RG_TARGET}")

    component_compile_options(
        -D${RG_TARGET}
        -DRETRO_GO
        ${ARGV}
    )

    if(RG_ENABLE_NETPLAY)
        component_compile_options(-DRG_ENABLE_NETWORKING -DRG_ENABLE_NETPLAY)
    elseif(RG_ENABLE_NETWORKING)
        component_compile_options(-DRG_ENABLE_NETWORKING)
    endif()

    if(RG_ENABLE_PROFILING)
        # Still debating whether -fno-inline is necessary or not...
        component_compile_options(-DRG_ENABLE_PROFILING -finstrument-functions)
    endif()
endmacro()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(${RG_BUILD_APP})
