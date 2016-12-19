
static sem_t isr_sem;

/******************************************************************************
* interrupt handler
*****************************************************************************/
static void ftgmac100_interrupt(void *arg)
{
    struct eth_device *netdev = arg;
    struct ftgmac100 *priv = netdev->priv;

    if (likely(netif_running(netdev))) {
        /* Disable interrupts for polling */
        ftgmac100_disable_all_int(priv);
        sem_post(&isr_sem);
    }
}

#define BUDGET  (RX_QUEUE_ENTRIES*3/4)

static int ftgmac100_poll(struct eth_device *netdev)
{
    struct ftgmac100 *priv = netdev->priv;
    unsigned int status;
    bool completed = true;
    int rx = 0;

    status = ioread32(priv->base + FTGMAC100_OFFSET_ISR);
    iowrite32(status, priv->base + FTGMAC100_OFFSET_ISR);

    if (status & (FTGMAC100_INT_RPKT_BUF | FTGMAC100_INT_NO_RXBUF)) {
        /*
        * FTGMAC100_INT_RPKT_BUF:
        *	RX DMA has received packets into RX buffer successfully
        *
        * FTGMAC100_INT_NO_RXBUF:
        *	RX buffer unavailable
        */
        bool retry;

        do {
            retry = ftgmac100_rx_packet(priv, &rx);
        } while (retry && rx < BUDGET);

        if (retry && rx == BUDGET)
            completed = false;
    }

    if (status & (FTGMAC100_INT_XPKT_ETH | FTGMAC100_INT_XPKT_LOST)) {
        /*
        * FTGMAC100_INT_XPKT_ETH:
        *	packet transmitted to ethernet successfully
        *
        * FTGMAC100_INT_XPKT_LOST:
        *	packet transmitted to ethernet lost due to late
        *	collision or excessive collision
        */
        ftgmac100_tx_complete(priv);
    }

    if (status & (FTGMAC100_INT_NO_RXBUF | FTGMAC100_INT_RPKT_LOST |
        FTGMAC100_INT_AHB_ERR | FTGMAC100_INT_PHYSTS_CHG)) {
        LOG_WARNING "[ISR] = 0x%x: %s%s%s%s\n", status,
            status & FTGMAC100_INT_NO_RXBUF ? "NO_RXBUF " : "",
            status & FTGMAC100_INT_RPKT_LOST ? "RPKT_LOST " : "",
            status & FTGMAC100_INT_AHB_ERR ? "AHB_ERR " : "",
            status & FTGMAC100_INT_PHYSTS_CHG ? "PHYSTS_CHG" : "" LOG_END

        if (status & FTGMAC100_INT_NO_RXBUF) {
            /* RX buffer unavailable */
            netdev->stats.rx_over_errors++;
        }

        if (status & FTGMAC100_INT_RPKT_LOST) {
            /* received packet lost due to RX FIFO full */
            netdev->stats.rx_fifo_errors++;
        }
    }

    if (completed) {
        /* enable all interrupts */
        ftgmac100_enable_all_int(priv);
    }
    else {
        LOG_WARNING "rx second pass!!!! => cpu too busy??? \n" LOG_END
        sem_post(&isr_sem);
    }

    return rx;
}


static volatile int task_done = 0;

void* iteMacThreadFunc(void* data)
{
    sem_init(&isr_sem, 0, 0);
    task_done = 1;

wait:
    sem_wait(&isr_sem);
    if (dev)
        ftgmac100_poll(dev);
    goto wait;
}

static void checkLinkStatus(struct eth_device *netdev)
{
    struct ftgmac100 *priv = netdev->priv;
    struct mii_if_info *mii_if = &priv->mii;
    unsigned int status;

    if (!priv->cfg->phy_link_change) {
        ithPrintf("phy_link_change callback is NULL! Please implement it! \n");
        return;
    }

    if (NETIF_RUNNING(netdev) && priv->cfg->phy_link_change())
    {
        int cur_link = mii_link_ok(&priv->mii);
        int prev_link = NETIF_CARRIER_OK(netdev);

        if (cur_link && !prev_link)
        {
            ftgmac100_start_hw(priv);
            NETIF_CARRIER_ON(netdev);
        }
        else if (prev_link && !cur_link)
        {
            NETIF_CARRIER_OFF(netdev);
        }
        //ithPrintf("\n\n PHY status change! \n\n");
    }
}

static void linkIntrHandler(unsigned int pin, void *arg)
{
    struct eth_device *netdev = (struct eth_device *)arg;
    struct ftgmac100 *priv = netdev->priv;

    //ithPrintf(" gpio isr \n");

    if (pin != priv->cfg->linkGpio)
    {
        ithPrintf(" MAC: phy link gpio error! %d \n", pin);
        return;
    }

    checkLinkStatus(netdev);

    return;
}

static void intrEnable(struct eth_device *netdev)
{
    struct ftgmac100 *priv = netdev->priv;
    int linkGpio = priv->cfg->linkGpio;

    /** register interrupt handler to interrupt mgr */
    ithIntrRegisterHandlerIrq(ITH_INTR_MAC, ftgmac100_interrupt, (void*)netdev);
    ithIntrSetTriggerModeIrq(ITH_INTR_MAC, ITH_INTR_LEVEL);
    ithIntrSetTriggerLevelIrq(ITH_INTR_MAC, ITH_INTR_HIGH_RISING);
    ithIntrEnableIrq(ITH_INTR_MAC);

#if defined(LINK_INTR)
    ithPrintf(" link gpio %d \n", linkGpio);
    if (linkGpio > 0)
    {
        /** register phy link up/down interrupt handler to gpio mgr, GPIO */
        ithGpioEnable(linkGpio);
        ithGpioSetIn(linkGpio);
        ithGpioClearIntr(linkGpio);
        if (priv->cfg->linkGpio_isr)
            ithGpioRegisterIntrHandler(linkGpio, priv->cfg->linkGpio_isr, (void*)netdev);
        else
            ithGpioRegisterIntrHandler(linkGpio, linkIntrHandler, (void*)netdev);
        ithGpioCtrlEnable(linkGpio, ITH_GPIO_INTR_LEVELTRIGGER);  /* use level trigger mode */
        ithGpioCtrlEnable(linkGpio, ITH_GPIO_INTR_TRIGGERFALLING); /* low active */
        ithGpioEnableIntr(linkGpio);
    }
#endif

    if (!task_done)
        LOG_ERROR " Task not create!!! \n\n" LOG_END
}

static void intrDisable(struct ftgmac100 *priv)
{
    ithIntrDisableIrq(ITH_INTR_MAC);
}
