/*
 * rfft_256.h
 *
 *  Created on: 2015¦~7¤ë20¤é
 *      Author: ite01527
 */

#ifndef RFFT_256_H_
#define RFFT_256_H_

#ifndef FIXED_POINT
#define FIXED_POINT
#endif
#define NFFT 256
#define xi_min .0316f // -15dB
#define Gmin .0316f // -30dB
#define U_Size 4
#define V_Size 32
#define end_startup 64
#define PartLen (NFFT>>1)
#define PartLen1 (PartLen+1)

extern Word32 CB_FREQ_INDICES_256[129];
extern Word32 CB_OFST[];
extern const Word32 sprd_table[][18];
extern Word32 T_ABS[];

enum {
	Wiener_Mode = 0, Auditory_Mask_Mode, Wind_Mode
};

typedef struct {
	Word16 buffer[128]; // prev
	Word16 sc_buffer[256]; // single channel

	Word32 reset;
	Word32 cntr;
	Word32 mode;
#if !defined(FIXED_POINT)
	Float32 ad;
	Float32 Bmin;
	Float32 gamma0;
	Float32 gamma1;
	Float32 zeta0;
	Float32 alpha;
	Float32 alpha_s;
	Float32 beta;
	Float32 c;
	Float32 S[1 + NFFT / 2];
	Float32 S_tild[1 + NFFT / 2];
	Float32 Smin[1 + NFFT / 2];
	Float32 Smin_sw[1 + NFFT / 2];
	Float32 Smin_tild[1 + NFFT / 2];
	Float32 Smin_sw_tild[1 + NFFT / 2];
	Float32 gamma_prev[1 + NFFT / 2];
	Float32 lambda[1 + NFFT / 2];
	Float32 xi[1 + NFFT / 2];
	Float32 Gh1[1 + NFFT / 2];
	Float32 alpha_d[1 + NFFT / 2];
	Float32 q[1 + NFFT / 2];
	Float32 str_min[1 + NFFT / 2][U_Size];
	Float32 str_min_tild[1 + NFFT / 2][U_Size];
	Float32 ps[1 + NFFT / 2];
	Float32 qs[1 + NFFT / 2];
	Float32 nu[1 + NFFT / 2];
	Float32 noise_b[1 + NFFT / 2];
#else
	Word32 p[PartLen1]; // smooth over time
	Word32 blockInd; // frame idx
	Word32 qs[PartLen1];
	Word32 Bmin;
	Word32 beta;
	Word32 gamma_prev[PartLen1];
	Word32 xi[PartLen1];
	Word32 nu[PartLen1];
	Word32 Gh1[PartLen1];
	Word64 S[PartLen1];
	Word64 S_tild[PartLen1];
	Word64 Smin[PartLen1];
	Word64 Smin_sw[PartLen1];
	Word64 Smin_tild[PartLen1];
	Word64 Smin_sw_tild[PartLen1];
	Word64 lambda[PartLen1];
	Word64 noise_b[PartLen1];
	Word64 Sxx[PartLen1];
	Word64 Nxx[PartLen1];
#endif

} nr_state;

typedef Word32 (*func_ptr)(Complex16_t *, Complex16_t *, Word32);

extern nr_state NR_STATE_TX;
extern nr_state NR_STATE_RX;
extern Word16 twiddle_fact256[512];
extern Word16 twiddle_fact512[512];

void init_nr_core(nr_state *st, Word32 mode);
Word32 cfft_128(Complex16_t *Sin, Complex16_t *twd, Word32 N);
Word32 cfft_128_temp(Complex16_t *Sin, Complex16_t *twd, Word32 N);
Word32 cFFT_256(ComplexInt16 *Sin, ComplexInt16 *Twd, Word32 N);
Word32 rfft_256(Complex16_t *Sin, Word32 N);
Word32 rfft_512(ComplexInt16 *Sin, Word32 N);
void hs_ifft_256(Complex16_t *p, Word16 *out, Word32 N);
void hs_ifft_512(Complex16_t *p, Word16 *out, Word32 N);
void noise_reduction(nr_state *, Complex16_t *Sin, Word32 blk_exponent,
		Word32 N);
void noise_suppression(nr_state *Inst, ComplexInt16 *Sin, Word32 blk_exponent);
void fifo_ctrl(Word16 *Sin, Word16 *Sout, nr_state *NRI);
void mask_thresh(Word64 *ps, Word64 * const mask, Word32 N, Word32 *idx);
void Overlap_Add(Word16 *Sin, Word16 *Sout, nr_state *NRI);

#endif /* RFFT_256_H_ */
