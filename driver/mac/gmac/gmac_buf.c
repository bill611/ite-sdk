/******************************************************************************
* internal functions (buffer)
*****************************************************************************/
static int ftgmac100_alloc_rx_page(struct ftgmac100 *priv, struct ftgmac100_rxdes *rxdes)
{
    struct eth_device *netdev = priv->netdev;
    unsigned int buf;

    buf = (unsigned int)malloc(RX_BUF_SIZE);  /* api is 64-bytes alignment */
    if (!buf)
        return ERROR_MAC_ALLOC_RX_BUF_FAIL;

    ftgmac100_rxdes_set_page(rxdes, buf);
    ftgmac100_rxdes_set_dma_addr(rxdes, buf);
    ftgmac100_rxdes_set_dma_own(rxdes);
    return 0;
}

static void ftgmac100_free_buffers(struct ftgmac100 *priv)
{
    int i;

    for (i = 0; i < RX_QUEUE_ENTRIES; i++) {
        struct ftgmac100_rxdes *rxdes = &priv->descs->rxdes[i];
        unsigned int buf = ftgmac100_rxdes_get_page(rxdes);

        if (!buf)
            continue;

        free((void*)buf);
        ftgmac100_rxdes_set_page(rxdes, 0);
    }

    /* no need to free tx buffer */
    itpWTFree((uint32_t)priv->descs);
    priv->descs = 0;

}

static int ftgmac100_alloc_buffers(struct ftgmac100 *priv)
{
    int i, rc = 0;

    priv->descs = (struct ftgmac100_descs*)itpWTAlloc(sizeof(struct ftgmac100_descs));
    if (!priv->descs) {
        rc = ERROR_MAC_ALLOC_DESC_FAIL;
        goto end;
    }
    memset((void*)priv->descs, 0, sizeof(struct ftgmac100_descs));

    /* initialize RX ring */
    ftgmac100_rxdes_set_end_of_ring(&priv->descs->rxdes[RX_QUEUE_ENTRIES - 1]);

    for (i = 0; i < RX_QUEUE_ENTRIES; i++) {
        struct ftgmac100_rxdes *rxdes = &priv->descs->rxdes[i];

        if (ftgmac100_alloc_rx_page(priv, rxdes))
            goto err;
    }

    /* initialize TX ring */
    ftgmac100_txdes_set_end_of_ring(&priv->descs->txdes[TX_QUEUE_ENTRIES - 1]);
    return 0;

err:
    ftgmac100_free_buffers(priv);
end:
    check_result(rc);
    return rc;
}