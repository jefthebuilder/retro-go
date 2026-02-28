idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES retro-go esp_http_client json nvs_flash app_update
)

# Use retro-go's compile options helper
rg_setup_compile_options(-O2)

# Optional: Add additional compile options
target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wno-error
    -Wno-unused-function
    -Wno-unused-variable
)
