/*
 * Copyright (c) 2016 ITE Tech. Inc. All Rights Reserved.
 */
/** @file
 *  Gigabit Ethernet Controller private data header file.
 *
 * @author Irene Lin
 * @version 1.0
 */

#ifndef GMAC_H
#define GMAC_H

#ifdef __cplusplus
extern "C" {
#endif

#include "linux/spinlock.h"
#include "mii.h"
#include "ndis.h"

/* private data */
struct ftgmac100_descs {
    struct ftgmac100_rxdes rxdes[RX_QUEUE_ENTRIES];
    struct ftgmac100_txdes txdes[TX_QUEUE_ENTRIES];
};

struct ftgmac100 {
    unsigned int base;

    struct ftgmac100_descs *descs;

    unsigned int rx_pointer;
    unsigned int tx_clean_pointer;
    unsigned int tx_pointer;
    unsigned int tx_pending;

    spinlock_t tx_lock;

    struct eth_device *netdev;
    struct mii_if_info mii;
    int old_speed;

    unsigned int autong_complete : 1;	/* Auto-Negotiate completed */
    unsigned int mode : 3;            /* real, mac lb, phy pcs lb, phy mdi lb */
    unsigned int is_grmii : 1;         /* is giga ethernet or fast ethernet */
    ITE_MAC_CFG_T* cfg;

    unsigned int      rx_flag;   /* see <if.h> IFF_PROMISC, IFF_ALLMULTI, IFF_MULTICAST */
    int               mc_count;  /* Multicast mac addresses count */
    unsigned int      mc_ht0;    /* Multicast hash table 0 */
    unsigned int      mc_ht1;    /* Multicast hash table 1 */

    void(*netif_rx)(void *arg, void *packet, int len);
    void*           netif_arg;
};

static int ftgmac100_alloc_rx_page(struct ftgmac100 *priv, struct ftgmac100_rxdes *rxdes);


#ifdef __cplusplus
}
#endif

#endif // GMAC_H
