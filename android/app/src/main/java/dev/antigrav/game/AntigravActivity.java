package dev.antigrav.game;

import android.content.Context;
import android.database.Cursor;
import android.net.Uri;
import android.provider.OpenableColumns;
import android.net.wifi.WifiManager;

import org.libsdl.app.SDLActivity;

/** Activité unique du jeu. Tout le reste est en C++, chargé par SDL. */
public class AntigravActivity extends SDLActivity {
    private WifiManager.MulticastLock multicastLock;

    @Override
    protected String[] getLibraries() {
        return new String[] {"SDL3", "main"};
    }

    /**
     * Appelé depuis le C++ quand le joueur héberge une partie. Sans ce verrou,
     * Android jette les paquets diffusés et l'hôte reste invisible.
     */
    public void acquireMulticastLock() {
        if (multicastLock != null) {
            return;
        }
        WifiManager wifi = (WifiManager) getApplicationContext().getSystemService(Context.WIFI_SERVICE);
        if (wifi == null) {
            return;
        }
        multicastLock = wifi.createMulticastLock("antigrav");
        multicastLock.setReferenceCounted(false);
        multicastLock.acquire();
    }

    /**
     * Appelé depuis le C++ : nom de fichier d'un document choisi dans le
     * sélecteur du système. Son adresse content:// ne le contient pas.
     * Renvoie une chaîne vide si le système ne le donne pas.
     */
    public String displayName(String uri) {
        try (Cursor cursor = getContentResolver().query(
                Uri.parse(uri), new String[] {OpenableColumns.DISPLAY_NAME}, null, null, null)) {
            if (cursor != null && cursor.moveToFirst()) {
                String name = cursor.getString(0);
                return name != null ? name : "";
            }
        } catch (Exception e) {
            // Fournisseur de documents capricieux : on fera sans le nom.
        }
        return "";
    }

    @Override
    protected void onDestroy() {
        if (multicastLock != null && multicastLock.isHeld()) {
            multicastLock.release();
        }
        super.onDestroy();
    }
}
