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
    char message[512];
    va_list ap;
    va_start(ap, fmt);
    SDL_vsnprintf(message, sizeof(message), fmt, ap);
    va_end(ap);
    SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "%s", message);
#ifdef __ANDROID__
    // Sur un téléphone personne ne lit le journal : sans cette fenêtre, le jeu
    // se ferme sans explication.
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Antigrav ne peut pas démarrer", message,
                             nullptr);
#endif
    std::abort();
}
