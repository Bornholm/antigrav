#pragma once

// Journalisation. Passe par SDL, donc par logcat sur Android.
void logInfo(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void logWarn(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
[[noreturn]] void fatal(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
