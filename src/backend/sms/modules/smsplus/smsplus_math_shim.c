#include "math.h"
#include <stdint.h>

static const double kPi = 3.14159265358979323846;
static const double kTwoPi = 6.28318530717958647692;
static const double kHalfPi = 1.57079632679489661923;
static const double kLn2 = 0.69314718055994530942;
static const double kInvLn10 = 0.43429448190325182765;

typedef union
{
    double d;
    uint64_t u;
} smsplus_double_bits_t;

double fabs(double x)
{
    return x < 0.0 ? -x : x;
}

double floor(double x)
{
    long long i = (long long) x;
    if (((double) i > x) && x < 0.0)
    {
        --i;
    }
    return (double) i;
}

double ceil(double x)
{
    long long i = (long long) x;
    if (((double) i < x) && x > 0.0)
    {
        ++i;
    }
    return (double) i;
}

double sqrt(double x)
{
    if (x <= 0.0)
    {
        return 0.0;
    }

    double guess = x > 1.0 ? x : 1.0;
    for (int i = 0; i < 12; ++i)
    {
        guess = 0.5 * (guess + x / guess);
    }
    return guess;
}

static double smsplus_wrap_angle(double x)
{
    if ((x > kTwoPi) || (x < -kTwoPi))
    {
        x -= floor(x / kTwoPi) * kTwoPi;
    }
    if (x > kPi)
    {
        x -= kTwoPi;
    }
    else if (x < -kPi)
    {
        x += kTwoPi;
    }
    return x;
}

double sin(double x)
{
    x = smsplus_wrap_angle(x);
    if (x > kHalfPi)
    {
        x = kPi - x;
    }
    else if (x < -kHalfPi)
    {
        x = -kPi - x;
    }

    const double x2 = x * x;
    const double x3 = x * x2;
    const double x5 = x3 * x2;
    const double x7 = x5 * x2;
    const double x9 = x7 * x2;
    return x
         - (x3 / 6.0)
         + (x5 / 120.0)
         - (x7 / 5040.0)
         + (x9 / 362880.0);
}

static double smsplus_normalize_subnormal(double x, int *exp2)
{
    while (x > 0.0 && x < 1.0)
    {
        x *= 2.0;
        --(*exp2);
    }
    return x;
}

double log(double x)
{
    if (x <= 0.0)
    {
        return -1.0e300;
    }

    smsplus_double_bits_t bits;
    bits.d = x;

    int exp2 = (int) ((bits.u >> 52) & 0x7FFu);
    if (exp2 == 0)
    {
        x = smsplus_normalize_subnormal(x, &exp2);
        bits.d = x;
        exp2 += (int) ((bits.u >> 52) & 0x7FFu);
    }

    exp2 -= 1023;
    bits.u = (bits.u & ((1ULL << 52) - 1ULL)) | (1023ULL << 52);
    const double m = bits.d;
    const double y = (m - 1.0) / (m + 1.0);
    const double y2 = y * y;
    const double y3 = y * y2;
    const double y5 = y3 * y2;
    const double y7 = y5 * y2;
    const double y9 = y7 * y2;
    const double y11 = y9 * y2;
    const double ln_m = 2.0 * (y + y3 / 3.0 + y5 / 5.0 + y7 / 7.0 + y9 / 9.0 + y11 / 11.0);
    return (double) exp2 * kLn2 + ln_m;
}

double log10(double x)
{
    return log(x) * kInvLn10;
}

static double smsplus_exp_series(double x)
{
    const double x2 = x * x;
    const double x3 = x2 * x;
    const double x4 = x3 * x;
    const double x5 = x4 * x;
    const double x6 = x5 * x;
    const double x7 = x6 * x;
    return 1.0
         + x
         + x2 / 2.0
         + x3 / 6.0
         + x4 / 24.0
         + x5 / 120.0
         + x6 / 720.0
         + x7 / 5040.0;
}

static double smsplus_scale_pow2(double x, int exp2)
{
    while (exp2 > 0)
    {
        x *= 2.0;
        --exp2;
    }
    while (exp2 < 0)
    {
        x *= 0.5;
        ++exp2;
    }
    return x;
}

static double smsplus_exp(double x)
{
    if (x > 709.0)
    {
        return 1.0e300;
    }
    if (x < -745.0)
    {
        return 0.0;
    }

    const int exp2 = (int) floor(x / kLn2);
    const double r = x - ((double) exp2 * kLn2);
    return smsplus_scale_pow2(smsplus_exp_series(r), exp2);
}

double pow(double base, double exponent)
{
    if (base == 0.0)
    {
        return exponent > 0.0 ? 0.0 : 1.0e300;
    }
    if (base < 0.0)
    {
        const double whole = floor(exponent);
        if (whole != exponent)
        {
            return 0.0;
        }
        const double mag = smsplus_exp(exponent * log(-base));
        return (((long long) whole) & 1LL) ? -mag : mag;
    }

    return smsplus_exp(exponent * log(base));
}
