/******************************************************************************
* internal functions (receive)
*****************************************************************************/
static int ftgmac100_next_rx_pointer(int pointer)
{
    return (pointer + 1) & (RX_QUEUE_ENTRIES - 1);
}

static void ftgmac100_rx_pointer_advance(struct ftgmac100 *priv)
{
    priv->rx_pointer = ftgmac100_next_rx_pointer(priv->rx_pointer);
}

static struct ftgmac100_rxdes *ftgmac100_current_rxdes(struct ftgmac100 *priv)
{
    return &priv->descs->rxdes[priv->rx_pointer];
}

static struct ftgmac100_rxdes *
ftgmac100_rx_locate_first_segment(struct ftgmac100 *priv)
{
    struct ftgmac100_rxdes *rxdes = ftgmac100_current_rxdes(priv);

    while (ftgmac100_rxdes_packet_ready(rxdes)) {
        if (ftgmac100_rxdes_first_segment(rxdes))
            return rxdes;

        ftgmac100_rxdes_set_dma_own(rxdes);
        ftgmac100_rx_pointer_advance(priv);
        rxdes = ftgmac100_current_rxdes(priv);
    }

    return NULL;
}

static bool ftgmac100_rx_packet_error(struct ftgmac100 *priv,
struct ftgmac100_rxdes *rxdes)
{
    struct eth_device *netdev = priv->netdev;
    bool error = false;

    if (unlikely(ftgmac100_rxdes_rx_error(rxdes))) {
        LOG_ERROR "rx err\n" LOG_END

        netdev->stats.rx_errors++;
        error = true;
    }

    if (unlikely(ftgmac100_rxdes_crc_error(rxdes))) {
        LOG_ERROR  "rx crc err\n" LOG_END

        netdev->stats.rx_crc_errors++;
        error = true;
    }
    else if (unlikely(ftgmac100_rxdes_ipcs_err(rxdes))) {
        LOG_ERROR  "rx IP checksum err\n" LOG_END

        error = true;
    }

    if (unlikely(ftgmac100_rxdes_frame_too_long(rxdes))) {
        LOG_ERROR  "rx frame too long\n" LOG_END

        netdev->stats.rx_length_errors++;
        error = true;
    }
    else if (unlikely(ftgmac100_rxdes_runt(rxdes))) {
        LOG_ERROR  "rx runt\n" LOG_END

        netdev->stats.rx_length_errors++;
        error = true;
    }
    else if (unlikely(ftgmac100_rxdes_odd_nibble(rxdes))) {
        LOG_ERROR  "rx odd nibble\n" LOG_END

        netdev->stats.rx_length_errors++;
        error = true;
    }

    return error;
}

static void ftgmac100_rx_drop_packet(struct ftgmac100 *priv)
{
    struct eth_device *netdev = priv->netdev;
    struct ftgmac100_rxdes *rxdes = ftgmac100_current_rxdes(priv);
    bool done = false;

    LOG_WARNING "drop packet %p\n", rxdes LOG_END

    do {
        if (ftgmac100_rxdes_last_segment(rxdes))
            done = true;

        ftgmac100_rxdes_set_dma_own(rxdes);
        ftgmac100_rx_pointer_advance(priv);
        rxdes = ftgmac100_current_rxdes(priv);
    } while (!done && ftgmac100_rxdes_packet_ready(rxdes));

    netdev->stats.rx_dropped++;
}

static bool ftgmac100_rx_packet(struct ftgmac100 *priv, int *processed)
{
    struct eth_device *netdev = priv->netdev;
    struct ftgmac100_rxdes *rxdes;
    bool done = false;
    unsigned int length = 0;
    dma_addr_t rx_buf;

    rxdes = ftgmac100_rx_locate_first_segment(priv);
    if (!rxdes)
        return false;

    if (unlikely(ftgmac100_rx_packet_error(priv, rxdes))) {
        ftgmac100_rx_drop_packet(priv);
        return true;
    }

    if (unlikely(ftgmac100_rxdes_multicast(rxdes)))
        netdev->stats.multicast++;

#if 0
    /*
    * It seems that HW does checksum incorrectly with fragmented packets,
    * so we are conservative here - if HW checksum error, let software do
    * the checksum again.
    */
    if ((ftgmac100_rxdes_is_tcp(rxdes) && !ftgmac100_rxdes_tcpcs_err(rxdes)) ||
        (ftgmac100_rxdes_is_udp(rxdes) && !ftgmac100_rxdes_udpcs_err(rxdes)))
        skb->ip_summed = CHECKSUM_UNNECESSARY;
#endif

    do {
        dma_addr_t addr = ftgmac100_rxdes_get_dma_addr(rxdes);
        unsigned int size;

        size = ftgmac100_rxdes_data_length(rxdes);
        length += size;
        rx_buf = addr;  // maybe TODO

        if (ftgmac100_rxdes_last_segment(rxdes))
            done = true;
        else
            LOG_WARNING "More descripts for one rx packet! ==> TODO!!! \n" LOG_END

        #if !defined(CFG_CPU_WB) /* already do this before change owner to dma */
            ithInvalidateDCacheRange((void*)(addr + 2), size);
        #endif

        ftgmac100_rx_pointer_advance(priv);
        rxdes = ftgmac100_current_rxdes(priv);
    } while (!done);

    netdev->stats.rx_packets++;
    netdev->stats.rx_bytes += length;

    /* push packet to protocol stack */
    priv->netif_rx(priv->netif_arg, (void*)(rx_buf + 2), length);

    (*processed)++;
    return true;
}
