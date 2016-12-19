/******************************************************************************
* internal functions (receive descriptor)
*****************************************************************************/
static bool ftgmac100_rxdes_first_segment(struct ftgmac100_rxdes *rxdes)
{
    return rxdes->rxdes0 & cpu_to_le32(FTGMAC100_RXDES0_FRS);
}

static bool ftgmac100_rxdes_last_segment(struct ftgmac100_rxdes *rxdes)
{
    return rxdes->rxdes0 & cpu_to_le32(FTGMAC100_RXDES0_LRS);
}

static bool ftgmac100_rxdes_packet_ready(struct ftgmac100_rxdes *rxdes)
{
    return rxdes->rxdes0 & cpu_to_le32(FTGMAC100_RXDES0_RXPKT_RDY);
}

static void ftgmac100_rxdes_set_dma_own(struct ftgmac100_rxdes *rxdes)
{
#if defined(CFG_CPU_WB)
    /* avoid cpu flush dirty cache to memory */
    ithInvalidateDCacheRange((void*)rxdes->rxdes3, RX_BUF_SIZE);
#endif
    /* clear status bits */
    rxdes->rxdes0 &= cpu_to_le32(FTGMAC100_RXDES0_EDORR);
}

static bool ftgmac100_rxdes_rx_error(struct ftgmac100_rxdes *rxdes)
{
    return rxdes->rxdes0 & cpu_to_le32(FTGMAC100_RXDES0_RX_ERR);
}

static bool ftgmac100_rxdes_crc_error(struct ftgmac100_rxdes *rxdes)
{
    return rxdes->rxdes0 & cpu_to_le32(FTGMAC100_RXDES0_CRC_ERR);
}

static bool ftgmac100_rxdes_frame_too_long(struct ftgmac100_rxdes *rxdes)
{
    return rxdes->rxdes0 & cpu_to_le32(FTGMAC100_RXDES0_FTL);
}

static bool ftgmac100_rxdes_runt(struct ftgmac100_rxdes *rxdes)
{
    return rxdes->rxdes0 & cpu_to_le32(FTGMAC100_RXDES0_RUNT);
}

static bool ftgmac100_rxdes_odd_nibble(struct ftgmac100_rxdes *rxdes)
{
    return rxdes->rxdes0 & cpu_to_le32(FTGMAC100_RXDES0_RX_ODD_NB);
}

static unsigned int ftgmac100_rxdes_data_length(struct ftgmac100_rxdes *rxdes)
{
#if defined(MAC_CRC_DIS)
    unsigned int len = le32_to_cpu(rxdes->rxdes0) & FTGMAC100_RXDES0_VDBC;
    return (len - 4);
#else
    return le32_to_cpu(rxdes->rxdes0) & FTGMAC100_RXDES0_VDBC;
#endif
}

static bool ftgmac100_rxdes_multicast(struct ftgmac100_rxdes *rxdes)
{
    return rxdes->rxdes0 & cpu_to_le32(FTGMAC100_RXDES0_MULTICAST);
}

static void ftgmac100_rxdes_set_end_of_ring(struct ftgmac100_rxdes *rxdes)
{
    rxdes->rxdes0 |= cpu_to_le32(FTGMAC100_RXDES0_EDORR);
}

static void ftgmac100_rxdes_set_dma_addr(struct ftgmac100_rxdes *rxdes,
    dma_addr_t addr)
{
    rxdes->rxdes3 = cpu_to_le32(addr);
}

static dma_addr_t ftgmac100_rxdes_get_dma_addr(struct ftgmac100_rxdes *rxdes)
{
    return le32_to_cpu(rxdes->rxdes3);
}

static bool ftgmac100_rxdes_is_tcp(struct ftgmac100_rxdes *rxdes)
{
    return (rxdes->rxdes1 & cpu_to_le32(FTGMAC100_RXDES1_PROT_MASK)) ==
        cpu_to_le32(FTGMAC100_RXDES1_PROT_TCPIP);
}

static bool ftgmac100_rxdes_is_udp(struct ftgmac100_rxdes *rxdes)
{
    return (rxdes->rxdes1 & cpu_to_le32(FTGMAC100_RXDES1_PROT_MASK)) ==
        cpu_to_le32(FTGMAC100_RXDES1_PROT_UDPIP);
}

static bool ftgmac100_rxdes_tcpcs_err(struct ftgmac100_rxdes *rxdes)
{
    return rxdes->rxdes1 & cpu_to_le32(FTGMAC100_RXDES1_TCP_CHKSUM_ERR);
}

static bool ftgmac100_rxdes_udpcs_err(struct ftgmac100_rxdes *rxdes)
{
    return rxdes->rxdes1 & cpu_to_le32(FTGMAC100_RXDES1_UDP_CHKSUM_ERR);
}

