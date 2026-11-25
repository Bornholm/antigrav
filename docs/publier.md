# Publier

## Intégration continue

`.github/workflows/build.yml` compile le jeu pour Linux, lance les tests, puis compile l'APK de debug. Il tourne à chaque push sur `main`, sur les pull requests, sur les tags `v*` et à la demande. Les deux résultats sont à télécharger dans les artefacts de l'exécution : `soundwave-linux-x86_64` et `soundwave-android`.

Pousser un tag `v*`, par exemple `v0.2.0`, crée en plus une release GitHub avec l'archive Linux, l'APK en variante release et leurs sommes SHA-256. Le numéro de version de l'APK vient du tag. Le menu principal affiche cette version en bas à droite. Hors d'un tag, il affiche la sortie de `git describe`, par exemple `v0.5.0-3-gabc1234`.

## Clé de signature

Sans réglage, chaque exécution signe l'APK avec une clé de debug différente, et Android refuse alors d'installer un APK par-dessus le précédent. Pour une clé fixe, créer un keystore une fois :

```sh
keytool -genkeypair -keystore soundwave.keystore -storetype PKCS12 \
  -alias soundwave -keyalg RSA -keysize 4096 -validity 10000
```

Puis déclarer quatre secrets dans le dépôt GitHub :

| Secret | Contenu |
|---|---|
| `ANDROID_KEYSTORE_BASE64` | le keystore encodé, sortie de `base64 -w0 soundwave.keystore` |
| `ANDROID_KEYSTORE_PASSWORD` | mot de passe du keystore |
| `ANDROID_KEY_ALIAS` | alias de la clé, `soundwave` dans l'exemple |
| `ANDROID_KEY_PASSWORD` | mot de passe de la clé, facultatif s'il est le même que celui du keystore |

Garder une copie du keystore hors du dépôt : une clé perdue ne se régénère pas. La même signature marche en local avec les variables `SOUNDWAVE_KEYSTORE`, qui est le chemin du fichier, `SOUNDWAVE_KEYSTORE_PASSWORD`, `SOUNDWAVE_KEY_ALIAS` et `SOUNDWAVE_KEY_PASSWORD`.

## Play Store

Sur un tag `v*`, la CI produit aussi un bundle `soundwave.aab`, à télécharger dans l'artefact `soundwave-android-bundle` de l'exécution. C'est ce fichier qu'on envoie dans la Play Console. En local :

```sh
cd android
gradle bundleRelease
```

Le bundle sort dans `app/build/outputs/bundle/release/app-release.aab`.

La Play Console refuse un bundle dont le numéro de version (`versionCode`) a déjà servi. La CI prend le numéro de l'exécution, qui ne fait que monter. En local, le fixer avec `SOUNDWAVE_VERSION_CODE`.

### Signature

Avec la signature d'application de Google Play, Google signe lui-même ce que les joueurs installent. Le keystore décrit plus haut devient la clé d'import : il prouve seulement que le bundle vient de nous. Conséquence : un APK pris dans une release GitHub et le jeu installé depuis le Play Store n'ont pas la même signature, et Android refuse de remplacer l'un par l'autre. Il faut désinstaller pour passer de l'un à l'autre.

### Fiche

Les textes et les images de la fiche sont dans `fastlane/metadata/android`, un dossier par langue :

| Fichier | Contenu |
|---|---|
| `title.txt` | nom, 30 caractères au plus |
| `short_description.txt` | accroche, 80 caractères au plus |
| `full_description.txt` | description, 4 000 caractères au plus |
| `images/icon.png` | icône de 512 × 512 |
| `images/featureGraphic.png` | bannière de 1 024 × 500 |
| `images/phoneScreenshots/` | captures de 1 920 × 1 080 |

Les captures viennent du jeu lancé sans fenêtre, par exemple :

```sh
./build/soundwave --headless --lang en --race --autopilot --track 1 --time 52 \
  --frames 4 --size 1920x1080 --no-audio --out 1.png
```

La politique de confidentialité que demande la Play Console est publiée sur https://lookingfora.name/soundwave/confidentialite/. Cette adresse est liée depuis la fiche, elle ne doit pas changer. Le texte de référence est [confidentialite.md](confidentialite.md) : après une modification, reporter le changement sur la page du blog.
