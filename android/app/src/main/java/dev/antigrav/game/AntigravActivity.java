package dev.antigrav.game;

import org.libsdl.app.SDLActivity;

/** Activité unique du jeu. Tout le reste est en C++, chargé par SDL. */
public class AntigravActivity extends SDLActivity {
    @Override
    protected String[] getLibraries() {
        return new String[] {"SDL3", "main"};
    }
}
