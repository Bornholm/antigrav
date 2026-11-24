# Options de la ligne de commande

| Option | Effet |
|---|---|
| `--race` | démarre directement une course, sans menu |
| `--track N`, `--ship N` | morceau de 0 à 2, vaisseau de 0 à 3 |
| `--autopilot` | l'ordinateur pilote aussi le vaisseau du joueur |
| `--sync` | mode sans fenêtre : tient le vaisseau filmé à la vitesse de la musique, pour vérifier que la piste tombe sur les temps |
| `--grid-start`, `--rolling-start` | départ arrêté ou lancé |
| `--size LxH` | taille de la fenêtre ou de l'image |
| `--scale N` | échelle de la 3D par rapport à la sortie, 0.7 par exemple |
| `--stats` | affiche les temps GPU et le nombre d'images par seconde |
| `--no-vsync` | ne pas attendre l'écran, pour mesurer |
| `--no-bloom`, `--no-reflections`, `--no-hud`, `--no-audio`, `--no-music`, `--no-click` | couper un effet |
| `--validation`, `--no-validation` | couches de validation Vulkan |
| `--headless` | rendre sans fenêtre |
| `--frames N` | s'arrêter après N images |
| `--time S` | démarrer à S secondes, mode sans fenêtre |
| `--out chemin.png` | écrire la dernière image ; avec `%04d` dans le chemin, toutes les images |
| `--wav chemin.wav` | écrire le son de la course, mode sans fenêtre |
| `--screen nom` | mode sans fenêtre : capturer un écran de menu, `main`, `solo`, `multi`, `adresse` ou `salon` |
| `--host-auto N`, `--join-auto`, `--realtime` | essai réseau sans interface, voir [Développer](developper.md) |
