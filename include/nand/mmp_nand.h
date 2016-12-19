#ifndef MMP_NAND_H
#define MMP_NAND_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * DLL export API declaration for Win32 and WinCE.
 */
#if 0//defined(WIN32) || defined(_WIN32_WCE)
    #if defined(NAND_EXPORTS)
        #define NAND_API __declspec(dllexport)
    #else
        #define NAND_API __declspec(dllimport)
    #endif
#else
    #define NAND_API extern
#endif /* defined(WIN32) */


NAND_API int
mmpNandGetCapacity(unsigned long* lastBlockId,
                   unsigned long* blockLength);
                   
NAND_API int 
mmpNandReadSector(unsigned long blockId,
                  unsigned long sizeInByte,
                  unsigned char* srcBuffer);
                  
NAND_API int 
mmpNandWriteSector(unsigned long blockId,
                  unsigned long sizeInByte,
                  unsigned char* destBuffer); 

#ifdef __cplusplus
}
#endif

#endif /* MMP_NAND_H */