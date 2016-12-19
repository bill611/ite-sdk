if (DEFINED CFG_CHIP_PKG_IT9910)
    include_directories(
        .
        ..
        ${SMEDIA_INC_DIRS}
        ${PROJECT_SOURCE_DIR}/sdk/include/capture
        ${PROJECT_SOURCE_DIR}/sdk/include/encoder
        ${PROJECT_SOURCE_DIR}/sdk/include
    )

    add_library(capture STATIC
        mmp_capture.c
        capture_hw.c
        capture.c
        cap_util.c
    )
if (SENSOR_FW_NAME STREQUAL OMNIVISION_OV7725)  
    add_library(capture STATIC
    	io_sensor_omnivision_ov7725.c
    )
elseif (SENSOR_FW_NAME STREQUAL PIXELPLUS_PO3100)
    add_library(capture STATIC
    	io_sensor_pixelplus_po3100.c
    )
elseif (SENSOR_FW_NAME STREQUAL AR0130)
    add_library(capture STATIC
    	io_sensor_ar0130.c
    )
endif()      
endif()