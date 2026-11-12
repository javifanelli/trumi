idf_component_register(SRCS "cerebro.c"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_http_client wifi)

# cerebro_secretos.h no se sube al repo, asi que puede aparecer DESPUES de una
# compilacion. CONFIGURE_DEPENDS hace que en cada build se revise si el archivo
# aparecio o desaparecio, y que cerebro.c se recompile si cambia.
file(GLOB secretos CONFIGURE_DEPENDS "${COMPONENT_DIR}/cerebro_secretos.h")
if(secretos)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE TRUMI_HAY_SECRETOS_CEREBRO=1)
    set_source_files_properties(cerebro.c PROPERTIES OBJECT_DEPENDS "${secretos}")
endif()
