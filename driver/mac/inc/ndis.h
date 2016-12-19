/*
 * Copyright (c) 2011 ITE Tech. Inc. All Rights Reserved.
 */
/** @file
 *
 * @author Irene Lin
 * @version 1.0
 */

#ifndef MAC_NDIS_H
#define MAC_NDIS_H


#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
//                              Include Files
//=============================================================================
#include "ite/ith.h"


//=============================================================================
//                              Constant Definition
//=============================================================================
#define ETH_ZLEN	60		/* Min. octets in frame sans FCS */

//=============================================================================
//                              Structure Definition
//=============================================================================
struct net_device_stats {
    uint32_t	rx_packets;
    uint32_t	tx_packets;
    uint32_t	rx_bytes;
    uint32_t	tx_bytes;
    uint32_t	rx_errors;
#if 0
    uint32_t	tx_errors;
#endif
    uint32_t	rx_dropped;
    uint32_t	tx_dropped;
    uint32_t	multicast;
#if 0
    uint32_t	collisions;
#endif
    uint32_t	rx_length_errors;
    uint32_t	rx_over_errors;
    uint32_t	rx_crc_errors;
#if 0
    uint32_t	rx_frame_errors;
#endif
    uint32_t	rx_fifo_errors;
#if 0
    uint32_t	rx_missed_errors;
#endif
    uint32_t	tx_aborted_errors;
#if 0
    uint32_t	tx_carrier_errors;
    uint32_t	tx_fifo_errors;
    uint32_t	tx_heartbeat_errors;
    uint32_t	tx_window_errors;
    uint32_t	rx_compressed;
    uint32_t	tx_compressed;
#endif
};

struct eth_device 
{
    struct net_device_stats stats;
    void *priv;

#define LINK_STATE_START        (1<<0)
#define LINK_STATE_NOCARRIER    (1<<1)
#define TX_QUEUE_XOFF           (1<<2)
    uint32_t state;
    uint8_t  netaddr[6];
};

#define netif_start_queue		    NETIF_TX_QUEUE_ON
#define netif_stop_queue		    NETIF_TX_QUEUE_OFF
#define netif_queue_stopped(dev)	(dev->state & TX_QUEUE_XOFF)
#define netif_wake_queue		    NETIF_TX_QUEUE_ON

#define netif_start					NETIF_START
#define netif_stop   				NETIF_STOP
#define netif_running				NETIF_RUNNING


#define NETIF_START(x)          do { ((x)->state) |= LINK_STATE_START; } while(0)
#define NETIF_STOP(x)           do { ((x)->state) &= ~LINK_STATE_START; } while(0)
#define NETIF_RUNNING(x)        (((x)->state) & LINK_STATE_START)

#define NETIF_CARRIER_OK(x)     (!(((x)->state) & LINK_STATE_NOCARRIER))
#define NETIF_CARRIER_ON(x)     do { ((x)->state) &= ~LINK_STATE_NOCARRIER; } while(0)
#define NETIF_CARRIER_OFF(x)    do { ((x)->state) |= LINK_STATE_NOCARRIER; } while(0)

#define NETIF_TX_QUEUE_OFF(x)   do { ((x)->state) |= TX_QUEUE_XOFF; } while(0)
#define NETIF_TX_QUEUE_ON(x)    do { ((x)->state) &= ~TX_QUEUE_XOFF; } while(0)
#define NETIF_TX_QUEUE_OK(x)    (!(((x)->state) & TX_QUEUE_XOFF))

//=============================================================================
//                              Macro Definition
//=============================================================================

//=============================================================================
//							Funtion Declaration
//=============================================================================


#ifdef __cplusplus
}
#endif

#endif //MAC_NDIS_H
