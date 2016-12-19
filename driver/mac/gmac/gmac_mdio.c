
/******************************************************************************
* struct mii_bus functions
*****************************************************************************/
static int ftgmac100_mdiobus_read(struct eth_device *netdev, int phy_addr, int regnum)
{
    struct ftgmac100 *priv = (struct ftgmac100 *)netdev->priv;
    unsigned int phycr;
    int i;

    phycr = ioread32(priv->base + FTGMAC100_OFFSET_PHYCR);

    /* preserve MDC cycle threshold */
    phycr &= FTGMAC100_PHYCR_MDC_CYCTHR_MASK;

    phycr |= FTGMAC100_PHYCR_PHYAD(phy_addr) |
        FTGMAC100_PHYCR_REGAD(regnum) |
        FTGMAC100_PHYCR_MIIRD;

    iowrite32(phycr, priv->base + FTGMAC100_OFFSET_PHYCR);

    for (i = 0; i < 10; i++) {
        phycr = ioread32(priv->base + FTGMAC100_OFFSET_PHYCR);

        if ((phycr & FTGMAC100_PHYCR_MIIRD) == 0) {
            int data;

            data = ioread32(priv->base + FTGMAC100_OFFSET_PHYDATA);
            return FTGMAC100_PHYDATA_MIIRDATA(data);
        }

        udelay(100);
    }

    ithPrintf("mdio read timed out \n");
    return -1;
}

static void ftgmac100_mdiobus_write(struct eth_device *netdev, int phy_addr, int regnum,
    int value)
{
    struct ftgmac100 *priv = (struct ftgmac100 *)netdev->priv;
    unsigned int phycr;
    int data;
    int i;

    phycr = ioread32(priv->base + FTGMAC100_OFFSET_PHYCR);

    /* preserve MDC cycle threshold */
    phycr &= FTGMAC100_PHYCR_MDC_CYCTHR_MASK;

    phycr |= FTGMAC100_PHYCR_PHYAD(phy_addr) |
        FTGMAC100_PHYCR_REGAD(regnum) |
        FTGMAC100_PHYCR_MIIWR;

    data = FTGMAC100_PHYDATA_MIIWDATA(value);

    iowrite32(data, priv->base + FTGMAC100_OFFSET_PHYDATA);
    iowrite32(phycr, priv->base + FTGMAC100_OFFSET_PHYCR);

    for (i = 0; i < 10; i++) {
        phycr = ioread32(priv->base + FTGMAC100_OFFSET_PHYCR);

        if ((phycr & FTGMAC100_PHYCR_MIIWR) == 0)
            return;

        udelay(100);
    }

    ithPrintf("mdio write timed out \n");
    return;// -1;
}
