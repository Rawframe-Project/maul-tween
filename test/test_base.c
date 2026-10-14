// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Versions and result names.

#include "test_harness.h"

#include "maul-tween/base.h"

#include <string.h>

static void TestVersionMatchesHeader(void)
{
    mtwnVersion version = mtwnGetVersion();
    CHECK(version.major == MTWN_VERSION_MAJOR, "major version");
    CHECK(version.minor == MTWN_VERSION_MINOR, "minor version");
    CHECK(version.patch == MTWN_VERSION_PATCH, "patch version");
}

static void TestResultNames(void)
{
    static const struct
    {
        mtwnResult result;
        const char* name;
    } cases[] = {
        {mtwn_success, "mtwn_success"},
        {mtwn_errorInvalid, "mtwn_errorInvalid"},
        {mtwn_errorCapacity, "mtwn_errorCapacity"},
        {mtwn_errorLimit, "mtwn_errorLimit"},
        {mtwn_errorVersion, "mtwn_errorVersion"},
        {mtwn_errorStale, "mtwn_errorStale"},
        {mtwn_errorBusy, "mtwn_errorBusy"},
        {mtwn_errorState, "mtwn_errorState"},
    };
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; ++i)
    {
        CHECK(strcmp(mtwnResultName(cases[i].result), cases[i].name) == 0, cases[i].name);
    }
    CHECK(strcmp(mtwnResultName(12345), "unknown result") == 0, "unknown name");
    CHECK(strcmp(mtwnResultName(-8), "unknown result") == 0, "one past the last error");
}

int main(void)
{
    TestVersionMatchesHeader();
    TestResultNames();
    return s_failures == 0 ? 0 : 1;
}
