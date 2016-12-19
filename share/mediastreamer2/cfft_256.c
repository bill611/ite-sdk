/*
 * cfft_256.c
 *
 *  Created on: 2015¦~10¤ë13¤é
 *      Author: ite01527
 */
#include <assert.h>
#include <math.h>
#include "type_def.h"
#include "basic_op.h"
#include "dft_filt_bank.h"

//#define CORDIC

static UWord32 angles[20] = { 536870912, 316933405, 167458907, 85004756,
		42667331, 21354465, 10679838, 5340245, 2670163, 1335086, 667544, 333772,
		166886, 83443, 41721, 20860, 10430, 5215, 2607, 1303 };
static Word32 bit_rev_tab[256] = { 0, 128, 64, 192, 32, 160, 96, 224, 16, 144,
		80, 208, 48, 176, 112, 240, 8, 136, 72, 200, 40, 168, 104, 232, 24, 152,
		88, 216, 56, 184, 120, 248, 4, 132, 68, 196, 36, 164, 100, 228, 20, 148,
		84, 212, 52, 180, 116, 244, 12, 140, 76, 204, 44, 172, 108, 236, 28,
		156, 92, 220, 60, 188, 124, 252, 2, 130, 66, 194, 34, 162, 98, 226, 18,
		146, 82, 210, 50, 178, 114, 242, 10, 138, 74, 202, 42, 170, 106, 234,
		26, 154, 90, 218, 58, 186, 122, 250, 6, 134, 70, 198, 38, 166, 102, 230,
		22, 150, 86, 214, 54, 182, 118, 246, 14, 142, 78, 206, 46, 174, 110,
		238, 30, 158, 94, 222, 62, 190, 126, 254, 1, 129, 65, 193, 33, 161, 97,
		225, 17, 145, 81, 209, 49, 177, 113, 241, 9, 137, 73, 201, 41, 169, 105,
		233, 25, 153, 89, 217, 57, 185, 121, 249, 5, 133, 69, 197, 37, 165, 101,
		229, 21, 149, 85, 213, 53, 181, 117, 245, 13, 141, 77, 205, 45, 173,
		109, 237, 29, 157, 93, 221, 61, 189, 125, 253, 3, 131, 67, 195, 35, 163,
		99, 227, 19, 147, 83, 211, 51, 179, 115, 243, 11, 139, 75, 203, 43, 171,
		107, 235, 27, 155, 91, 219, 59, 187, 123, 251, 7, 135, 71, 199, 39, 167,
		103, 231, 23, 151, 87, 215, 55, 183, 119, 247, 15, 143, 79, 207, 47,
		175, 111, 239, 31, 159, 95, 223, 63, 191, 127, 255 };

typedef struct {
	Word16 WtAddr;
	Word16 DataIn;
	Word16 RdAddr;
	Word16 DataOut;
	Word16 (*read_func)(Word16, Word16, void *);
	void (*write_func)(Word16, Word16, Word16, void *);
	Word16 mem[4][64];
} sram;

typedef struct {
	Word16 RdAddr;
	Word16 (*read_func)(Word16, Word16, void*);
	Word16 mem[3][64];
} rom;

static sram Mr;
static sram Mi;
static rom Rr;
static rom Ri; // twiddle factors

void Init_Twiddle() { // dimension = N/4
	Word32 i;
	for (i = 0; i < 64; i += 1) {
		Rr.mem[0][i] = 32767 * cosf(2 * pi * i / 256); // W(s, N)
		Ri.mem[0][i] = -32767 * sinf(2 * pi * i / 256);
		Rr.mem[1][i] = 32767 * cosf(2 * pi * (2 * i) / 256); // W(2s, N)
		Ri.mem[1][i] = -32767 * sinf(2 * pi * (2 * i) / 256);
		Rr.mem[2][i] = 32767 * cosf(2 * pi * (3 * i) / 256); // W(3s, N)
		Ri.mem[2][i] = -32767 * sinf(2 * pi * (3 * i) / 256);
	}
}

