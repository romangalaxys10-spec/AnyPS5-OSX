#include <cmath>
#include <cstdint>

#include "prx/libc/include/General.hpp"

extern "C" {

float APS5_VABI fmodf_nid_postfix(float x, float y) { return std::fmod(x, y); }
float APS5_VABI asinf_nid_postfix(float x) { return std::asin(x); }
float APS5_VABI acosf_nid_postfix(float x) { return std::acos(x); }
float APS5_VABI atan2f_nid_postfix(float y, float x) { return std::atan2(y, x); }
float APS5_VABI tanf_nid_postfix(float x) { return std::tan(x); }
float APS5_VABI log10f_nid_postfix(float x) { return std::log10(x); }
double APS5_VABI exp2_nid_postfix(double x) { return std::exp2(x); }
double APS5_VABI ldexp_nid_postfix(double x, int exponent) { return std::ldexp(x, exponent); }
double APS5_VABI scalbn_nid_postfix(double x, int exponent) { return std::scalbn(x, exponent); }
float APS5_VABI scalbnf_nid_postfix(float x, int exponent) { return std::scalbn(x, exponent); }
double APS5_VABI frexp_nid_postfix(double x, int* exponent) { return std::frexp(x, exponent); }
float APS5_VABI frexpf_nid_postfix(float x, int* exponent) { return std::frexp(x, exponent); }
// Guest long is 64-bit, including on Windows where native long is 32-bit.
std::int64_t APS5_VABI lround_nid_postfix(double x) { return std::llround(x); }
std::int64_t APS5_VABI lroundf_nid_postfix(float x) { return std::llround(x); }
std::int64_t APS5_VABI llround_nid_postfix(double x) { return std::llround(x); }
int APS5_VABI __isfinitef_nid_postfix(float x) { return std::isfinite(x) ? 1 : 0; }
int APS5_VABI __isnormal_nid_postfix(double x) { return std::isnormal(x) ? 1 : 0; }
int APS5_VABI __isnormalf_nid_postfix(float x) { return std::isnormal(x) ? 1 : 0; }
int APS5_VABI __isinff_nid_postfix(float x) { return std::isinf(x) ? 1 : 0; }

double APS5_VABI cbrt_nid_postfix(double x) { return std::cbrt(x); }
double APS5_VABI asin_nid_postfix(double x) { return std::asin(x); }
double APS5_VABI acos_nid_postfix(double x) { return std::acos(x); }
double APS5_VABI exp_nid_postfix(double x) { return std::exp(x); }
double APS5_VABI atan_nid_postfix(double x) { return std::atan(x); }
double APS5_VABI tan_nid_postfix(double x) { return std::tan(x); }
double APS5_VABI log2_nid_postfix(double x) { return std::log2(x); }
double APS5_VABI log_nid_postfix(double x) { return std::log(x); }

float APS5_VABI sinf_nid_postfix(float x) { return std::sin(x); }
float APS5_VABI cosf_nid_postfix(float x) { return std::cos(x); }

void APS5_VABI sincosf_nid_postfix(float x, float* sinp, float* cosp) {
    *sinp = std::sin(x);
    *cosp = std::cos(x);
}

double APS5_VABI sin_nid_postfix(double x) { return std::sin(x); }
double APS5_VABI cos_nid_postfix(double x) { return std::cos(x); }

void APS5_VABI sincos_nid_postfix(double x, double* sinp, double* cosp) {
    *sinp = std::sin(x);
    *cosp = std::cos(x);
}

float APS5_VABI atanf_nid_postfix(float x) { return std::atan(x); }
double APS5_VABI atan2_nid_postfix(double y, double x) { return std::atan2(y, x); }
float APS5_VABI powf_nid_postfix(float base, float exp) { return std::pow(base, exp); }
double APS5_VABI pow_nid_postfix(double base, double exp) { return std::pow(base, exp); }
float APS5_VABI expf_nid_postfix(float x) { return std::exp(x); }
float APS5_VABI exp2f_nid_postfix(float x) { return std::exp2(x); }
float APS5_VABI logf_nid_postfix(float x) { return std::log(x); }
float APS5_VABI log2f_nid_postfix(float x) { return std::log2(x); }
double APS5_VABI log10_nid_postfix(double x) { return std::log10(x); }
float APS5_VABI ldexpf_nid_postfix(float x, int exp) { return std::ldexp(x, exp); }
double APS5_VABI fmod_nid_postfix(double x, double y) { return std::fmod(x, y); }
float APS5_VABI roundf_nid_postfix(float x) { return std::round(x); }
double APS5_VABI round_nid_postfix(double x) { return std::round(x); }

int APS5_VABI __isfinite_nid_postfix(double x) { return std::isfinite(x) ? 1 : 0; }
int APS5_VABI __isnan_nid_postfix(double x) { return std::isnan(x) ? 1 : 0; }
int APS5_VABI __signbit_nid_postfix(double x) { return std::signbit(x) ? 1 : 0; }

}
