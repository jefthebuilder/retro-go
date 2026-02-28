cmake_minimum_required(VERSION 3.5)

set(COMPONENT_SRCDIRS "src")
set(COMPONENT_ADD_INCLUDEDIRS "include")
set(COMPONENT_REQUIRES retro-go esp_http_client)

register_component()
rg_setup_compile_options(-O2)
