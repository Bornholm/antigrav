#pragma once

// Journalisation. Passe par SDL, donc par logcat sur Android.
// MinGW vérifie par défaut les formats du printf de Windows, qui ne connaît
// pas %zu. Le jeu y est compilé avec le printf de MinGW.
#ifdef __MINGW32__
#define SOUNDWAVE_PRINTF gnu_printf
#else
#define SOUNDWAVE_PRINTF printf
#endif

void logInfo(const char* fmt, ...) __attribute__((format(SOUNDWAVE_PRINTF, 1, 2)));
void logWarn(const char* fmt, ...) __attribute__((format(SOUNDWAVE_PRINTF, 1, 2)));
[[noreturn]] void fatal(const char* fmt, ...) __attribute__((format(SOUNDWAVE_PRINTF, 1, 2)));
