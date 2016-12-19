/*
 * hd_aec.h
 *
 *  Created on: 2015¦~9¤ë21¤é
 *      Author: ite01527
 */

#ifndef HD_AEC_H_
#define HD_AEC_H_

#define nFFT 128
#define nDFT 256
#define part_len (nDFT >> 1)
#define part_len1 (part_len + 1)
#define part_len2 (part_len << 1)
#define part_len4 (part_len << 2)
#define PART_LEN 256
#define PART_LEN1 (PART_LEN + 1)
#define PART_LEN2 (PART_LEN << 1)
#define PART_LEN4 (PART_LEN << 2)
#define BARK_LEN 22
#define M 4 // tail-length 64ms
#define Q 3 // tail-length 256/16*3=48ms
#define epsilon 1
#define thresh 3.8147e-6
//#define thresh 2e-6
#define FREQ_BIN_LOW 5
#define FREQ_BIN_HIGH 54
#define NLP_COMP_LOW 0.25f
#define SUPGAIN_DEFAULT 16
#define Noise_Floor 33

typedef struct {
	Word16 buffer_rrin[nFFT];
	Word16 buffer_ssin[nFFT];
	Word16 ee_buffer[nFFT];
	Word16 yy_buffer[nFFT];
	Word16 dd_buffer[nFFT];
	Complex32_t XFm[M][1 + nFFT / 2]; // M blocks
#if !defined(FIXED_POINT)
	ComplexFloat32 WFb[M][1 + nFFT / 2];
#else
	Complex32_t WFb[M][1 + nFFT / 2];
#endif
	Word64 pn[1 + nFFT / 2];
} FDAF_STATE;

typedef struct {
	Word16 alpha_m;
	Word16 currentVADvalue;
	Word16 timer;
	Word16 nlpFlag;
	Word16 buffer_rrin[nDFT];
	Word16 buffer_ssin[nDFT];
	Word16 buffer_olpa[nDFT / 2];
	UWord32 seed;
	ComplexInt32 XFm[M][part_len1];
	Word32 Gainno1[part_len1]; // for residual echo estimations
	Word32 Gainno2[part_len1]; // for roughly speech estimations
	Word32 Gainno3[part_len1]; // for parallel apriori SNR
	Word32 gammano1[part_len1]; // post snr over echo-free near
	Word32 gammano2[part_len1]; // post snr over residual echo
	Word32 gammano3[part_len1]; // masked
	Word64 S_ss[M][part_len1]; // S_ss=&S_ss[0], Word64 (* const S_ss)[1+nDFT/2]
	Word64 S_vs[M][part_len1 << 1]; // *(S_ss+1)=S_ss[1], S_ss = malloc(sizeof(ptr)*nBands)
	Word64 S_nn[part_len1]; // comfort noise
	Word64 S_ee[part_len1]; // error signals
	Word64 lambda[part_len1];
	Word64 nearFilt[part_len1]; // filtered near-end magnitude
	Word64 echoEst64Gained[part_len1]; // filtered echo magnitude
	Word16 qDomainOld[M][part_len1];
} FDSR_STATE;

typedef struct {
	Word16 timer;
	Word16 buffer[nFFT / 2];
	UWord32 seed;
	Word32 diverge_state;
	Word32 Gainno1[1 + nFFT / 2]; // for residual echo estimations
	Word32 Gainno2[1 + nFFT / 2]; // perceptual masked gain
	Word32 Gainno3[1 + nFFT / 2]; // apply NLP
	Word32 gammano1[1 + nFFT / 2];
	Word64 Sed[1 + nFFT / 2];
	Word64 See[1 + nFFT / 2];
	Word64 Sdd[1 + nFFT / 2];
	Word64 Syy[1 + nFFT / 2];
	Word64 Sym[1 + nFFT / 2];
	Word64 lambda[1 + nFFT / 2];
} NLP_STATE;

typedef struct {
	Word16 xBuf[PART_LEN2];
	Word16 dBufNoisy[PART_LEN2];
	Word16 outBuf[PART_LEN2];
	UWord32 nearFilt[BARK_LEN];
	UWord32 echoFilt[BARK_LEN];
	UWord32 XFm[Q][BARK_LEN];
	UWord64 Sxx[Q][BARK_LEN];
	UWord64 Sxy[Q][BARK_LEN];
} AecmCore;

typedef union {
	Complex16_t a;
	Word32 b; // little endian or big endian ?
} C16_t;

extern FDAF_STATE aec_state; // single channel instance
extern NLP_STATE nlp_state;
extern FDSR_STATE anc_state;
extern FDSR_STATE agc_state;
extern AecmCore aecm;

extern/*C16_t*/Word16 TWD_64[];
extern/*C16_t*/Word16 TWD_128[];

Word32 cfft_64(Complex16_t *Sin, Complex16_t *TWD);
Word32 rfft_128(Complex16_t *Sin);
void hs_ifft_128(Complex16_t *p, Word16 *out);
void Overlap_Save(Word16 *Sin, Word16 *Sout);
void NLP_Init(NLP_STATE *st);
void PBFDAF_Init(FDAF_STATE *st);
void PBFDSR_Init(FDSR_STATE *st);
Word32 norm2Word16(void *Sin, Word32 N);
Word32 PBFDSR(Word16 * const __restrict Sin, Word16 * const __restrict Rin,
		Word16 * const out, FDSR_STATE * __restrict st);
void PBFDAF(Word16 * const __restrict Sin, Word16 * const __restrict Rin,
		Word16 * const out, FDAF_STATE * __restrict st);
void PBFDAF_RAES(FDAF_STATE *ft, NLP_STATE *st, Word16 *ee, Word16 *dd,
		Word16 *yy, Word16 *out);
void PAES_Process_Block(AecmCore * __restrict aecm,
		const Word16 * __restrict Sin, const Word16 * __restrict Rin,
		Word16 *output);
void BandSplit(Word16 *InPut, Word16 *OutL, Word16 *OutH, Word16 (*buffer)[32]);
void BandSynthesis(Word16 *InPutL, Word16 *InPutH, Word16 *Out);

#endif /* HD_AEC_H_ */