static void PE_R4(ComplexInt16 *a, ComplexInt16 *b, ComplexInt16 *c,
		ComplexInt16 *d) {

	btr_fly_r2(&a[0], &c[0]);
	btr_fly_r2(&b[0], &d[0]);

	d[0] = complex_swap(d[0]); // * -j
	btr_fly_r2(&a[0], &b[0]);
	btr_fly_r2(&c[0], &d[0]);
}

void cordic(Word32 gamma, ComplexInt16 *vector) { // 2*pi*addr/N, 2^32/256
	Word32 v0, v1;
	Word32 j;
	Word32 poweroftwo = 0;
	Word32 sigma;
	Word32 angle = angles[0];
	Word32 beta;
	ComplexInt32 vctr;

	if (gamma > 0x40000000 || gamma < -0x40000000) { // 2^30/2^32=(1/4)*2pi=pi/2
		beta = (gamma > 0) ? gamma - 0x80000000 : gamma + 0x80000000; // int32_t
		cordic(beta, vector);
		vector->real *= -1;
		vector->imag *= -1;
		return;

	} else {
		beta = gamma;
	}

	vctr.real = vector->real;
	vctr.imag = vector->imag;

	for (j = 0; j < 20; j += 1) {
		sigma = (beta < 0) ? -1 : 1;
		v0 = vctr.real;
		v1 = vctr.imag;
		vctr.real = v0 - (sigma * v1 >> poweroftwo);
		vctr.imag = (sigma * v0 >> poweroftwo) + v1;
		beta -= sigma * angle;
		angle = angles[j + 1];
		poweroftwo += 1;
	}
	vector->real = sat_32_16(vctr.real * 19898 >> 15);
	vector->imag = sat_32_16(vctr.imag * 19898 >> 15);
}

// process elements
static void PE(ComplexInt16 *a, ComplexInt16 *b, ComplexInt16 *c,
		ComplexInt16 *d, Word16 Addr, Word16 scale, Word16 *detect) {

#if !defined(CORDIC)
	ComplexInt16 X, Y, Z;
#endif

	a[0] = complex_asr(a[0], scale + 4); // (1+j)*W(1,8)+1=2.^0.5+1=2.41421
	b[0] = complex_asr(b[0], scale + 4); // (1+j)*W(1,8)+1=2.^0.5+1=2.41421
	c[0] = complex_asr(c[0], scale + 4); // (1+j)*W(1,8)+1=2.^0.5+1=2.41421
	d[0] = complex_asr(d[0], scale + 4); // (1+j)*W(1,8)+1=2.^0.5+1=2.41421

	PE_R4(a, b, c, d); // Addr, angle=2*pi*addr/N

#if !defined(CORDIC)
	X.real = Rr.read_func(Addr, 0, &Rr); // ROM0, W(s,N)
	X.imag = Ri.read_func(Addr, 0, &Ri);
	Y.real = Rr.read_func(Addr, 1, &Rr); // ROM1, W(2s,N)
	Y.imag = Ri.read_func(Addr, 1, &Ri);
	Z.real = Rr.read_func(Addr, 2, &Rr); // ROM2, W(3s,N)
	Z.imag = Ri.read_func(Addr, 2, &Ri);

	b[0] = complex_mul(Y, b[0]);
	c[0] = complex_mul(X, c[0]);
	d[0] = complex_mul(Z, d[0]);

#else
	cordic(-Addr * 2 << 24, b);
	cordic(-Addr * 1 << 24, c);
	cordic(-Addr * 3 << 24, d);

#endif

	detect[0] = exp_adj(a[0].real, detect[0]); // single BFP
	detect[0] = exp_adj(a[0].imag, detect[0]);
	detect[0] = exp_adj(b[0].real, detect[0]);
	detect[0] = exp_adj(b[0].imag, detect[0]);
	detect[0] = exp_adj(c[0].real, detect[0]);
	detect[0] = exp_adj(c[0].imag, detect[0]);
	detect[0] = exp_adj(d[0].real, detect[0]);
	detect[0] = exp_adj(d[0].imag, detect[0]);
}

