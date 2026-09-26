#include "prx/libc/include/general/VabiMacros.hpp"
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>
extern "C" {
double APS5_VABI atof_nid_postfix(const char*);
float APS5_VABI strtof_nid_postfix(const char*, char**);
long double APS5_VABI strtold_nid_postfix(const char*, char**);
int* APS5_VABI __error_nid_postfix();
float APS5_VABI fmodf_nid_postfix(float, float);
float APS5_VABI asinf_nid_postfix(float);
float APS5_VABI acosf_nid_postfix(float);
float APS5_VABI atan2f_nid_postfix(float, float);
float APS5_VABI tanf_nid_postfix(float);
float APS5_VABI log10f_nid_postfix(float);
double APS5_VABI exp2_nid_postfix(double);
double APS5_VABI ldexp_nid_postfix(double, int);
double APS5_VABI scalbn_nid_postfix(double, int);
float APS5_VABI scalbnf_nid_postfix(float, int);
double APS5_VABI frexp_nid_postfix(double, int*);
float APS5_VABI frexpf_nid_postfix(float, int*);
std::int64_t APS5_VABI lround_nid_postfix(double);
std::int64_t APS5_VABI lroundf_nid_postfix(float);
std::int64_t APS5_VABI llround_nid_postfix(double);
int APS5_VABI __isfinitef_nid_postfix(float);
int APS5_VABI __isnormal_nid_postfix(double);
int APS5_VABI __isnormalf_nid_postfix(float);
int APS5_VABI __isinff_nid_postfix(float);
}
static void Require(bool value) { if (!value) std::abort(); }
int main() {
    Require(atof_nid_postfix(" -12.5tail") == -12.5);
    char* end = nullptr;
    const char input[] = "0x1.8p+2 remainder";
    Require(strtof_nid_postfix(input, &end) == 6.f && end == input + 8);
    const char invalid[] = "invalid";
    Require(strtof_nid_postfix(invalid, &end) == 0.f && end == invalid);
    *__error_nid_postfix() = 0;
    Require(std::isinf(strtof_nid_postfix("1e1000", nullptr)));
    Require(*__error_nid_postfix() == 34);
    Require(strtold_nid_postfix("1.0000000000000000001!", &end) > 1.L && *end == '!');
    Require(fmodf_nid_postfix(5.5f, 2.f) == 1.5f);
    Require(fmodf_nid_postfix(-5.5f, 2.f) == -1.5f);
    Require(std::signbit(fmodf_nid_postfix(-4.f, 2.f)));
    Require(std::isnan(fmodf_nid_postfix(1.f, 0.f)));
    Require(std::abs(asinf_nid_postfix(0.5f) - 0.5235988f) < 0.000001f);
    Require(std::abs(acosf_nid_postfix(0.5f) - 1.0471976f) < 0.000001f);
    Require(std::abs(atan2f_nid_postfix(1.f, -1.f) - 2.3561945f) < 0.000001f);
    Require(tanf_nid_postfix(0.f) == 0.f);
    Require(log10f_nid_postfix(100.f) == 2.f);
    Require(exp2_nid_postfix(-3.) == 0.125);
    Require(ldexp_nid_postfix(0.75, 4) == 12.);
    Require(scalbn_nid_postfix(0.75, -2) == 0.1875);
    Require(scalbnf_nid_postfix(0.75f, 4) == 12.f);
    int exponent = 0;
    Require(frexp_nid_postfix(12., &exponent) == 0.75 && exponent == 4);
    Require(frexpf_nid_postfix(-12.f, &exponent) == -0.75f && exponent == 4);
    Require(lround_nid_postfix(4294967296.5) == INT64_C(4294967297));
    Require(lround_nid_postfix(-2.5) == -3);
    Require(lroundf_nid_postfix(4294967296.f) == INT64_C(4294967296));
    Require(lroundf_nid_postfix(2.5f) == 3);
    Require(llround_nid_postfix(-4294967296.5) == -INT64_C(4294967297));
    const auto infinity = std::numeric_limits<float>::infinity();
    const auto nan = std::numeric_limits<float>::quiet_NaN();
    Require(__isinff_nid_postfix(infinity) == 1 && __isinff_nid_postfix(-infinity) == 1);
    Require(__isinff_nid_postfix(nan) == 0 && __isinff_nid_postfix(1.f) == 0);
    Require(__isfinitef_nid_postfix(0.f) == 1 && __isfinitef_nid_postfix(infinity) == 0);
    Require(__isfinitef_nid_postfix(nan) == 0);
    Require(__isnormalf_nid_postfix(1.f) == 1 && __isnormalf_nid_postfix(0.f) == 0);
    Require(__isnormalf_nid_postfix(std::numeric_limits<float>::denorm_min()) == 0);
    Require(__isnormal_nid_postfix(1.) == 1 && __isnormal_nid_postfix(0.) == 0);
    Require(__isnormal_nid_postfix(std::numeric_limits<double>::denorm_min()) == 0);
}
