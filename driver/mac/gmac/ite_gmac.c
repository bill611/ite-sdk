/*
 * Copyright (c) 2016 ITE Tech. Inc. All Rights Reserved.
 */
/** @file
 *  Gigabit Ethernet Controller extern API implementation.
 *
 * @author Irene Lin
 * @version 1.0
 */
#include <malloc.h>
#include <stdio.h>
#include "ite/ith.h"
#include "ite/itp.h"
#include "ite/ite_mac.h"
#include "linux/os.h"
#include "ndis.h"
#include "mii.h"
#include "gmac/config.h"
#include "gmac/gmac_reg.h"
#include "gmac/gmac.h"
#include "gmac/gmac_err.h"

//=============================================================================
//                              Global Data Definition
//=============================================================================
static struct eth_device eth_dev;
static struct eth_device *dev = NULL;


//=============================================================================
//                              Private Function Definition
//=============================================================================

static void MacConfig(struct ftgmac100 *priv, const uint8_t* ioConfig)
{
    int i, count=0;
    int io_cnt = priv->is_grmii ? ITE_MAC_GRMII_PIN_CNT : ITE_MAC_RMII_PIN_CNT;
    bool use_gpio34 = false;

    for (i = 0; i<io_cnt; i++) {
        ithGpioSetMode(ioConfig[i], ITH_GPIO_MODE2);

        if (ioConfig[i] == 34)
            use_gpio34 = true;
    }

    if (!priv->is_grmii) {
        if (use_gpio34)
            ;  // TODO: fast ethernet choose GPIO30-33 or GPIO34-37
    }

    /* for link gpio */
    if(priv->cfg->linkGpio>0)
        ithGpioSetMode(priv->cfg->linkGpio, ITH_GPIO_MODE0);

    /* enable gmac controller's clock setting */
    // TODO
}

static void phy_read_mode(struct eth_device *netdev, int* speed, int* duplex)
{
    struct ftgmac100 *priv = netdev->priv;
    struct mii_if_info *mii_if = &priv->mii;
    uint32_t status;
    int i;

    if (priv->cfg->phy_read_mode) {
        status = priv->cfg->phy_read_mode(speed, duplex);
        if (status) /* not link */
            return;
    } else {
        for (i = 0; i < 2; i++)
            status = mii_if->mdio_read(netdev, mii_if->phy_id, MII_BMSR);

        if (!(status & BMSR_LSTATUS))
            return;

        *speed = SPEED_10;
        *duplex = DUPLEX_HALF;

        /* AutoNegotiate completed */
#if 0
        {
            MAC_UINT16 autoadv, autorec;
            autoadv = mii_if->mdio_read(netdev, mii_if->phy_id, MII_ADVERTISE);
            autorec = mii_if->mdio_read(netdev, mii_if->phy_id, MII_LPA);
            status = autoadv & autorec;

            if (status & (ADVERTISE_100HALF | ADVERTISE_100FULL))
                *speed = SPEED_100;
            if (status & (ADVERTISE_100FULL | ADVERTISE_10FULL))
                *duplex = DUPLEX_FULL;
        }
#else
        {
            uint32_t ctrl;
            ctrl = mii_if->mdio_read(netdev, mii_if->phy_id, MII_BMCR);
            if (ctrl & BMCR_SPEED100)
                *speed = SPEED_100;
            if (ctrl & BMCR_FULLDPLX)
                *duplex = DUPLEX_FULL;
        }
#endif
    }

    priv->autong_complete = 1;

    ithPrintf(" Link On %s %s \n",
                    *speed == SPEED_100 ? "100Mbps" : "10Mbps",
                    *duplex == DUPLEX_FULL ? "full" : "half");
}

#include "gmac_hw.c"
#include "gmac_desc.c"
#include "gmac_rx.c"
#include "gmac_tx.c"
#include "gmac_buf.c"
#include "gmac_mdio.c"
#include "gmac_intr.c"
#include "gmac_netdev_ops.c"