static Word16 sram_read(Word16 Addr, Word16 bank, void *inst) {
	return (((sram *) inst)->mem[bank][Addr]);
}

static Word16 rom_read(Word16 Addr, Word16 bank, void *inst) {
	return (((rom *) inst)->mem[bank][Addr]);
}

static void sram_write(Word16 Addr, Word16 DataIn, Word16 bank, void *inst) {
	((sram *) inst)->mem[bank][Addr] = DataIn;
}

Word16 RD_ADDR_GEN(Word32 ctrl, Word32 bank, Word32 stage) {
	struct {
		UWord32 b0 :1;
		UWord32 b1 :1;
		UWord32 b2 :1;
		UWord32 b3 :1;
		UWord32 b4 :1;
		UWord32 b5 :1;
	} bit_set;

	Word16 value = 0;

	bit_set.b0 = (_Bool) (ctrl & 0x0001);
	bit_set.b1 = (_Bool) (ctrl & 0x0002);
	bit_set.b2 = (_Bool) (ctrl & 0x0004);
	bit_set.b3 = (_Bool) (ctrl & 0x0008);
	bit_set.b4 = (_Bool) (ctrl & 0x0010);
	bit_set.b5 = (_Bool) (ctrl & 0x0020);

	switch (stage) {
	case 2:
		switch (bank) {
		case 1:
			value = bit_set.b5 ^ bit_set.b4; // {b5^b4,b4,b3,b2,b1,b0}
			value = (value << 1) ^ bit_set.b4;
			value = (value << 1) ^ bit_set.b3;
			value = (value << 1) ^ bit_set.b2;
			value = (value << 1) ^ bit_set.b1;
			value = (value << 1) ^ bit_set.b0;

			break;
		case 2:
			value = bit_set.b5; // {b5, ~b4, b3, b2, b1, b0}
			value = (value << 1) ^ (~bit_set.b4 & 1);
			value = (value << 1) ^ bit_set.b3;
			value = (value << 1) ^ bit_set.b2;
			value = (value << 1) ^ bit_set.b1;
			value = (value << 1) ^ bit_set.b0;

			break;
		case 3:
			value = ~(bit_set.b5 ^ bit_set.b4) & 1; // {~(b5^b4),b4,b3,b2,b1,b0}
			value = (value << 1) ^ bit_set.b4;
			value = (value << 1) ^ bit_set.b3;
			value = (value << 1) ^ bit_set.b2;
			value = (value << 1) ^ bit_set.b1;
			value = (value << 1) ^ bit_set.b0;

			break;
		case 4:
			value = ~bit_set.b5 & 1; // {~b5,~b4,b[3:0]}
			value = (value << 1) ^ (~bit_set.b4 & 1);
			value = (value << 1) ^ bit_set.b3;
			value = (value << 1) ^ bit_set.b2;
			value = (value << 1) ^ bit_set.b1;
			value = (value << 1) ^ bit_set.b0;

			break;
		}
		break;
	case 3:
		switch (bank) {
		case 1: // {b5^b4,b4,b3^b2,b[2:0]}
			value = bit_set.b5 ^ bit_set.b4;
			value = (value << 1) ^ bit_set.b4;
			value = (value << 1) ^ (bit_set.b3 ^ bit_set.b2);
			value = (value << 1) ^ bit_set.b2;
			value = (value << 1) ^ bit_set.b1;
			value = (value << 1) ^ bit_set.b0;

			break;
		case 2: // {b5,~b4,b3,~b2,b1,b0}
			value = bit_set.b5;
			value = (value << 1) ^ (~bit_set.b4 & 1);
			value = (value << 1) ^ bit_set.b3;
			value = (value << 1) ^ (~bit_set.b2 & 1);
			value = (value << 1) ^ bit_set.b1;
			value = (value << 1) ^ bit_set.b0;

			break;
		case 3: // {~(b5^b4),b4,~(b3^b2),b[2:0]}
			value = ~(bit_set.b5 ^ bit_set.b4) & 1;
			value = (value << 1) ^ bit_set.b4;
			value = (value << 1) ^ (~(bit_set.b3 ^ bit_set.b2) & 1);
			value = (value << 1) ^ bit_set.b2;
			value = (value << 1) ^ bit_set.b1;
			value = (value << 1) ^ bit_set.b0;

			break;
		case 4: // {~b5,~b4,~b3,~b2,b[1:0]}
			value = ~bit_set.b5 & 1;
			value = (value << 1) ^ (~bit_set.b4 & 1);
			value = (value << 1) ^ (~bit_set.b3 & 1);
			value = (value << 1) ^ (~bit_set.b2 & 1);
			value = (value << 1) ^ bit_set.b1;
			value = (value << 1) ^ bit_set.b0;

			break;
		}
		break;
	case 4:
		switch (bank) {
		case 1:
			value = bit_set.b5 ^ bit_set.b4;
			value = (value << 1) ^ bit_set.b4;
			value = (value << 1) ^ (bit_set.b3 ^ bit_set.b2);
			value = (value << 1) ^ bit_set.b2;
			value = (value << 1) ^ (bit_set.b1 ^ bit_set.b0);
			value = (value << 1) ^ bit_set.b0;

			break;
		case 2:
			value = bit_set.b5;
			value = (value << 1) ^ (~bit_set.b4 & 1);
			value = (value << 1) ^ bit_set.b3;
			value = (value << 1) ^ (~bit_set.b2 & 1);
			value = (value << 1) ^ bit_set.b1;
			value = (value << 1) ^ (~bit_set.b0 & 1);

			break;
		case 3:
			value = ~(bit_set.b5 ^ bit_set.b4) & 1;
			value = (value << 1) ^ bit_set.b4;
			value = (value << 1) ^ (~(bit_set.b3 ^ bit_set.b2) & 1);
			value = (value << 1) ^ bit_set.b2;
			value = (value << 1) ^ (~(bit_set.b1 ^ bit_set.b0) & 1);
			value = (value << 1) ^ bit_set.b0;

			break;
		case 4:
			value = ~bit_set.b5 & 1;
			value = (value << 1) ^ (~bit_set.b4 & 1);
			value = (value << 1) ^ (~bit_set.b3 & 1);
			value = (value << 1) ^ (~bit_set.b2 & 1);
			value = (value << 1) ^ (~bit_set.b1 & 1);
			value = (value << 1) ^ (~bit_set.b0 & 1);

			break;
		}
		break;
	}
	return (value);
}

