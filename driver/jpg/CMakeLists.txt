add_definitions(
    -DCFG_ITP_PLATFORM
    # -DCFG_PAL_PLATFORM
)

if (CMAKE_BUILD_TYPE STREQUAL Debug)
    add_definitions(
        -DDEBUG
    )
endif()

include_directories(
    .
    ${SMEDIA_INC_DIRS}
    ${PROJECT_SOURCE_DIR}/sdk/include/jpg
    ${PROJECT_SOURCE_DIR}/sdk/include/isp
)

ADD_LIBRARY(jpg STATIC
    ${PROJECT_SOURCE_DIR}/sdk/include/jpg/ite_jpg.h
    ${PROJECT_SOURCE_DIR}/sdk/include/jpg/jpg_err.h
    ${PROJECT_SOURCE_DIR}/sdk/include/jpg/jpg_stream.h
    ite_jpg.c
    jpg_types.h
    jpg_codec.h
    jpg_common.c
    jpg_common.h
    jpg_config.h
    jpg_dec_mjpg.c
    jpg_dec_cmd.c
    jpg_decoder.c
    jpg_defs.h
    jpg_enc_mjpg.c
    jpg_encoder.c
    jpg_hw.c
    jpg_hw.h
    jpg_parser.c
    jpg_parser.h
    jpg_reg.h
    jpg_stream_file.c
    jpg_stream_mem.c
    register_template.h
    jpg_extern_link.c
    jpg_extern_link.h
)
