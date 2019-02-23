/*
* Bitwise range encoder by Igor Pavlov
* Modified by Conor McCarthy
*
* Public domain
*/

#include "fl2_internal.h"
#include "mem.h"
#include "platform.h"
#include "range_enc.h"

const unsigned price_table[kBitModelTotal >> kNumMoveReducingBits] = {
    128, 103,  91,  84,  78,  73,  69,  66,
    63,  61,  58,  56,  54,  52,  51,  49,
    48,  46,  45,  44,  43,  42,  41,  40,
    39,  38,  37,  36,  35,  34,  34,  33,
    32,  31,  31,  30,  29,  29,  28,  28,
    27,  26,  26,  25,  25,  24,  24,  23,
    23,  22,  22,  22,  21,  21,  20,  20,
    19,  19,  19,  18,  18,  17,  17,  17,
    16,  16,  16,  15,  15,  15,  14,  14,
    14,  13,  13,  13,  12,  12,  12,  11,
    11,  11,  11,  10,  10,  10,  10,   9,
    9,   9,   9,   8,   8,   8,   8,   7,
    7,   7,   7,   6,   6,   6,   6,   5,
    5,   5,   5,   5,   4,   4,   4,   4,
    3,   3,   3,   3,   3,   2,   2,   2,
    2,   2,   2,   1,   1,   1,   1,   1
};

void RC_setOutputBuffer(RangeEncoder* const rc, BYTE *const out_buffer, size_t chunk_size)
{
    rc->out_buffer = out_buffer;
    rc->chunk_size = chunk_size;
    rc->out_index = 0;
}

void RC_reset(RangeEncoder* const rc)
{
    rc->low = 0;
    rc->range = (U32)-1;
    rc->cache_size = 0;
    rc->cache = 0;
}

#ifdef __64BIT__

void FORCE_NOINLINE RC_shiftLow(RangeEncoder* const rc)
{
    U64 low = rc->low;
    rc->low = (U32)(low << 8);
    if (low < 0xFF000000 || low > 0xFFFFFFFF) {
        BYTE high = (BYTE)(low >> 32);
        rc->out_buffer[rc->out_index++] = rc->cache + high;
        rc->cache = (BYTE)(low >> 24);
        if (rc->cache_size != 0) {
            high += 0xFF;
            do {
                rc->out_buffer[rc->out_index++] = high;
            } while (--rc->cache_size != 0);
        }
    }
    else {
        rc->cache_size++;
    }
}

#else

void FORCE_NOINLINE RC_shiftLow(RangeEncoder* const rc)
{
    U32 low = (U32)rc->low;
    unsigned high = (unsigned)(rc->low >> 32);
    rc->low = low << 8;
    if (low < (U32)0xFF000000 || high != 0) {
        rc->out_buffer[rc->out_index++] = rc->cache + (BYTE)high;
        rc->cache = (BYTE)(low >> 24);
        if (rc->cache_size != 0) {
            high += 0xFF;
            do {
                rc->out_buffer[rc->out_index++] = (BYTE)high;
            } while (--rc->cache_size != 0);
        }
    }
    else {
        rc->cache_size++;
    }
}

#endif

void RC_encodeBitTree(RangeEncoder* const rc, Probability *const probs, unsigned bit_count, unsigned symbol)
{
    assert(bit_count > 1);
    --bit_count;
    unsigned bit = symbol >> bit_count;
    RC_encodeBit(rc, &probs[1], bit);
    size_t tree_index = 1;
    do {
        --bit_count;
        tree_index = (tree_index << 1) | bit;
        bit = (symbol >> bit_count) & 1;
        RC_encodeBit(rc, &probs[tree_index], bit);
    } while (bit_count != 0);
}

void RC_encodeBitTreeReverse(RangeEncoder* const rc, Probability *const probs, unsigned bit_count, unsigned symbol)
{
    assert(bit_count != 0);
    unsigned bit = symbol & 1;
    RC_encodeBit(rc, &probs[1], bit);
    unsigned tree_index = 1;
    while (--bit_count != 0) {
        tree_index = (tree_index << 1) + bit;
        symbol >>= 1;
        bit = symbol & 1;
		RC_encodeBit(rc, &probs[tree_index], bit);
	}
}

void FORCE_NOINLINE RC_encodeDirect(RangeEncoder* const rc, unsigned value, unsigned bit_count)
{
	assert(bit_count > 0);
	do {
        rc->range >>= 1;
		--bit_count;
        rc->low += rc->range & -((int)(value >> bit_count) & 1);
		if (rc->range < kTopValue) {
            rc->range <<= 8;
			RC_shiftLow(rc);
		}
	} while (bit_count != 0);
}


