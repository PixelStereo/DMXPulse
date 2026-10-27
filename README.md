# DMXPulse — Qt 6 / C++17

Application native pour macOS, Windows et Linux. Interface en français.

## Utilisation

1. Choisir l’interface réseau (nom et IPv4, bouton Actualiser pour rechercher les adaptateurs branchés à chaud) et démarrer l’écoute. Autoriser le réseau local / UDP dans le pare-feu si nécessaire.
2. Choisir Art-Net ou sACN et le numéro d’univers. Les numéros Art-Net sont **zéro-based (0–32767)** ; sACN utilise **1–63999**.
3. Cliquer sur une source dans la liste pour afficher ses 512 canaux. Les données sans actualisation depuis 2,5 secondes sont grisées ; les sources sont conservées 60 secondes.
4. Cliquer sur un canal puis ajuster sa valeur entre 0 et 255 au curseur ou au champ numérique. Chaque case affiche `canal:valeur reçue`. Le champ sous la grille affiche la valeur de sortie du canal sélectionné ; l’infobulle de chaque case détaille réception et sortie.
5. Pour reprendre un état reçu, cliquer **Copier réception → sortie**. La sortie démarre à zéro et constitue un univers entier de 512 canaux ; elle n’est pas fusionnée automatiquement avec la réception.
6. Art-Net : saisir une IPv4 de destination unicast ou broadcast adaptée au réseau. sACN : destination multicast automatique `239.255.HI.LO`.
7. Activer l’émission : la sortie est envoyée à 30 Hz environ. **Sortie à zéro** met tous les canaux de sortie à 0 ; si l’émission est active, ce changement est immédiatement émis au prochain cycle. Arrêter l’émission laisse le comportement final aux récepteurs ; sACN envoie trois paquets de terminaison.

L’écoute sACN rejoint le groupe multicast du seul univers sélectionné et reçoit aussi les paquets unicast arrivant au port 5568. Art-Net écoute les ArtDmx reçus au port 6454. La liste représente les flux observés, pas un inventaire de tous les équipements du réseau. L’application conserve chaque source distinctement et indique sa priorité sACN. Pas de fusion HTP/LTP, ArtPoll, RDM, synchronisation sACN, découverte d’univers sACN ou prise en charge IPv6. Les données sACN synchronisées et les paquets preview sont ignorés. Les paquets ayant un start code différent de 0 sont ignorés. Les paquets obsolètes / dupliqués sont filtrés par numéro de séquence.

## Compilation

Prérequis : CMake 3.21+, Qt 6.4+ (Widgets, Network et Test), compilateur C++17.
Qt doit être compilé pour la plateforme et le compilateur choisis.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/chemin/vers/Qt/6.x/plateforme
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

- macOS : `build/DMXPulse.app`. Distribution : `macdeployqt build/DMXPulse.app`.
- Windows : utiliser un terminal de développement MSVC et Qt MSVC. Exécutable : `build/Release/DMXPulse.exe` avec un générateur multi-configuration. Exécuter `windeployqt build/Release/DMXPulse.exe` pour les dépendances.
- Linux : exécutable `build/DMXPulse`. Installer les bibliothèques Qt correspondantes via le gestionnaire de paquets, ou préparer un paquet adapté à la distribution.

## Validation

Tests QtTest : encodage/décodage des deux protocoles, 512 canaux, limites des univers, paquets tronqués, champs invalides ; test de l’interface avec réception Art-Net et sACN UDP locale, édition d’un canal et retour de la valeur émise. Le test réseau demande la possibilité de lier les ports UDP 6454/5568. Validation matérielle sur une installation lumière et compilation Windows/Linux restent à effectuer.

## Références

- Art-Net™ Designed by and Copyright Artistic Licence.
- Documentation Art-Net : https://artisticlicence.com/pages/data-and-dmx-support-documents
- ANSI E1.31-2018 : https://tsp.esta.org/tsp/documents/docs/E1-31-2018.pdf

Les sources sont prévues pour macOS, Windows et Linux. La compilation et les tests ont été exécutés sur macOS Apple Silicon avec Qt 6.12. Les paquets de distribution restent à préparer.