Word16 InPut_Sel(UWord32 ctrl, Word32 mux) { // for mux 5~8
	UWord32 b5 = (ctrl & 0x0002) >> 1;
	UWord32 b4 = ctrl & 0x0001;

	switch (mux) {
	case 5:
		return (ctrl ^ (b4 << 1)); // {b[5]^b[4], b[4]}
		break;
	case 6:
		return ((b5 << 1) | (0x0001 & ~b4)); // {b[5], ~b[4]}
		break;
	case 7:
		return (((0x0001 & ~(b5 ^ b4)) << 1) | b4); // {~(b[5]^b[4]), b[4]}
		break;
	case 8:
		return (0x0003 & ~ctrl); // {~b[5], ~b[4]}
		break;
	default:
		return (0);
		break;
	}
}

Word16 MUX_Sel(UWord32 ctrl, Word32 mux) {
	UWord32 b5 = (ctrl & 0x0002) >> 1;
	UWord32 b4 = ctrl & 0x0001;

	switch (mux) {
	case 1:
		return (0x0003 & ctrl); // {b[5], b[4]}
		break;
	case 2:
		return (((b5 ^ b4) << 1) | (0x0001 & ~b4)); // {b[5]^b[4], ~b[4]}
		break;
	case 3:
		return (((0x0001 & ~b5) << 1) | b4); // {~b[5], b[4]}
		break;
	case 4:
		return (((0x0001 & ~(b5 ^ b4)) << 1) | (0x0001 & ~b4)); // {~(b[5]^b[4]), ~b[4]}
		break;
	default:
		return (0);
		break;
	}
}

