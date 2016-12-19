/*
 * aecm_core.h
 *
 *  Created on: 2015¦~11¤ë12¤é
 *      Author: ite01527
 */

#ifndef AECM_CORE_H_
#define AECM_CORE_H_
//#define PBFDAF_FLAG

Word32 aecm_core(Word16 * const __restrict Sin, Word16 * const __restrict Rin,
		Word16 * const out, void * __restrict st);
void FreqWarpping(Word16 * __restrict Sin, Word16 * __restrict Rin, Word16 *out,
		void * __restrict Inst);

#endif /* AECM_CORE_H_ */
