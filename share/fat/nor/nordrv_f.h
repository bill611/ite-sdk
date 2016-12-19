#ifndef _NORDRV_F_H_
#define _NORDRV_F_H_

#include "fat/fat.h"

#ifdef __cplusplus
extern "C" {
#endif

extern F_DRIVER *nor_initfunc(unsigned long driver_panor);

enum {
  NOR_NO_ERROR,
  NOR_ERR_SECTOR=101,
  NOR_ERR_NOTAVAILABLE
};

#ifdef __cplusplus
}
#endif

/******************************************************************************
 *
 *  End of nordrv.c
 *
 *****************************************************************************/

#endif /* _NORDRV_H_ */
