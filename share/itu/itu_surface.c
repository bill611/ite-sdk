#include <assert.h>
#include <malloc.h>
#include "ucl/ucl.h"
#include "ite/itp.h"
#include "ite/itu.h"
#include "itu_cfg.h"
#include "gfx/gfx.h"
#include "itu_private.h"

void ituSurfaceSetClipping(ITUSurface *surf, int x, int y, int w, int h)
{
    if (x == 0 && y == 0 && w == 0 && h == 0)
        surf->flags &= ~ITU_CLIPPING;
    else
        surf->flags |= ITU_CLIPPING;

    surf->clipping.x      = x;
    surf->clipping.y      = y;
    surf->clipping.width  = (w <= surf->width) ? w : surf->width;
    surf->clipping.height = (h <= surf->height) ? h : surf->height;
}

ITUSurface *ituSurfaceDecompress(ITUSurface *surf)
{
    ITUSurface   *retSurf = NULL;
    unsigned int flags;
    assert(surf->flags & ITU_COMPRESSED);

    if ((surf->flags & ITU_STATIC) && (surf->lockSize > 0)) // as reference count
    {
        assert(surf->lockAddr);                             // as cached decompressed surface
        surf->lockSize++;
        return (ITUSurface *) surf->lockAddr;
    }

    if (surf->flags & ITU_JPEG)
    {
        flags   = surf->flags & ~(ITU_COMPRESSED | ITU_STATIC | ITU_JPEG);
        retSurf = ituJpegLoad(surf->width, surf->height, (uint8_t *)surf->addr, surf->size);
    }
    else
    {
        int ret, compressedSize, bufSize;
        uint8_t* buf;

        if (surf->format == ITU_RGB565A8)
            bufSize = surf->pitch * surf->height + surf->width * surf->height;
        else
            bufSize = surf->pitch * surf->height;

        buf = malloc(bufSize);
        if (!buf)
        {
            LOG_ERR "out of memory: %d\n", surf->pitch * surf->height LOG_END
            return NULL;
        }

        compressedSize = surf->size;
        LOG_DBG "decompress surface size %d to %d\n", compressedSize, bufSize LOG_END

#if defined(CFG_DCPS_ENABLE) && !defined(CFG_ITU_UCL_ENABLE)
        // hardware decompress
        ioctl(ITP_DEVICE_DECOMPRESS, ITP_IOCTL_INIT, NULL);
        surf->lockSize = ithBswap32(bufSize);
        surf->parent   = (ITUSurface *)ithBswap32(compressedSize);
        ret            = write(ITP_DEVICE_DECOMPRESS, &surf->lockSize, compressedSize);
        if (ret != compressedSize)
        {
            LOG_ERR "decompress write error: %d != %d\n", ret, compressedSize LOG_END
            free(buf);
            return NULL;
        }

        ret = read(ITP_DEVICE_DECOMPRESS, buf, bufSize);
        if (ret != bufSize)
        {
            LOG_ERR "decompress read error: %d != %d\n", ret, bufSize LOG_END
            free(buf);
            return NULL;
        }
        ioctl(ITP_DEVICE_DECOMPRESS, ITP_IOCTL_EXIT, NULL);
        surf->lockSize = 0;

#else
        // software decompress
        if (ucl_init() != UCL_E_OK)
        {
            LOG_ERR "internal error - ucl_init() failed !!!\n" LOG_END
            free(buf);
            return NULL;
        }

        ret = bufSize;
        if (ucl_nrv2e_decompress_8((const ucl_bytep)surf->addr, compressedSize, buf, &ret, NULL) != UCL_E_OK || ret != bufSize)
        {
            LOG_ERR "internal error - decompression failed\n" LOG_END
            free(buf);
            return NULL;
        }
#endif     // defined(CFG_DCPS_ENABLE) && !defined(CFG_ITU_UCL_ENABLE)

        flags   = surf->flags & ~(ITU_COMPRESSED | ITU_STATIC);
        retSurf = ituCreateSurface(surf->width, surf->height, surf->pitch, surf->format, buf, flags);
    }

    if (retSurf && (surf->flags & ITU_STATIC) && (surf->lockSize == 0))
    {
        surf->lockSize++;
        surf->lockAddr  = (uint8_t *)retSurf;
        retSurf->parent = (struct ITUSurfaceTag *)surf;
    }
    return retSurf;
}

void ituSurfaceRelease(ITUSurface *surf)
{
    ITUSurface *parentSurf = (ITUSurface *)surf->parent;

    if (parentSurf && (parentSurf->flags & ITU_STATIC))
    {
        assert(parentSurf->flags & ITU_COMPRESSED);
        assert(parentSurf->lockSize);

        if (--parentSurf->lockSize == 0)
        {
            ituDestroySurface(surf);
        }
    }
    else
        ituDestroySurface(surf);
}