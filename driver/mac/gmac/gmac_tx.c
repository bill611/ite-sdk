/******************************************************************************
* internal functions (transmit)
*****************************************************************************/
static int ftgmac100_next_tx_pointer(int pointer)
{
    return (pointer + 1) & (TX_QUEUE_ENTRIES - 1);
}

static void ftgmac100_tx_pointer_advance(struct ftgmac100 *priv)
{
    priv->tx_pointer = ftgmac100_next_tx_pointer(priv->tx_pointer);
}

static void ftgmac100_tx_clean_pointer_advance(struct ftgmac100 *priv)
{
    priv->tx_clean_pointer = ftgmac100_next_tx_pointer(priv->tx_clean_pointer);
}

static struct ftgmac100_txdes *ftgmac100_current_txdes(struct ftgmac100 *priv)
{
    return &priv->descs->txdes[priv->tx_pointer];
}

static struct ftgmac100_txdes *
ftgmac100_current_clean_txdes(struct ftgmac100 *priv)
{
    return &priv->descs->txdes[priv->tx_clean_pointer];
}

static bool ftgmac100_tx_complete_packet(struct ftgmac100 *priv)
{
    struct eth_device *netdev = priv->netdev;
    struct ftgmac100_txdes *txdes;
    unsigned int len;
    unsigned int addr;

    if (priv->tx_pending == 0)
        return false;

    txdes = ftgmac100_current_clean_txdes(priv);

    if (ftgmac100_txdes_owned_by_dma(txdes))
        return false;

    len = ftgmac100_txdes_get_len(txdes);
    addr = ftgmac100_txdes_get_dma_addr(txdes);

    netdev->stats.tx_packets++;
    netdev->stats.tx_bytes += len;

    ftgmac100_txdes_reset(txdes);

    ftgmac100_tx_clean_pointer_advance(priv);

    spin_lock(&priv->tx_lock);
    priv->tx_pending--;
    spin_unlock(&priv->tx_lock);
    netif_wake_queue(netdev);

    return true;
}

static void ftgmac100_tx_complete(struct ftgmac100 *priv)
{
    while (ftgmac100_tx_complete_packet(priv))
        ;
}

static int ftgmac100_xmit(struct ftgmac100 *priv, void* packet,
    unsigned int length)
{
    struct eth_device *netdev = priv->netdev;
    struct ftgmac100_txdes *txdes;
    unsigned int len = (length < ETH_ZLEN) ? ETH_ZLEN : length;

    txdes = ftgmac100_current_txdes(priv);
    ftgmac100_tx_pointer_advance(priv);

    /* setup TX descriptor */
    ftgmac100_txdes_set_len(txdes, len);
    ftgmac100_txdes_set_dma_addr(txdes, (dma_addr_t)packet);
    ftgmac100_txdes_set_buffer_size(txdes, len);

    ftgmac100_txdes_set_first_segment(txdes);
    ftgmac100_txdes_set_last_segment(txdes);
    ftgmac100_txdes_set_txint(txdes);
#if 0
    if (skb->ip_summed == CHECKSUM_PARTIAL) {
        __be16 protocol = skb->protocol;

        if (protocol == cpu_to_be16(ETH_P_IP)) {
            u8 ip_proto = ip_hdr(skb)->protocol;

            ftgmac100_txdes_set_ipcs(txdes);
            if (ip_proto == IPPROTO_TCP)
                ftgmac100_txdes_set_tcpcs(txdes);
            else if (ip_proto == IPPROTO_UDP)
                ftgmac100_txdes_set_udpcs(txdes);
        }
    }
#endif

    spin_lock(&priv->tx_lock);
    priv->tx_pending++;
    if (priv->tx_pending >= (TX_QUEUE_ENTRIES - 4))
        netif_stop_queue(netdev);

    /* start transmit */
    ftgmac100_txdes_set_dma_own(txdes);
    spin_unlock(&priv->tx_lock);

    ftgmac100_txdma_normal_prio_start_polling(priv);

    return 0;
}
