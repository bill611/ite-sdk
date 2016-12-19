
/******************************************************************************
 * struct net_device_ops functions
 *****************************************************************************/
static int ftgmac100_open(struct eth_device *netdev, int alloc_buf)
{
    struct ftgmac100 *priv = (struct ftgmac100 *)netdev->priv;
    int err;

    if (alloc_buf) {
        err = ftgmac100_alloc_buffers(priv);
        if (err) 
            goto err_end;
    }

    priv->rx_pointer = 0;
    priv->tx_clean_pointer = 0;
    priv->tx_pointer = 0;
    priv->tx_pending = 0;

    err = ftgmac100_reset_hw(priv);
    if (err)
        goto err_end;

    ftgmac100_init_hw(priv);
    ftgmac100_start_hw(priv);

    netif_start_queue(netdev);
    netif_start(netdev);

    /* enable all interrupts */
    ftgmac100_enable_all_int(priv);
    intrEnable(netdev);
    return 0;

err_end:
    check_result(err);
    if (err)
        LOG_ERROR "[%s] rc = 0x%08X \n", __FUNCTION__, err LOG_END
    return err;
}

static int ftgmac100_stop(struct eth_device *netdev, int free_buf)
{
    struct ftgmac100 *priv = (struct ftgmac100 *)netdev->priv;

    ftgmac100_disable_all_int(priv);
    intrDisable(priv);
    netif_stop_queue(netdev);
    netif_stop(netdev);

    ftgmac100_stop_hw(priv);
    if(free_buf)
        ftgmac100_free_buffers(priv);

    return 0;
}

static int ftgmac100_hard_start_xmit(void* packet, uint32_t len, struct eth_device *netdev)
{
    struct ftgmac100 *priv = (struct ftgmac100 *)netdev->priv;

    if (len > MAX_PKT_SIZE) {
        LOG_ERROR "tx packet too big\n" LOG_END

        netdev->stats.tx_dropped++;
        return 0;
    }

    return ftgmac100_xmit(priv, packet, len);
}

/* optional */
static int ftgmac100_do_ioctl(struct eth_device *netdev, struct mii_ioctl_data* data, int cmd)
{
    struct ftgmac100 *priv = (struct ftgmac100 *)netdev->priv;

    return generic_mii_ioctl(&priv->mii, data, cmd, NULL);
}

/**********************************************/
static int crc32(uint8_t* s, int length)
{
    int perByte;
    int perBit;
    /* crc polynomial for Ethernet */
    const unsigned long poly = 0xedb88320;
    /* crc value - preinitialized to all 1's */
    unsigned long crc_value = 0xffffffff;
    unsigned long crc_bit_reverse = 0x0, i;

    for(perByte=0; perByte<length; perByte++)
    {
        uint8_t c;

        c = *(s++);
        for(perBit=0; perBit<8; perBit++)
        {
            crc_value = (crc_value >> 1)^(((crc_value^c)&0x01)?poly:0);
            c >>= 1;
        }
    }

    for(i=0; i<32; i++)
        crc_bit_reverse |= ((crc_value>>i) & 0x1) << (31-i);

    //return crc_value;
    return crc_bit_reverse;
}

static void ftgmac100_setmulticast(struct ftgmac100 *priv, uint8_t* mc_list, int count)
{
    uint8_t* cur_addr;
    int i, crc_val;
    uint32_t ht0=0, ht1=0;

    for(i=0; i<count; i++)
    {
        cur_addr = mc_list + (i*8);
        if(*cur_addr & 1)
        {
            crc_val = crc32(cur_addr, 6);
            crc_val = (crc_val >> 26) & 0x3F;  // MSB 6 bits

            if(crc_val >= 32)
                ht1 |= (0x1 << (crc_val-32));
            else
                ht0 |= (0x1 << crc_val);
        }
    }

    priv->mc_ht0 = ht0;
    priv->mc_ht1 = ht1;
}

static int ftgmac100_set_rx_mode(struct eth_device *netdev, int flag, uint8_t* mc_list, int count)
{
    struct ftgmac100 *priv = (struct ftgmac100 *)netdev->priv;

    priv->rx_flag = flag; /* see <if.h> IFF_PROMISC, IFF_ALLMULTI, IFF_MULTICAST */
    priv->mc_count = count;

    if((flag & IFF_MULTICAST) && (count > 0) && mc_list)
        ftgmac100_setmulticast(priv, mc_list, count);
    else
    {
        priv->mc_ht0 = 0x0;
        priv->mc_ht1 = 0x0;
    }

    if(NETIF_RUNNING(dev))
        ftgmac100_start_hw(priv);

    return 0;
}

