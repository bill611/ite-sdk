
#define iowrite32(val, reg)     ithWriteRegA(reg, val)
#define ioread32(reg)           ithReadRegA(reg)

/******************************************************************************
* internal functions (hardware register access)
*****************************************************************************/
#define INT_MASK_ALL_ENABLED	(FTGMAC100_INT_RPKT_LOST	| \
    FTGMAC100_INT_XPKT_ETH | \
    FTGMAC100_INT_XPKT_LOST | \
    FTGMAC100_INT_AHB_ERR | \
    FTGMAC100_INT_RPKT_BUF | \
    FTGMAC100_INT_NO_RXBUF)

static void ftgmac100_enable_all_int(struct ftgmac100 *priv)
{
    iowrite32(INT_MASK_ALL_ENABLED, priv->base + FTGMAC100_OFFSET_IER);
}

static void ftgmac100_disable_all_int(struct ftgmac100 *priv)
{
    iowrite32(0, priv->base + FTGMAC100_OFFSET_IER);
}

static void ftgmac100_set_rx_ring_base(struct ftgmac100 *priv, dma_addr_t addr)
{
    iowrite32(addr, priv->base + FTGMAC100_OFFSET_RXR_BADR);
}

static void ftgmac100_set_rx_buffer_size(struct ftgmac100 *priv,
    unsigned int size)
{
    size = FTGMAC100_RBSR_SIZE(size);
    iowrite32(size, priv->base + FTGMAC100_OFFSET_RBSR);
}

static void ftgmac100_set_normal_prio_tx_ring_base(struct ftgmac100 *priv,
    dma_addr_t addr)
{
    iowrite32(addr, priv->base + FTGMAC100_OFFSET_NPTXR_BADR);
}

static void ftgmac100_txdma_normal_prio_start_polling(struct ftgmac100 *priv)
{
    iowrite32(1, priv->base + FTGMAC100_OFFSET_NPTXPD);
}

static int ftgmac100_reset_hw(struct ftgmac100 *priv)
{
    struct eth_device *netdev = (struct eth_device *)priv->netdev;
    int i;

    /* NOTE: reset clears all registers */
    iowrite32(FTGMAC100_MACCR_SW_RST, priv->base + FTGMAC100_OFFSET_MACCR);
    for (i = 0; i < 5; i++) {
        unsigned int maccr;

        maccr = ioread32(priv->base + FTGMAC100_OFFSET_MACCR);
        if (!(maccr & FTGMAC100_MACCR_SW_RST))
            return 0;

        udelay(1000);
    }

    LOG_ERROR "software reset failed\n" LOG_END
    return ERROR_MAC_RESET_TIMEOUT;
}

static void ftgmac100_set_mac(struct ftgmac100 *priv, const unsigned char *mac)
{
    unsigned int maddr = mac[0] << 8 | mac[1];
    unsigned int laddr = mac[2] << 24 | mac[3] << 16 | mac[4] << 8 | mac[5];

    iowrite32(maddr, priv->base + FTGMAC100_OFFSET_MAC_MADR);
    iowrite32(laddr, priv->base + FTGMAC100_OFFSET_MAC_LADR);
}

static void ftgmac100_init_hw(struct ftgmac100 *priv)
{
    /* setup ring buffer base registers */
    ftgmac100_set_rx_ring_base(priv, (unsigned int)priv->descs->rxdes);

    ftgmac100_set_normal_prio_tx_ring_base(priv, (unsigned int)priv->descs->txdes);

    ftgmac100_set_rx_buffer_size(priv, RX_BUF_SIZE);

    iowrite32(FTGMAC100_APTC_RXPOLL_CNT(1), priv->base + FTGMAC100_OFFSET_APTC);

    ftgmac100_set_mac(priv, priv->netdev->netaddr);
}

#define MACCR_ENABLE_ALL	(FTGMAC100_MACCR_TXDMA_EN	| \
    FTGMAC100_MACCR_RXDMA_EN | \
    FTGMAC100_MACCR_TXMAC_EN | \
    FTGMAC100_MACCR_RXMAC_EN | \
    FTGMAC100_MACCR_FULLDUP | \
    FTGMAC100_MACCR_CRC_APD | \
    FTGMAC100_MACCR_RX_RUNT | \
    FTGMAC100_MACCR_RX_BROADPKT)

static void ftgmac100_start_hw(struct ftgmac100 *priv)
{
    int maccr = MACCR_ENABLE_ALL;
    int speed, duplex;

    if (priv->mode == ITE_ETH_MAC_LB) {
        maccr |= FTGMAC100_MACCR_LOOP_EN;
        maccr |= FTGMAC100_MACCR_GIGA_MODE;
        maccr |= FTGMAC100_MACCR_FULLDUP;
        NETIF_CARRIER_ON(priv->netdev);
    } else {
        phy_read_mode(priv->netdev, &speed, &duplex);

        if (speed == SPEED_1000)
            maccr |= FTGMAC100_MACCR_GIGA_MODE;
        else if (speed == SPEED_100)
            maccr |= FTGMAC100_MACCR_FAST_MODE;

        if (duplex == DUPLEX_FULL)
            maccr |= FTGMAC100_MACCR_FULLDUP;
    }

    if (priv->mode != ITE_ETH_REAL) {
        maccr |= FTGMAC100_MACCR_RX_ALL;
        #if defined(MAC_CRC_DIS)
        maccr |= FTGMAC100_MACCR_DISCARD_CRCERR;
        #endif
    }

#if 0 // TODO

    #if defined(CLK_FROM_PHY)
    maccr |= MAC_REF_CLK_FROM_PHY;
    #endif
    if (priv->cfg->clk_inv)
        maccr |= MAC_REF_CLK_INVERT;

    maccr |= MAC_REF_CLK_DELAY(priv->cfg->clk_delay);
#endif // #if 0 // TODO

    #if defined(MULTICAST)
    /* see <if.h> IFF_PROMISC, IFF_ALLMULTI, IFF_MULTICAST */
    if (priv->rx_flag & IFF_PROMISC)
        maccr |= FTGMAC100_MACCR_RX_ALL;

    if (priv->rx_flag & IFF_ALLMULTI)
        maccr |= FTGMAC100_MACCR_RX_MULTIPKT;

    if (priv->rx_flag & IFF_MULTICAST)
        maccr |= FTGMAC100_MACCR_HT_MULTI_EN;

    iowrite32(priv->mc_ht0, priv->base + FTGMAC100_OFFSET_MAHT0);
    iowrite32(priv->mc_ht1, priv->base + FTGMAC100_OFFSET_MAHT1);
    #endif

    switch (speed) {
    default:
    case 10:
        break;

    case 100:
        maccr |= FTGMAC100_MACCR_FAST_MODE;
        break;

    case 1000:
        maccr |= FTGMAC100_MACCR_GIGA_MODE;
        break;
    }

    iowrite32(maccr, priv->base + FTGMAC100_OFFSET_MACCR);
}

static void ftgmac100_stop_hw(struct ftgmac100 *priv)
{
    iowrite32(0, priv->base + FTGMAC100_OFFSET_MACCR);
}