Word16 cfft_256(ComplexInt16 *Sin, ComplexInt16 *Sout) { // 256 16-bit complex values
	Word16 bfp_exp = 0;
	Word16 detect = -4;
	Word16 scale = -4;
	Word32 i;

	Mr.read_func = sram_read;
	Mr.write_func = sram_write;
	Mi.read_func = sram_read;
	Mi.write_func = sram_write;
	Rr.read_func = rom_read;
	Ri.read_func = rom_read;

	i = 0;
	do { // 0 ~ N/4-1
		Mr.mem[0][i] = Sin->real;
		Mi.mem[0][i] = Sin++->imag;
	} while (++i < 64);

	i = 0;
	do { // N/4 ~ N/2-1
		Mr.mem[1][i] = Sin->real;
		Mi.mem[1][i] = Sin++->imag;
	} while (++i < 64);

	i = 0;
	do { // N/2 ~ 3*N/4-1
		Mr.mem[2][i] = Sin->real;
		Mi.mem[2][i] = Sin++->imag;
	} while (++i < 64);

	i = 0;
	do { // 3*N/4 ~ N-1
		Mr.mem[3][i] = Sin->real;
		Mi.mem[3][i] = Sin++->imag;
	} while (++i < 64);

// Stage I
	for (i = 0; i < 64; i += 1) { // # of complex operations
		Word32 MUX_CTRL;
		Word32 MUX_CTRL1;
		Word32 MUX_CTRL2;
		Word32 MUX_CTRL3;

		ComplexInt16 tmp[4];

		tmp[0].real = Mr.read_func(i, 0, &Mr); // decimation in frequency
		tmp[0].imag = Mi.read_func(i, 0, &Mi); // 0, N/4, N/2, 3*N/4
		tmp[1].real = Mr.read_func(i, 1, &Mr);
		tmp[1].imag = Mi.read_func(i, 1, &Mi);
		tmp[2].real = Mr.read_func(i, 2, &Mr);
		tmp[2].imag = Mi.read_func(i, 2, &Mi);
		tmp[3].real = Mr.read_func(i, 3, &Mr);
		tmp[3].imag = Mi.read_func(i, 3, &Mi);

#if 0
		fwrite(&tmp[0].real, sizeof(Word16), 1, dptr_i);
		fwrite(&tmp[0].imag, sizeof(Word16), 1, dptr_i);
		fwrite(&tmp[1].real, sizeof(Word16), 1, dptr_i);
		fwrite(&tmp[1].imag, sizeof(Word16), 1, dptr_i);
		fwrite(&tmp[2].real, sizeof(Word16), 1, dptr_i);
		fwrite(&tmp[2].imag, sizeof(Word16), 1, dptr_i);
		fwrite(&tmp[3].real, sizeof(Word16), 1, dptr_i);
		fwrite(&tmp[3].imag, sizeof(Word16), 1, dptr_i);
#endif

		PE(&tmp[0], &tmp[1], &tmp[2], &tmp[3], i & 0x003f, -4, &detect);

		MUX_CTRL = InPut_Sel((i & 0x30) >> 4, 5); // Determine Write Address
		MUX_CTRL1 = InPut_Sel((i & 0x30) >> 4, 6); // i : counter
		MUX_CTRL2 = InPut_Sel((i & 0x30) >> 4, 7); // i[5:4] for ctrl
		MUX_CTRL3 = InPut_Sel((i & 0x30) >> 4, 8);

		Mr.write_func(i, tmp[MUX_CTRL].real, 0, &Mr); // b[5:0], 6-bit counter for 64 cycles
		Mi.write_func(i, tmp[MUX_CTRL].imag, 0, &Mi); // 64 cycles butterfly operations
		Mr.write_func(i, tmp[MUX_CTRL1].real, 1, &Mr); // Stage I to II
		Mi.write_func(i, tmp[MUX_CTRL1].imag, 1, &Mi);
		Mr.write_func(i, tmp[MUX_CTRL2].real, 2, &Mr); // bank2
		Mi.write_func(i, tmp[MUX_CTRL2].imag, 2, &Mi);
		Mr.write_func(i, tmp[MUX_CTRL3].real, 3, &Mr); // bank3
		Mi.write_func(i, tmp[MUX_CTRL3].imag, 3, &Mi);
	}

	scale = detect;
	bfp_exp += (scale + 4);
	detect = -4;

// Stage II
	for (i = 0; i < 64; i += 1) { // # of complex operations
		Word32 MUX_CTRL;
		Word32 MUX_CTRL1;
		Word32 MUX_CTRL2;
		Word32 MUX_CTRL3;

		Word16 ADDR;
		Word16 ADDR1;
		Word16 ADDR2;
		Word16 ADDR3;

		ComplexInt16 tmp[4];

		MUX_CTRL = InPut_Sel((i & 0x30) >> 4, 5); // Determine Read Address
		MUX_CTRL1 = InPut_Sel((i & 0x30) >> 4, 6);
		MUX_CTRL2 = InPut_Sel((i & 0x30) >> 4, 7);
		MUX_CTRL3 = InPut_Sel((i & 0x30) >> 4, 8);

		ADDR = RD_ADDR_GEN(i, 1, 2);
		ADDR1 = RD_ADDR_GEN(i, 2, 2);
		ADDR2 = RD_ADDR_GEN(i, 3, 2);
		ADDR3 = RD_ADDR_GEN(i, 4, 2);

		tmp[MUX_CTRL].real = Mr.read_func(ADDR, 0, &Mr);
		tmp[MUX_CTRL].imag = Mi.read_func(ADDR, 0, &Mi);
		tmp[MUX_CTRL1].real = Mr.read_func(ADDR1, 1, &Mr);
		tmp[MUX_CTRL1].imag = Mi.read_func(ADDR1, 1, &Mi);
		tmp[MUX_CTRL2].real = Mr.read_func(ADDR2, 2, &Mr);
		tmp[MUX_CTRL2].imag = Mi.read_func(ADDR2, 2, &Mi);
		tmp[MUX_CTRL3].real = Mr.read_func(ADDR3, 3, &Mr);
		tmp[MUX_CTRL3].imag = Mi.read_func(ADDR3, 3, &Mi);

#if 0
		fwrite(&tmp[0].real, sizeof(Word16), 1, dptr_i);
		fwrite(&tmp[0].imag, sizeof(Word16), 1, dptr_i);
		fwrite(&tmp[1].real, sizeof(Word16), 1, dptr_i);
		fwrite(&tmp[1].imag, sizeof(Word16), 1, dptr_i);
		fwrite(&tmp[2].real, sizeof(Word16), 1, dptr_i);
		fwrite(&tmp[2].imag, sizeof(Word16), 1, dptr_i);
		fwrite(&tmp[3].real, sizeof(Word16), 1, dptr_i);
		fwrite(&tmp[3].imag, sizeof(Word16), 1, dptr_i);
#endif

		PE(&tmp[0], &tmp[1], &tmp[2], &tmp[3], i << 2 & 0x003f, scale, &detect);

		MUX_CTRL = InPut_Sel((i & 0xc) >> 2, 5); // Write Address
		MUX_CTRL1 = InPut_Sel((i & 0xc) >> 2, 6);
		MUX_CTRL2 = InPut_Sel((i & 0xc) >> 2, 7);
		MUX_CTRL3 = InPut_Sel((i & 0xc) >> 2, 8);

		Mr.write_func(ADDR, tmp[MUX_CTRL].real, 0, &Mr); // Stage II to III
		Mi.write_func(ADDR, tmp[MUX_CTRL].imag, 0, &Mi);
		Mr.write_func(ADDR1, tmp[MUX_CTRL1].real, 1, &Mr);
		Mi.write_func(ADDR1, tmp[MUX_CTRL1].imag, 1, &Mi);
		Mr.write_func(ADDR2, tmp[MUX_CTRL2].real, 2, &Mr);
		Mi.write_func(ADDR2, tmp[MUX_CTRL2].imag, 2, &Mi);
		Mr.write_func(ADDR3, tmp[MUX_CTRL3].real, 3, &Mr);
		Mi.write_func(ADDR3, tmp[MUX_CTRL3].imag, 3, &Mi);
	}

	scale = detect;
	bfp_exp += (scale + 4);
	detect = -4;

// Stage III
	for (i = 0; i < 64; i += 1) { // # of complex operations
		Word32 MUX_CTRL;
		Word32 MUX_CTRL1;
		Word32 MUX_CTRL2;
		Word32 MUX_CTRL3;

		Word16 ADDR;
		Word16 ADDR1;
		Word16 ADDR2;
		Word16 ADDR3;

		ComplexInt16 tmp[4];

		MUX_CTRL = InPut_Sel((i & 0xc) >> 2, 5);
		MUX_CTRL1 = InPut_Sel((i & 0xc) >> 2, 6);
		MUX_CTRL2 = InPut_Sel((i & 0xc) >> 2, 7);
		MUX_CTRL3 = InPut_Sel((i & 0xc) >> 2, 8);

		ADDR = RD_ADDR_GEN(i, 1, 3);
		ADDR1 = RD_ADDR_GEN(i, 2, 3);
		ADDR2 = RD_ADDR_GEN(i, 3, 3);
		ADDR3 = RD_ADDR_GEN(i, 4, 3);

		tmp[MUX_CTRL].real = Mr.read_func(ADDR, 0, &Mr);
		tmp[MUX_CTRL].imag = Mi.read_func(ADDR, 0, &Mi);
		tmp[MUX_CTRL1].real = Mr.read_func(ADDR1, 1, &Mr);
		tmp[MUX_CTRL1].imag = Mi.read_func(ADDR1, 1, &Mi);
		tmp[MUX_CTRL2].real = Mr.read_func(ADDR2, 2, &Mr);
		tmp[MUX_CTRL2].imag = Mi.read_func(ADDR2, 2, &Mi);
		tmp[MUX_CTRL3].real = Mr.read_func(ADDR3, 3, &Mr);
		tmp[MUX_CTRL3].imag = Mi.read_func(ADDR3, 3, &Mi);

#if 0
		fwrite(&tmp[0].real, sizeof(Word16), 1, dptr_i);
		fwrite(&tmp[0].imag, sizeof(Word16), 1, dptr_i);
		fwrite(&tmp[1].real, sizeof(Word16), 1, dptr_i);
		fwrite(&tmp[1].imag, sizeof(Word16), 1, dptr_i);
		fwrite(&tmp[2].real, sizeof(Word16), 1, dptr_i);
		fwrite(&tmp[2].imag, sizeof(Word16), 1, dptr_i);
		fwrite(&tmp[3].real, sizeof(Word16), 1, dptr_i);
		fwrite(&tmp[3].imag, sizeof(Word16), 1, dptr_i);
#endif

		PE(&tmp[0], &tmp[1], &tmp[2], &tmp[3], i << 4 & 0x003f, scale, &detect);

		MUX_CTRL = InPut_Sel(i & 0x3, 5); // Stage III to IV
		MUX_CTRL1 = InPut_Sel(i & 0x3, 6);
		MUX_CTRL2 = InPut_Sel(i & 0x3, 7);
		MUX_CTRL3 = InPut_Sel(i & 0x3, 8);

		Mr.write_func(ADDR, tmp[MUX_CTRL].real, 0, &Mr);
		Mi.write_func(ADDR, tmp[MUX_CTRL].imag, 0, &Mi);
		Mr.write_func(ADDR1, tmp[MUX_CTRL1].real, 1, &Mr);
		Mi.write_func(ADDR1, tmp[MUX_CTRL1].imag, 1, &Mi);
		Mr.write_func(ADDR2, tmp[MUX_CTRL2].real, 2, &Mr);
		Mi.write_func(ADDR2, tmp[MUX_CTRL2].imag, 2, &Mi);
		Mr.write_func(ADDR3, tmp[MUX_CTRL3].real, 3, &Mr);
		Mi.write_func(ADDR3, tmp[MUX_CTRL3].imag, 3, &Mi);
	}

	scale = detect;
	bfp_exp += (scale + 4);
	detect = -4;

// Stage IV

	for (i = 0; i < 64; i += 1) { // # of complex operations
		Word32 MUX_CTRL;
		Word32 MUX_CTRL1;
		Word32 MUX_CTRL2;
		Word32 MUX_CTRL3;

		Word16 ADDR;
		Word16 ADDR1;
		Word16 ADDR2;
		Word16 ADDR3;

		ComplexInt16 tmp[4];

		MUX_CTRL = InPut_Sel(i & 0x3, 5);
		MUX_CTRL1 = InPut_Sel(i & 0x3, 6);
		MUX_CTRL2 = InPut_Sel(i & 0x3, 7);
		MUX_CTRL3 = InPut_Sel(i & 0x3, 8);

		ADDR = RD_ADDR_GEN(i, 1, 4);
		ADDR1 = RD_ADDR_GEN(i, 2, 4);
		ADDR2 = RD_ADDR_GEN(i, 3, 4);
		ADDR3 = RD_ADDR_GEN(i, 4, 4);

		tmp[MUX_CTRL].real = Mr.read_func(ADDR, 0, &Mr);
		tmp[MUX_CTRL].imag = Mi.read_func(ADDR, 0, &Mi);
		tmp[MUX_CTRL1].real = Mr.read_func(ADDR1, 1, &Mr);
		tmp[MUX_CTRL1].imag = Mi.read_func(ADDR1, 1, &Mi);
		tmp[MUX_CTRL2].real = Mr.read_func(ADDR2, 2, &Mr);
		tmp[MUX_CTRL2].imag = Mi.read_func(ADDR2, 2, &Mi);
		tmp[MUX_CTRL3].real = Mr.read_func(ADDR3, 3, &Mr);
		tmp[MUX_CTRL3].imag = Mi.read_func(ADDR3, 3, &Mi);

#if 0
		fwrite(&tmp[0].real, sizeof(Word16), 1, dptr_i);
		fwrite(&tmp[0].imag, sizeof(Word16), 1, dptr_i);
		fwrite(&tmp[1].real, sizeof(Word16), 1, dptr_i);
		fwrite(&tmp[1].imag, sizeof(Word16), 1, dptr_i);
		fwrite(&tmp[2].real, sizeof(Word16), 1, dptr_i);
		fwrite(&tmp[2].imag, sizeof(Word16), 1, dptr_i);
		fwrite(&tmp[3].real, sizeof(Word16), 1, dptr_i);
		fwrite(&tmp[3].imag, sizeof(Word16), 1, dptr_i);
#endif

		PE(&tmp[0], &tmp[1], &tmp[2], &tmp[3], i << 6 & 0x003f, scale, &detect); // bit reverse order

#if 1
		Sout[bit_rev_tab[4 * i]].real = tmp[0].real;
		Sout[bit_rev_tab[4 * i]].imag = tmp[0].imag;
		Sout[bit_rev_tab[4 * i + 1]].real = tmp[1].real;
		Sout[bit_rev_tab[4 * i + 1]].imag = tmp[1].imag;
		Sout[bit_rev_tab[4 * i + 2]].real = tmp[2].real;
		Sout[bit_rev_tab[4 * i + 2]].imag = tmp[2].imag;
		Sout[bit_rev_tab[4 * i + 3]].real = tmp[3].real;
		Sout[bit_rev_tab[4 * i + 3]].imag = tmp[3].imag;
#else
		Sout->real = tmp[0].real;
		Sout++->imag = tmp[0].imag;
		Sout->real = tmp[1].real;
		Sout++->imag = tmp[1].imag;
		Sout->real = tmp[2].real;
		Sout++->imag = tmp[2].imag;
		Sout->real = tmp[3].real;
		Sout++->imag = tmp[3].imag;
#endif
	}

	scale = detect;
	detect = -4;
	return (bfp_exp);
}
