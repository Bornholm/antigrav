#include "core/log.h"

#include <SDL3/SDL.h>
#include <cstdarg>
#include <cstdlib>

void logInfo(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    SDL_LogMessageV(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO, fmt, ap);
    va_end(ap);
}

void logWarn(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    SDL_LogMessageV(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_WARN, fmt, ap);
    va_end(ap);
}

void fatal(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    SDL_LogMessageV(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_CRITICAL, fmt, ap);
    va_end(ap);
    std::abort();
}
