#ifndef SMSBARE_FREESTANDING_MATH_H
#define SMSBARE_FREESTANDING_MATH_H

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifdef __cplusplus
extern "C" {
#endif

double fabs(double x);
double floor(double x);
double ceil(double x);
double sqrt(double x);
double sin(double x);
double log(double x);
double log10(double x);
double pow(double base, double exponent);

#ifdef __cplusplus
}
#endif

#endif