//=============================================================================
//                              Public Function Definition
//=============================================================================
int iteMacInitialize(ITE_MAC_CFG_T* cfg)
{
    int rc=0;
    struct ftgmac100 *priv;
    LOG_ENTER "[%s]  \n", __FUNCTION__ LOG_END

    if(dev)
        return rc;

    dev = &eth_dev;
    memset(dev, 0, sizeof(struct eth_device));
    priv = (struct ftgmac100*)malloc(sizeof(struct ftgmac100));
    if (!priv) {
        rc = ERROR_MAC_PRIV_MEM_ALLOC_FAIL;
        goto end;
    }
    memset((void*)priv, 0, sizeof(struct ftgmac100));
    //priv->base = ITH_ETHERNET_BASE;
    priv->base = 0xC0800000;   // TODO
    priv->netdev = dev;
    spin_lock_init(&priv->tx_lock);

    /* initialize struct mii_if_info */
    priv->mii.phy_id	= cfg->phyAddr;
    priv->mii.phy_id_mask	= 0x1f;
    priv->mii.reg_num_mask	= 0x1f;
    priv->mii.dev		= &eth_dev;
    priv->mii.mdio_read = ftgmac100_mdiobus_read;
    priv->mii.mdio_write = ftgmac100_mdiobus_write;
    priv->cfg = cfg;
    priv->is_grmii = cfg->flags & ITE_MAC_GRMII;
    printf("giga ethernet: %d\n", priv->is_grmii);
    dev->priv = priv;
    NETIF_CARRIER_OFF(dev);

    MacConfig(priv, cfg->ioConfig);

#if defined(CLK_FROM_PHY)
    ithWriteRegMaskA(priv->base + FTGMAC100_OFFSET_MACCR, MAC_REF_CLK_FROM_PHY, MAC_REF_CLK_FROM_PHY);
#endif

end:
    check_result(rc);
    LOG_LEAVE "[%s]  \n", __FUNCTION__ LOG_END
    return rc;
}

static int inited = 0;

int iteMacOpen(uint8_t* mac_addr, void (*func)(void *arg, void *packet, int len), void* arg, int mode)
{
    int rc=0;
    struct ftgmac100 *priv;
    LOG_ENTER "[%s]  \n", __FUNCTION__ LOG_END

    if(!dev)
        return rc;

    priv = dev->priv;
    priv->netif_rx = func;
    priv->netif_arg = arg;
    priv->mode = mode;
    memset((void*)&dev->stats, 0x0, sizeof(struct net_device_stats));
    printf(" eth mode: %d \n", mode);
    if(mac_addr)
        memcpy(mac_addr, dev->netaddr, 6);

    if (inited)
        return rc;

    if(rc=ftgmac100_open(dev, 1))
        LOG_ERROR "[%s] rc = 0x%08X \n", __FUNCTION__, rc LOG_END

    inited = 1;

    LOG_LEAVE "[%s]  \n", __FUNCTION__ LOG_END
    return rc;
}

int iteMacStop(void)
{
    int rc=0;
    LOG_ENTER "[%s]  \n", __FUNCTION__ LOG_END

    if(!dev)
        return rc;

    if(rc=ftgmac100_stop(dev, 1))
        LOG_ERROR "[%s] rc = 0x%08X \n", __FUNCTION__, rc LOG_END

    LOG_LEAVE "[%s]  \n", __FUNCTION__ LOG_END
    return rc;
}

