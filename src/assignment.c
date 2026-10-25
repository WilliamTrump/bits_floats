#include "assignment.h"

#include <math.h>
#include <stddef.h>

unsigned bitset_symmetric_difference_count(uint32_t a, uint32_t b) {
    uint32_t x = a ^ b;
    unsigned count = 0;

    for (unsigned i = 0; i < 32; i++) {
        count += (x >> i) & 1u;
    }
    return count;

}

uint32_t bit_range_mask(unsigned low, unsigned high) {
    //start with 32 1s, shift it right by 32 - width
    uint32_t ones = UINT32_MAX >> (32u - (high - low + 1));
    return ones << low;
}

uint32_t splice_bits(
    uint32_t destination,
    uint32_t source,
    unsigned low,
    unsigned high
) {
    uint32_t mask = bit_range_mask(low, high);
    return (destination & ~mask) | ((source << low) & mask);
}

int32_t decode_twos_complement(uint32_t bits, unsigned width) {
    uint32_t mask = UINT32_MAX >> (32u - width);
    uint32_t value = bits & mask;
    uint32_t sign = 1u << (width - 1u);\


    if ((value & sign) == 0) {
        return (int32_t)value;
    }


    uint32_t extended = value | ~mask;
    return -(int32_t)(~extended) - 1;
}

bool add_signed_width(
    uint32_t a_bits,
    uint32_t b_bits,
    unsigned width,
    uint32_t *sum_bits
) {
    uint32_t mask = UINT32_MAX >> (32u - width);
    uint32_t sign = 1u << (width - 1u);

    uint32_t a = a_bits & mask;
    uint32_t b = b_bits & mask;
    uint32_t s = (a + b) & mask;

    *sum_bits = s;

    bool a_neg = (a & sign) != 0;
    bool b_neg = (b & sign) != 0;
    bool s_neg = (s & sign) != 0;

    return (a_neg == b_neg) && (s_neg != a_neg);
}

int compare_signed_unsigned_math(
    int32_t signed_value,
    uint32_t unsigned_value
) {
    if (signed_value < 0) {
        return -1;
    }

    uint32_t s = (uint32_t)signed_value;

    if (s < unsigned_value) {
        return -1;
    }
    if (s > unsigned_value) {
        return 1;
    }
    return 0;

}

uint32_t multiply_by_24(uint32_t x) {
    return (x << 5) - (x << 3);
}

fp32_info_t fp32_decode(uint32_t bits) {
    uint32_t exp  = (bits >> 23) & 0xFFu;
    uint32_t frac = bits & 0x7FFFFFu;

    fp32_info_t info = {
        .negative = (bits >> 31) != 0,
        .exponent_bits = (uint8_t)exp,
        .fraction_bits = frac,
    };

    if (exp == 0) {
        info.kind = frac ? FP32_SUBNORMAL : FP32_ZERO;
        info.unbiased_exponent = -126;
    } else if (exp == 0xFFu) {
        info.kind = frac ? FP32_NAN : FP32_INFINITY;
        info.unbiased_exponent = 0;
    } else {
        info.kind = FP32_NORMAL;
        info.unbiased_exponent = (int)exp - 127;
    }
    return info;
}


double fp32_exact_value(uint32_t bits) {
    fp32_info_t info = fp32_decode(bits);
    double sign = info.negative ? -1.0 : 1.0;

    if (info.kind == FP32_NAN) {
        return NAN;
    }
    if (info.kind == FP32_INFINITY) {
        return sign * INFINITY;
    }

    double m = info.fraction_bits;      
    if (info.kind == FP32_NORMAL) {
        m += 0x800000;                  
    }

    for (int e = info.unbiased_exponent - 23; e < 0; e++) {
        m *= 0.5;
    }
    for (int e = info.unbiased_exponent - 23; e > 0; e--) {
        m *= 2.0;
    }

    return sign * m;
}


uint32_t fp32_next_up_bits(uint32_t bits) {
    uint32_t exp  = (bits >> 23) & 0xFFu;
    uint32_t frac = bits & 0x7FFFFFu;
    bool negative = (bits >> 31) != 0;

    if (exp == 0xFFu) {
        if (frac != 0) {
            return bits;                        
        }
        return negative ? 0xFF7FFFFFu : bits;   
    }

    if ((bits & 0x7FFFFFFFu) == 0) {
        return 0x00000001u;                     
    }

    return negative ? bits - 1u : bits + 1u;
}

bool fp32_truncate_to_int(uint32_t bits, int32_t *out) {
    uint32_t exp  = (bits >> 23) & 0xFFu;
    uint32_t frac = bits & 0x7FFFFFu;
    bool negative = (bits >> 31) != 0;

    if (exp == 0xFFu) {
        return false;                           
    }

    if (exp < 127u) {
        *out = 0;                               
        return true;
    }

    unsigned e = exp - 127u;                    

    if (e >= 31u) {                             
        if (e == 31u && negative && frac == 0) {
            *out = INT32_MIN;    
            return true;           
        }
        return false;
    }

    uint32_t sig = 0x800000u | frac;
    uint32_t mag = (e >= 23u) ? sig << (e - 23u)    
                              : sig >> (23u - e);   

    *out = negative ? -(int32_t)mag : (int32_t)mag;
    return true;
}