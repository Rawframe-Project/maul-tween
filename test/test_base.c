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
        mtwnResult value;
        const char* name;
    } cases[] = {
        {mtwn_success, 0, "mtwn_success"},
        {mtwn_errorInvalid, -1, "mtwn_errorInvalid"},
        {mtwn_errorCapacity, -2, "mtwn_errorCapacity"},
        {mtwn_errorStale, -3, "mtwn_errorStale"},
        {mtwn_errorState, -6, "mtwn_errorState"},
        {mtwn_errorVersion, -7, "mtwn_errorVersion"},
        {mtwn_errorLimit, -9, "mtwn_errorLimit"},
        {mtwn_errorBusy, -10, "mtwn_errorBusy"},
    };
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; ++i)
    {
        CHECK(cases[i].result == cases[i].value &&
                  strcmp(mtwnGetResultName(cases[i].result), cases[i].name) == 0,
              cases[i].name);
    }
    // The family's shared errors maul-tween never returns, and past them.
    static const mtwnResult unknown[] = {-4, -5, -8, -11, -32, 12345};
    bool none = true;
    for (size_t i = 0; i < sizeof unknown / sizeof unknown[0]; ++i)
    {
        none = none && strcmp(mtwnGetResultName(unknown[i]), "unknown result") == 0;
    }
    CHECK(none, "the family's values it does not define, and others, unknown");
}

int main(void)
{
    TestVersionMatchesHeader();
    TestResultNames();
    return s_failures == 0 ? 0 : 1;
}
