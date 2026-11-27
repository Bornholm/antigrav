# Options de la ligne de commande

| Option | Effet |
|---|---|
| `--race` | démarre directement une course, sans menu |
| `--track N`, `--ship N` | morceau de 0 à 8, trois par coupe, vaisseau de 0 à 3. Sans `--ship`, le jeu reprend le dernier vaisseau choisi |
| `--check-update` | interroge GitHub sur la dernière version publiée, l'affiche et quitte. Windows et Linux seulement |
| `--pause` | avec `--headless`, ouvre le menu de course, qui met la course en pause |
| `--ghost` | avec `--headless`, ajoute un fantôme : la même course pilotée par l'ordinateur sur le vaisseau suivant |
| `--tip N` | avec `--headless`, affiche le conseil d'initiation N, de 0 (départ turbo) à 11 (tremplin) |
| `--autopilot` | l'ordinateur pilote aussi le vaisseau du joueur |
| `--lang fr`, `--lang en`, `--lang es` | langue de l'interface pour ce lancement |
| `--touch`, `--swap` | affiche les boutons tactiles comme sur un téléphone ; inverse leur disposition |
| `--song fichier` | court directement sur ce fichier audio |
| `--import fichier` | copie ce fichier dans la bibliothèque du joueur et le sélectionne |
| `--sync` | mode sans fenêtre : tient le vaisseau filmé à la vitesse de la musique, pour vérifier que la piste tombe sur les temps |
| `--grid-start`, `--rolling-start` | départ arrêté ou lancé |
| `--size LxH` | taille de la fenêtre ou de l'image |
| `--scale N` | échelle de la 3D par rapport à la sortie, 0.7 par exemple |
| `--difficulty N` | niveau des IA d'une course lancée par `--race` : 0 facile, 1 normale, 2 difficile |
| `--quality N` | qualité du rendu pour ce lancement : 0 basse, 1 moyenne, 2 haute. Sans cette option, le jeu suit le réglage QUALITÉ, automatique par défaut |
| `--cap60` | plafonne l'affichage à 60 images par seconde. Sans cette option, le jeu suit le réglage CADENCE, automatique par défaut |
| `--no-adaptive` | garde la 3D à une définition fixe |
| `--stats` | affiche les temps GPU et le nombre d'images par seconde |
| `--no-vsync` | ne pas attendre l'écran, pour mesurer |
| `--no-bloom`, `--no-reflections`, `--no-hud`, `--no-audio`, `--no-music` | couper un effet |
| `--validation`, `--no-validation` | couches de validation Vulkan |
| `--headless` | rendre sans fenêtre |
| `--frames N` | s'arrêter après N images |
| `--time S` | démarrer à S secondes, mode sans fenêtre |
| `--out chemin.png` | écrire la dernière image ; avec `%04d` dans le chemin, toutes les images |
| `--wav chemin.wav` | écrire le son de la course, mode sans fenêtre |
| `--screen nom` | mode sans fenêtre : capturer un écran de menu, `main`, `options`, `solo`, `morceaux`, `coupes`, `tableau`, `podium`, `multi`, `adresse`, `pseudo` ou `salon` |
| `--auto-cup N`, `--auto-play` | essai réseau d'une coupe entière : l'hôte court la coupe N, chaque instance enchaîne seule les écrans de fin |
| `--host-auto N`, `--join-auto`, `--realtime` | essai réseau sans interface, voir [Développer](developper.md) |
