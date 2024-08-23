set(COMPONENT_SRCDIRS "${RG_BUILD_APP}")
set(COMPONENT_ADD_INCLUDEDIRS "${RG_BUILD_APP}")
register_component()
rg_setup_compile_options(-Wno-error=format -mfix-esp32-psram-cache-issue)
