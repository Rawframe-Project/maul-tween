# Maul Tween API reference

Generated from the public headers by `tools/gen_api.py`. The headers
are the source of truth; this file mirrors them.

## `base.h`

The base of the Maul Tween API: the library version, the export and attribute macros, and the result codes every fallible function returns.

```c
mtwnVersion mtwnGetVersion(void);
```
Returns the version of the library that was linked, which may differ from the MTWN_VERSION macros a program was compiled with.  @return The library version. @par Thread safety Safe from any thread.

```c
const char* mtwnResultName(mtwnResult result);
```
Returns the name of a result code, for diagnostics.  @param result  Any value; an unknown one is named as such. @return A static, NUL-terminated string such as "mtwn_errorCapacity". @par Thread safety Safe from any thread.

---

2 functions across 1 headers.
