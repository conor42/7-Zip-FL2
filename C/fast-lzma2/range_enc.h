/*
* Bitwise range encoder by Igor Pavlov
* Modified by Conor McCarthy
*
* Public domain
*/

#ifndef RANGE_ENCODER_H
#define RANGE_ENCODER_H

#include "mem.h"
#include "compiler.h"

#if defined (__cplusplus)
extern "C" {
#endif

#ifdef LZMA_ENC_PROB32
typedef U32 Probability;
#else
typedef U16 Probability;
#endif

#define kNumTopBits 24U
#define kTopValue (1UL << kNumTopBits)
#define kNumBitModelTotalBits 11U
#define kBitModelTotal (1 << kNumBitModelTotalBits)
#define kNumMoveBits 5U
#define kProbInitValue (kBitModelTotal >> 1U)
#define kNumMoveReducingBits 4U
#define kNumBitPriceShiftBits 4U

extern const unsigned price_table[kBitModelTotal >> kNumMoveReducingBits];

typedef struct
{
	BYTE *out_buffer;
	size_t out_index;
	size_t chunk_size;
	U64 cache_size;
	U64 low;
	U32 range;
	BYTE cache;
} RangeEncoder;

void RC_reset(RangeEncoder* const rc);

void RC_setOutputBuffer(RangeEncoder* const rc, BYTE *const out_buffer, size_t chunk_size);

void RC_shiftLow(RangeEncoder* const rc);

void RC_encodeBitTree(RangeEncoder* const rc, Probability *const probs, unsigned bit_count, unsigned symbol);

void RC_encodeBitTreeReverse(RangeEncoder* const rc, Probability *const probs, unsigned bit_count, unsigned symbol);

void RC_encodeDirect(RangeEncoder* const rc, unsigned value, unsigned bit_count);

HINT_INLINE
void RC_encodeBit0(RangeEncoder* const rc, Probability *const rprob)
{
	unsigned prob = *rprob;
    rc->range = (rc->range >> kNumBitModelTotalBits) * prob;
	prob += (kBitModelTotal - prob) >> kNumMoveBits;
	*rprob = (Probability)prob;
	if (rc->range < kTopValue) {
        rc->range <<= 8;
		RC_shiftLow(rc);
	}
}

HINT_INLINE
void RC_encodeBit1(RangeEncoder* const rc, Probability *const rprob)
{
	unsigned prob = *rprob;
	U32 new_bound = (rc->range >> kNumBitModelTotalBits) * prob;
    rc->low += new_bound;
    rc->range -= new_bound;
	prob -= prob >> kNumMoveBits;
	*rprob = (Probability)prob;
	if (rc->range < kTopValue) {
        rc->range <<= 8;
		RC_shiftLow(rc);
	}
}

HINT_INLINE
void RC_encodeBit(RangeEncoder* const rc, Probability *const rprob, unsigned const bit)
{
	unsigned prob = *rprob;
	if (bit != 0) {
		U32 const new_bound = (rc->range >> kNumBitModelTotalBits) * prob;
        rc->low += new_bound;
        rc->range -= new_bound;
		prob -= prob >> kNumMoveBits;
	}
	else {
        rc->range = (rc->range >> kNumBitModelTotalBits) * prob;
		prob += (kBitModelTotal - prob) >> kNumMoveBits;
	}
	*rprob = (Probability)prob;
	if (rc->range < kTopValue) {
        rc->range <<= 8;
		RC_shiftLow(rc);
	}
}

#define GET_PRICE(prob, symbol) \
  price_table[((prob) ^ ((-(int)(symbol)) & (kBitModelTotal - 1))) >> kNumMoveReducingBits]

#define GET_PRICE_0(prob) price_table[(prob) >> kNumMoveReducingBits]

#define GET_PRICE_1(prob) price_table[((prob) ^ (kBitModelTotal - 1)) >> kNumMoveReducingBits]

HINT_INLINE
unsigned RC_getTreePrice(const Probability* const prob_table, unsigned const bit_count, size_t symbol)
{
	unsigned price = 0;
	symbol |= ((size_t)1 << bit_count);
	while (symbol != 1) {
		size_t const next_symbol = symbol >> 1;
		unsigned prob = prob_table[next_symbol];
		unsigned bit = (unsigned)symbol & 1;
		price += GET_PRICE(prob, bit);
		symbol = next_symbol;
	}
	return price;
}

HINT_INLINE
unsigned RC_getReverseTreePrice(const Probability* const prob_table, unsigned const bit_count, size_t symbol)
{
	unsigned price = 0;
	size_t m = 1;
	for (unsigned i = bit_count; i != 0; --i) {
		unsigned const prob = prob_table[m];
		unsigned const bit = symbol & 1;
		symbol >>= 1;
		price += GET_PRICE(prob, bit);
		m = (m << 1) | bit;
	}
	return price;
}

HINT_INLINE
void RC_flush(RangeEncoder* const rc)
{
    for (int i = 0; i < 5; ++i)
        RC_shiftLow(rc);
}

#if defined (__cplusplus)
}
#endif

#endif /* RANGE_ENCODER_H */