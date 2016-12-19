add_definitions(
    -DCFG_DHCPS_ADDR_COUNT=${CFG_DHCPS_ADDR_COUNT}
    )

add_library(dhcps STATIC
    dhcps.c
    dhcps.h
    )
