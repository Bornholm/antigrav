# Publier

## Intégration continue

`.github/workflows/build.yml` compile le jeu pour Linux, lance les tests, puis compile l'APK de debug. Il tourne à chaque push sur `main`, sur les pull requests, sur les tags `v*` et à la demande. Les deux résultats sont à télécharger dans les artefacts de l'exécution : `antigrav-linux-x86_64` et `antigrav-android`.

Pousser un tag `v*`, par exemple `v0.2.0`, crée en plus une release GitHub avec l'archive Linux, l'APK en variante release et leurs sommes SHA-256. Le numéro de version de l'APK vient du tag.

## Clé de signature

Sans réglage, chaque exécution signe l'APK avec une clé de debug différente, et Android refuse alors d'installer un APK par-dessus le précédent. Pour une clé fixe, créer un keystore une fois :

```sh
keytool -genkeypair -keystore antigrav.keystore -storetype PKCS12 \
  -alias antigrav -keyalg RSA -keysize 4096 -validity 10000
```

Puis déclarer quatre secrets dans le dépôt GitHub :

| Secret | Contenu |
|---|---|
| `ANDROID_KEYSTORE_BASE64` | le keystore encodé, sortie de `base64 -w0 antigrav.keystore` |
| `ANDROID_KEYSTORE_PASSWORD` | mot de passe du keystore |
| `ANDROID_KEY_ALIAS` | alias de la clé, `antigrav` dans l'exemple |
| `ANDROID_KEY_PASSWORD` | mot de passe de la clé, facultatif s'il est le même que celui du keystore |

Garder une copie du keystore hors du dépôt : une clé perdue ne se régénère pas. La même signature marche en local avec les variables `ANTIGRAV_KEYSTORE`, qui est le chemin du fichier, `ANTIGRAV_KEYSTORE_PASSWORD`, `ANTIGRAV_KEY_ALIAS` et `ANTIGRAV_KEY_PASSWORD`.
