/*
 * Copyright (c) 2016 ITE Tech. Inc. All Rights Reserved.
 */
/** @file
 *  Use as compile option header file.
 *
 * @author Irene Lin
 * @version 1.0
 */
#ifndef	GMAC_CONFIG_H
#define	GMAC_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif


//=============================================================================
//                              Compile Option
//=============================================================================
//#define CLK_FROM_PHY
#define MULTICAST
//#define MAC_CRC_DIS
#if defined(CFG_NET_ETHERNET_LINK_INTR)
    #define LINK_INTR
#endif


//=============================================================================
//                              Constant Definition
//=============================================================================

#define RX_QUEUE_ENTRIES    	256	/* must be power of 2 */
#define TX_QUEUE_ENTRIES    	512	/* must be power of 2 */

#define MAX_PKT_SIZE		    1518
#define RX_BUF_SIZE		    1600//2044	/* must be smaller than 0x3fff */

#if MAX_PKT_SIZE > 0x3fff
#error invalid MAX_PKT_SIZE
#endif

#if RX_BUF_SIZE > 0x3fff
#error invalid RX_BUF_SIZE
#endif

//=============================================================================
//                              LOG definition
//=============================================================================
// Log definitions
typedef enum
{
    MAC_ZONE_ERROR      = (0x1 << 0),
    MAC_ZONE_WARNING    = (0x1 << 1),
    MAC_ZONE_INFO       = (0x1 << 2),
    MAC_ZONE_DEBUG      = (0x1 << 3),
    MAC_ZONE_ENTER      = (0x1 << 4),
    MAC_ZONE_LEAVE      = (0x1 << 5),
    MAC_ZONE_ALL        = 0xFFFFFFFF
} MacLogZones;

#if 1
#define MAC_LOG_ZONES    (MAC_ZONE_ERROR | MAC_ZONE_WARNING)
//#define MAC_LOG_ZONES    (MAC_ZONE_ALL)

#define LOG_ERROR   ((void) ((MAC_ZONE_ERROR & MAC_LOG_ZONES) ? (printf("[MAC][ERROR]"
#define LOG_WARNING ((void) ((MAC_ZONE_WARNING & MAC_LOG_ZONES) ? (printf("[MAC][WARNING]"
#define LOG_INFO    ((void) ((MAC_ZONE_INFO & MAC_LOG_ZONES) ? (printf("[MAC][INFO]"
#define LOG_DEBUG   ((void) ((MAC_ZONE_DEBUG & MAC_LOG_ZONES) ? (printf("[MAC][DEBUG]"
#define LOG_ENTER   ((void) ((MAC_ZONE_ENTER & MAC_LOG_ZONES) ? (printf("[MAC][ENTER]"
#define LOG_LEAVE   ((void) ((MAC_ZONE_LEAVE & MAC_LOG_ZONES) ? (printf("[MAC][LEAVE]"
#define LOG_DATA    ((void) ((MAC_TRUE) ? (printf(
#define LOG_INFO2   ((void) ((MAC_TRUE) ? (printf(
#define LOG_END     )), 1 : 0));
#else
#define LOG_ZONES
#define LOG_ERROR
#define LOG_WARNING
#define LOG_INFO
#define LOG_DEBUG
#define LOG_ENTER
#define LOG_LEAVE
#define LOG_DATA
#define LOG_INFO2
#define LOG_END         ;
#endif


#define check_result(rc) do { if (rc) LOG_ERROR "[%s] res = %d(0x%08X) \n", __FUNCTION__, rc, rc LOG_END } while (0)



#ifdef __cplusplus
}
#endif

#endif
