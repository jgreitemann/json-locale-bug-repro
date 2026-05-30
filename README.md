# Bug: TOCTOU race between lexer construction and locale changes causes float truncation

## Description

There appears to be a time-of-check-to-time-of-use (TOCTOU) race condition in float parsing that causes silent data loss when another thread changes the locale during JSON deserialization.

The lexer queries `localeconv()->decimal_point` at construction time, replacing `.` in the token buffer with that character. Later, `strtod` parses the buffer using the *current* locale. If another thread changes the locale between these two points, the locale's current decimal point won't match what's in the buffer, causing `strtod` to stop parsing at the wrong character.

**Check:** `lexer` constructor (`lexer.hpp:127`) queries `get_decimal_point()` which calls `localeconv()`.

**Use:** `scan_number_done` (`lexer.hpp:1293`) calls `strtod()` which respects the *current* locale.

Any `setlocale()` call from another thread between construction and parsing can cause misalignment.

Additionally, changing locale in parallel with locale-dependent functions like `strtod()` is undefined behavior. Since JSON itself does not allow for decimal separators other than `.`, it is quite surprising that `json::parse()` exhibits the same safety issue and should be document if it cannot be avoided.

## Reproduction steps

1. Compile `main.cpp` (included below) with `-std=c++20`
2. Run the binary (it switches locale between `C` and `de_DE` on the main thread while a worker thread parses JSON)
3. Observe truncation or assertion failure

```bash
# Debug builds trigger an assertion
make debug && ./build/repro-debug

# Release builds silently truncate
make release && ./build/repro
```

## Expected vs actual

### Expected behavior

`99.123456` parsed correctly, with 10M+ iterations completing without error.

### Actual behavior (release)

Fractional part is silently truncated: `99.123456` becomes `99.0`.

I've also observed the reproduction to crash with `SIGSEGV` occasionally. I suspect that's another symptom of the  undefined behavior due to using `strtod` while the locale is being set.

#### Sample output (release)

```
Locale: C
TRUNCATION at iteration 68: expected 99.123456000, got 99.000000000
```
or
```
Locale: C
fish: Job 1, './build/repro' terminated by signal SIGSEGV (Address boundary error)
```

### Actual behavior (debug)

`JSON_ASSERT(endptr == token_buffer.data() + token_buffer.size())` at `lexer.hpp:1296` fires since the token buffer is not fully consumed.

#### Sample output (debug)

```
Locale: C
Assertion failed: (endptr == token_buffer.data() + token_buffer.size()), function scan_number, file lexer.hpp, line 1296.
```

## Compiler and OS

Apple clang 21.0.0 / macOS 26.5
GCC 14.1.0 (glibc 2.31.0) / Linux 7.0.0

## Library version

v3.11.3 (commit d10879bca, latest develop as of this report)