static bool ftgmac100_rxdes_ipcs_err(struct ftgmac100_rxdes *rxdes)
{
    return rxdes->rxdes1 & cpu_to_le32(FTGMAC100_RXDES1_IP_CHKSUM_ERR);
}

/*
* rxdes2 is not used by hardware. We use it to keep track of page.
* Since hardware does not touch it, we can skip cpu_to_le32()/le32_to_cpu().
*/
static void ftgmac100_rxdes_set_page(struct ftgmac100_rxdes *rxdes, unsigned int page)
{
    rxdes->rxdes2 = (unsigned int)page;
}

static unsigned int ftgmac100_rxdes_get_page(struct ftgmac100_rxdes *rxdes)
{
    return rxdes->rxdes2;
}


/******************************************************************************
* internal functions (transmit descriptor)
*****************************************************************************/
static void ftgmac100_txdes_reset(struct ftgmac100_txdes *txdes)
{
    /* clear all except end of ring bit */
    txdes->txdes0 &= cpu_to_le32(FTGMAC100_TXDES0_EDOTR);
    txdes->txdes1 = 0;
    txdes->txdes2 = 0;
    txdes->txdes3 = 0;
}

static bool ftgmac100_txdes_owned_by_dma(struct ftgmac100_txdes *txdes)
{
    return txdes->txdes0 & cpu_to_le32(FTGMAC100_TXDES0_TXDMA_OWN);
}

static void ftgmac100_txdes_set_dma_own(struct ftgmac100_txdes *txdes)
{
#if defined(CFG_CPU_WB)
    ithFlushDCacheRange((void*)txdes->txdes3, txdes->txdes2);
#endif
    /*
    * Make sure dma own bit will not be set before any other
    * descriptor fields.
    */
    wmb();
    txdes->txdes0 |= cpu_to_le32(FTGMAC100_TXDES0_TXDMA_OWN);
    ithFlushMemBuffer();
}

static void ftgmac100_txdes_set_end_of_ring(struct ftgmac100_txdes *txdes)
{
    txdes->txdes0 |= cpu_to_le32(FTGMAC100_TXDES0_EDOTR);
}

static void ftgmac100_txdes_set_first_segment(struct ftgmac100_txdes *txdes)
{
    txdes->txdes0 |= cpu_to_le32(FTGMAC100_TXDES0_FTS);
}

static void ftgmac100_txdes_set_last_segment(struct ftgmac100_txdes *txdes)
{
    txdes->txdes0 |= cpu_to_le32(FTGMAC100_TXDES0_LTS);
}

static void ftgmac100_txdes_set_buffer_size(struct ftgmac100_txdes *txdes,
    unsigned int len)
{
    txdes->txdes0 |= cpu_to_le32(FTGMAC100_TXDES0_TXBUF_SIZE(len));
}

static void ftgmac100_txdes_set_txint(struct ftgmac100_txdes *txdes)
{
    txdes->txdes1 |= cpu_to_le32(FTGMAC100_TXDES1_TXIC);
}

static void ftgmac100_txdes_set_tcpcs(struct ftgmac100_txdes *txdes)
{
    txdes->txdes1 |= cpu_to_le32(FTGMAC100_TXDES1_TCP_CHKSUM);
}

static void ftgmac100_txdes_set_udpcs(struct ftgmac100_txdes *txdes)
{
    txdes->txdes1 |= cpu_to_le32(FTGMAC100_TXDES1_UDP_CHKSUM);
}

static void ftgmac100_txdes_set_ipcs(struct ftgmac100_txdes *txdes)
{
    txdes->txdes1 |= cpu_to_le32(FTGMAC100_TXDES1_IP_CHKSUM);
}

static void ftgmac100_txdes_set_dma_addr(struct ftgmac100_txdes *txdes,
    dma_addr_t addr)
{
    txdes->txdes3 = cpu_to_le32(addr);
}

static dma_addr_t ftgmac100_txdes_get_dma_addr(struct ftgmac100_txdes *txdes)
{
    return le32_to_cpu(txdes->txdes3);
}

/*
* txdes2 is not used by hardware. We use it to keep track of socket buffer.
* Since hardware does not touch it, we can skip cpu_to_le32()/le32_to_cpu().
*/
static void ftgmac100_txdes_set_len(struct ftgmac100_txdes *txdes,
    unsigned int len)
{
    txdes->txdes2 = (unsigned int)len;
}

static unsigned int ftgmac100_txdes_get_len(struct ftgmac100_txdes *txdes)
{
    return txdes->txdes2;
}