int iteMacSend(void* packet, uint32_t len)
{
    int rc=0;
    LOG_ENTER "[%s] packet 0x%08X, len %d \n", __FUNCTION__, packet, len LOG_END

    if(!dev)
        return rc;

    if(!NETIF_RUNNING(dev))
        return -1;

    if(!NETIF_CARRIER_OK(dev))
    {
        LOG_INFO " tx: no carrier \n" LOG_END
        return -1;
    }

    while(!NETIF_TX_QUEUE_OK(dev)) /* wait for tx queue available */
    {
        printf("&\n");
  	    udelay(50);
    }

    if(rc=ftgmac100_hard_start_xmit(packet, len, dev))
        LOG_ERROR "[%s] rc = 0x%08X \n", __FUNCTION__, rc LOG_END

    LOG_LEAVE "[%s]  \n", __FUNCTION__ LOG_END
    return rc;
}

int iteMacIoctl(struct mii_ioctl_data* data, int cmd)
{
    int rc=0;

    if(!dev)
        return rc;

    if(rc=ftgmac100_do_ioctl(dev, data, cmd))
        LOG_ERROR "[%s] rc = 0x%08X \n", __FUNCTION__, rc LOG_END
    return rc;
}

int iteMacSetMacAddr(uint8_t* mac_addr)
{
    int rc=0;
    LOG_ENTER "[%s]  \n", __FUNCTION__ LOG_END

    if(!dev)
    {
        rc = ERROR_MAC_NO_DEV;
        goto end;
    }
    if(NETIF_RUNNING(dev))
    {
        rc = ERROR_MAC_DEV_BUSY;
        goto end;
    }

    memcpy(dev->netaddr, mac_addr, 6);

end:
    check_result(rc);

    LOG_LEAVE "[%s]  \n", __FUNCTION__ LOG_END
    return rc;
}

int iteMacSetRxMode(int flag, uint8_t* mc_list, int count)
{
    int rc=0;
    LOG_ENTER "[%s]  \n", __FUNCTION__ LOG_END

    if(!dev)
    {
        rc = ERROR_MAC_NO_DEV;
        goto end;
    }

    ithEnterCritical();
    rc = ftgmac100_set_rx_mode(dev, flag, mc_list, count);
    ithExitCritical();

end:
    check_result(rc);

    LOG_LEAVE "[%s]  \n", __FUNCTION__ LOG_END
    return rc;
}

int iteMacGetStats(uint32_t* tx_packets, uint32_t* tx_bytes, uint32_t* rx_packets, uint32_t* rx_bytes)
{
    int rc=0;

    if(!dev)
    {
        rc = ERROR_MAC_NO_DEV;
        goto end;
    }
    if(tx_packets) (*tx_packets) = dev->stats.tx_packets;
    if(tx_bytes)   (*tx_bytes)   = dev->stats.tx_bytes;
    if(rx_packets) (*rx_packets) = dev->stats.rx_packets;
    if(rx_bytes)   (*rx_bytes)   = dev->stats.rx_bytes;

end:
    check_result(rc);
    return rc;
}

int iteMacSuspend(void)
{
    int rc=0;
    LOG_ENTER "[%s]  \n", __FUNCTION__ LOG_END

    if(!dev)
        return rc;
    
    if(rc=ftgmac100_stop(dev, 0))
        LOG_ERROR "[%s] rc = 0x%08X \n", __FUNCTION__, rc LOG_END

    LOG_LEAVE "[%s]  \n", __FUNCTION__ LOG_END
    return rc;
}

int iteMacResume(void)
{
    int rc=0;
    LOG_ENTER "[%s]  \n", __FUNCTION__ LOG_END

    if(!dev)
        return rc;

    if(rc=ftgmac100_open(dev, 0))
        LOG_ERROR "[%s] rc = 0x%08X \n", __FUNCTION__, rc LOG_END

end:
    LOG_LEAVE "[%s]  \n", __FUNCTION__ LOG_END
    return rc;
}

void iteMacSetClock(int clk_inv, int delay)
{
    struct ftgmac100 *priv = dev->priv;

    uint32_t mask = MAC_REF_CLK_INVERT | (0xFF << 20);
    uint32_t val = clk_inv ? MAC_REF_CLK_INVERT : 0;

    val |= MAC_REF_CLK_DELAY(delay);

    ithWriteRegMaskA(priv->base + FTGMAC100_OFFSET_MACCR, mask, val);
}

