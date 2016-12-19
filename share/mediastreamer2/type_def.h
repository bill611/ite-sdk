/*
 * type_def.h
 *
 *  Created on: 2015¦~3¤ë23¤é
 *      Author: ite01527
 */
#ifndef TYPE_DEF_H_
#define TYPE_DEF_H_

#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define pi 3.1415926
#define max(a, b) (((a) > (b))? (a): (b))
#define min(a, b) (((a) < (b))? (a): (b))
#define ABS(a) (((a) < 0)? -(a): (a))

#if defined(_MSC_VER)
#define ALIGN4_BEGIN __declspec(align(4))
#define ALIGN4_END
#define __restrict
#define llabs ABS 
#else
#define ALIGN4_BEGIN
#define ALIGN4_END __attribute__((aligned(4)))
#endif

/*typedef enum {
 FALSE, TRUE
 } bool;*/

typedef int16_t Word16;
typedef uint16_t UWord16;
typedef int32_t Word32;
typedef uint32_t UWord32;
typedef int64_t Word64;
typedef uint64_t UWord64;
typedef float Float32; // single precision

typedef struct {
	UWord32 a0;
	UWord32 a1;
	UWord32 a2;
	UWord32 a3;
} UWRD128;

typedef struct {
	Word16 real;
	Word16 imag;
} Complex16_t; // 2

typedef struct {
	Word32 real;
	Word32 imag;
} Complex32_t; // 4

typedef struct {
	Float32 real;
	Float32 imag;
} ComplexFloat32; // 4-byte aligned

typedef Complex16_t ComplexInt16;
typedef Complex32_t ComplexInt32;

#endif
