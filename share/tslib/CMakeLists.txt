if (DEFINED CFG_TOUCH_ENABLE)

    add_definitions(
        -DTS_CONF="${CFG_PRIVATE_DRIVE}:/ts.conf"
        -DTS_POINTERCAL="${CFG_PUBLIC_DRIVE}:/pointercal"
        -DPLUGIN_DIR="${CFG_PRIVATE_DRIVE}:"
        )
            
    if (CMAKE_BUILD_TYPE STREQUAL Debug)
        add_definitions(
            -DDEBUG
            )
    endif()
    
    include_directories(
        .
        src
        )
    
    add_library(tslib STATIC
        src/ts_attach.c 
        src/ts_close.c 
        src/ts_config.c 
        src/ts_error.c
        src/ts_fd.c 
        src/ts_load_module.c 
        src/ts_open.c 
        src/ts_parse_vars.c
        src/ts_read.c 
        src/ts_read_raw.c 
        src/ts_option.c
        plugins/dejitter.c
        plugins/linear.c
        plugins/pthres.c
        plugins/variance.c
        plugins/${CFG_TOUCH_MODULE}-raw.c
        )
    
        configure_file(
            ${PROJECT_SOURCE_DIR}/sdk/target/touch/${CFG_TOUCH_MODULE}.conf 
            ${CMAKE_BINARY_DIR}/data/private/ts.conf
            NEWLINE_STYLE UNIX
            )
endif()