//=============================================================================
//  APIs for alter and report network device settings.
//=============================================================================
/**
 * is link status up/ok
 *
 * Returns 1 if the device reports link status up/ok, 0 otherwise.
 */
int iteEthGetLink(void)
{
    struct ftgmac100 *priv;

    if(!dev)
        return 0;

    if(!NETIF_RUNNING(dev))
        return 0;

    priv = (struct ftgmac100 *)dev->priv;
    
    if(priv->mode == ITE_ETH_MAC_LB)
        return 1;

#if !defined(LINK_INTR)
    {   // polling from application layer
        int cur_link;
        int prev_link = NETIF_CARRIER_OK(dev);

        if (priv->cfg->phy_link_status)
            cur_link = priv->cfg->phy_link_status();
        else
            cur_link = mii_link_ok(&priv->mii);

        if (cur_link && !prev_link)
        {
            ftgmac100_start_hw(priv);
            NETIF_CARRIER_ON(dev);
        }
        else if (prev_link && !cur_link)
        {
            printf("xxxxx\n");
            NETIF_CARRIER_OFF(dev);
        }
    }
#else
    if (priv->cfg->phy_link_status)
    {
        int cur_link;
        int prev_link = NETIF_CARRIER_OK(dev);

        cur_link = priv->cfg->phy_link_status();
        if (cur_link && !prev_link)
        {
            ftgmac100_start_hw(priv);
            NETIF_CARRIER_ON(dev);
        }
        else if (prev_link && !cur_link)
        {
            printf("xxxxx\n");
            NETIF_CARRIER_OFF(dev);
        }
    }
#endif

    return NETIF_CARRIER_OK(dev) ? 1 : 0;
}

int iteEthGetLink2(void)
{
    struct ftgmac100 *priv;

    if(!dev)
        return 0;
	
    priv = (struct ftgmac100 *)dev->priv;

    if (priv->cfg->phy_link_status)
        return priv->cfg->phy_link_status();

	return mii_link_ok(&priv->mii);
}

/**
 * get settings that are specified in @ecmd
 *
 * @ecmd requested ethtool_cmd
 *
 * Returns 0 for success, non-zero on error.
 */
int iteEthGetSetting(struct ethtool_cmd *ecmd)
{
    int rc=0;
    struct ftgmac100 *priv;

    if(!dev)
        return 0;

    priv = (struct ftgmac100 *)dev->priv;
    if(rc=mii_ethtool_gset(&priv->mii, ecmd))
        LOG_ERROR "[%s] rc = 0x%08X \n", __FUNCTION__, rc LOG_END
    return rc;
}

/**
 * set settings that are specified in @ecmd
 *
 * @ecmd requested ethtool_cmd
 *
 * Returns 0 for success, non-zero on error.
 */
int iteEthSetSetting(struct ethtool_cmd *ecmd)
{
    int rc=0;
    struct ftgmac100 *priv;

    if(!dev)
        return 0;

    priv = (struct ftgmac100 *)dev->priv;
    if(rc=mii_ethtool_sset(&priv->mii, ecmd))
        LOG_ERROR "[%s] rc = 0x%08X \n", __FUNCTION__, rc LOG_END
    return rc;
}

/**
 * restart NWay (autonegotiation) for this interface
 *
 * Returns 0 for success, non-zero on error.
 */
int iteEthNWayRestart(void)
{
    int rc=0;
    struct ftgmac100 *priv;

    if(!dev)
        return 0;

    priv = (struct ftgmac100 *)dev->priv;
    if(rc=mii_nway_restart(&priv->mii))
        LOG_ERROR "[%s] rc = 0x%08X \n", __FUNCTION__, rc LOG_END
    return rc;
}


