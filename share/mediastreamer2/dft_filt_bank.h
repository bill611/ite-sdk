/*
 * dft_filt_bank.h
 *
 *  Created on: 2015¦~3¤ë24¤é
 *      Author: ite01527
 */

#ifndef DFT_FILT_BANK_H_
#define DFT_FILT_BANK_H_
//#define INLMS
//#define EXPAND
//#define LEAK_LMS
//#define FREQ_IDT
#define DUAL_BAND

#ifndef DUAL_BAND
#define TAIL_LENGTH 28
#else
#define TAIL_LENGTH 16 // 48ms, L=384/R
#endif

#ifndef FIXED_POINT
#define FIXED_POINT
#endif

//#define beta0 98 // 2 * R / Fs
#define beta0 197
#define beta_max 49
#define EXPAND_KNEE 32
#define eps 1074L // 1e-5(1e-6), Q30
#define END_STARTUP (2*TAIL_LENGTH)
#define BAND_LOW 1
#define BAND_HIGH 14

typedef struct dft_filt_bank_state {
	Word32 cntr; // 0
	Word32 blockInd;
#ifndef DUAL_BAND
	Word32 Syy[9];
	Word32 See[9];
	Word32 rate[9];
#ifdef EXPAND
	Word32 Ev[9];
#endif
#ifdef INLMS
	Float32 eta[9];
	ComplexFloat32 psi[9][TAIL_LENGTH];
#else
	Word32 PY[9];
	Word32 PE[9];
	Word32 leak_esti[9];
#ifdef FIXED_POINT
	Float32 RYY;
	Float32 REY;
	Word64 PYY[9];
	Word64 PEY[9];
#else
	Float32 PYY[9];
	Float32 PEY[9];
#endif
#endif
	Complex16_t S_y[9];
	Complex16_t S_e[9];
	Complex32_t Sed[9];
	Word16 buffer_far[144];
	Word16 buffer_near[144];
	Word16 buffer_sys[16][12]; // ifft synthesis
	Complex16_t buffer_sbc_filt[9][TAIL_LENGTH];
	Complex32_t weight[9][TAIL_LENGTH];
#else
	Word16 ng_noise_dur[17];
	Word32 ng_gain[17];
	Word32 Syy[17];
	Word32 See[17];
	Word32 Sdd[17];
	Word32 rate[17];
#ifdef EXPAND
	Word32 Ev[17];
#endif
#ifdef INLMS
	Float32 eta[17];
	ComplexFloat32 psi[17][TAIL_LENGTH];
#else
	Word32 PY[17];
	Word32 PE[17];
	Word32 leak_esti[17];
#ifdef FIXED_POINT
	Float32 REY;
	Float32 RYY;
	Word64 PYY[17];
	Word64 PEY[17];
#else
	Float32 PYY[17];
	Float32 PEY[17];
#endif
	Word32 gamma_echo_prev[17];
	Word32 gh1[17];
#endif
	Complex16_t S_y[17];
	Complex16_t S_e[17];
	Complex32_t Sed[17];
	Word16 buffer_far[264];
	Word16 buffer_near[264];
	Word16 buffer_sys[32][11]; // ifft synthesis
	Complex16_t buffer_sbc_filt[17][TAIL_LENGTH];
	Complex32_t weight[17][TAIL_LENGTH];
#endif
} sbc_aec_state;

#ifdef DUAL_BAND
typedef Word16 (*array_ptr)[24];
#else
typedef Word16 (*array_ptr)[12];
#endif

void init_twd();
void init_sbc_aec_state(sbc_aec_state *st);
void cfft_16_r4(Complex16_t input[], Complex16_t twd[]);
void rfft_32(Word16 *p, Complex16_t *twd);
void hs_ifft_32(Complex16_t *p, Word16 *out, Complex16_t *twd);
void dft_filt_bank(sbc_aec_state *st, const Word16 * const near,
		const Word16 * const far, Word16 *out, UWord32 *debug);
void speex_get_residual_echo(sbc_aec_state *st, Complex16_t *S_y,
		Complex16_t *Sout);
void get_residual_echo(sbc_aec_state *st, Complex16_t *Sin, Complex16_t *Nin,
		Complex16_t *S_y, Complex16_t *Sout);

extern ComplexInt16 twd[16];
extern ComplexInt16 twd_32[16];
extern sbc_aec_state state;
extern FILE *dptr;
extern FILE *dptr_i;
extern FILE *dptr_ii;
extern FILE *dptr_iii;
extern FILE *dptr_iv;

#endif
