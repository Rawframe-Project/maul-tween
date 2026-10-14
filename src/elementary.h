// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The library's own exponential and logarithm (record mtwn-0008), so
// that they give the same bits on every platform: only arithmetic and
// the functions IEEE 754 rounds exactly. Each states the accuracy its
// tests hold it to.

#ifndef MAUL_TWEEN_SRC_ELEMENTARY_H
#define MAUL_TWEEN_SRC_ELEMENTARY_H

// e to the x, within 1 ulp for results in the normal range. Overflows
// to infinity above ln(DBL_MAX), is 0 below ln(2^-1075), and NaN for
// NaN; results in the subnormal range are rounded twice.
double mtwnExp(double x);

// 2 to the x, within 1 ulp for results in the normal range, and exact
// for every integer x whose power is a double. Overflows to infinity
// from 1024, is 0 below -1075, and NaN for NaN.
double mtwnExp2(double x);

// The natural logarithm of x, within 1 ulp: -infinity for 0, NaN below 0
// and for NaN, infinity for infinity, exactly 0 for 1.
double mtwnLog(double x);

#endif // MAUL_TWEEN_SRC_ELEMENTARY_H
