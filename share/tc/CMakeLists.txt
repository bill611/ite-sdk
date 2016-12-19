if (DEFINED CFG_BUILD_TC)
    add_definitions(
        -DBUILD_TC
        )
endif()
if (DEFINED CFG_TC3000_BZ)
    add_definitions(
        -DTC3000_BZ
        )
endif()
if (DEFINED CFG_TC3000_FL)
    add_definitions(
        -DTC3000_FL
        )
endif()
if (DEFINED CFG_TC3000_WM)
    add_definitions(
        -DTC3000_WM
        )
endif()
include_directories(
    ${PROJECT_SOURCE_DIR}/sdk/include/
    )

add_library(tc STATIC
    adpcm.c
	#rtpObject.c
    )


