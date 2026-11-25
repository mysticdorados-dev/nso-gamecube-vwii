# NSO GameCube USB Probe – Wii U / vWii

**État : prototype de diagnostic en code source, non compilé et non testé sur console. Ce n'est PAS encore un correctif Nintendont.**

L'objectif est de faire fonctionner la manette GameCube Nintendo Switch Online pour Switch 2 (`057E:2073`) en USB sur Wii U/vWii, puis dans Nintendont.

## Ce que tes tests ont établi

- HIDTest détecte `VID 057E`, `PID 2073`, endpoint interrupt `0x81`, paquet 64 octets.
- Les pressions de boutons ne produisent aucune donnée visible dans ce vieux HIDTest.
- Le pilote SDL3 initialise la manette par **interface USB 1 de type vendor 0xFF** ; endpoints **bulk OUT 0x02 / IN 0x82**. Ensuite le rapport d'entrée de l'interface 0 a l'ID **0x0A**.
- Le pilote USB existant de Nintendont connaît déjà `/dev/usb/ven` et les transferts bulk (`kernel/usb.c`) ; son pilote HID n'implémente pas cette initialisation (`kernel/HID.c`).

## Contenu du prototype

- `source/nsogc_protocol.[ch]` : séquence d'initialisation volatile d'après SDL3 + décodeur de rapport 0x0A (boutons, axes 12 bits, gâchettes).
- `source/main.c` : homebrew Wii/vWii expérimentale, qui recherche les deux interfaces de la manette, ouvre la vendor, envoie la séquence et lit les rapports HID.
- `tests/test_protocol.c` : tests unitaires du décodage et des octets de commande.
- `Makefile` : compilation DOL sous devkitPro/libogc.

## Compilation (sur un PC avec devkitPro Wii / devkitPPC)

Depuis le dossier contenant `Makefile` :

```sh
export DEVKITPRO=/opt/devkitpro
export DEVKITPPC=$DEVKITPRO/devkitPPC
make
```

Si tu disposes de Docker et de l'image devkitPro, l'équivalent est :

```sh
docker run --rm -v "$PWD":/project -w /project devkitpro/devkitppc:latest make
```

**Ces commandes de compilation ne sont pas vérifiées ici avec la chaîne Wii** : le système de construction utilisé pour ce livrable ne dispose pas de devkitPPC/libogc. Il peut être nécessaire d'ajuster la configuration aux versions installées.

Pour lancer le test (seulement après compilation réussie), copier :

```
SD:/apps/nsogc_probe/boot.dol   <- nsogc_probe.dol
SD:/apps/nsogc_probe/meta.xml
```

Lancer depuis **Homebrew Channel vWii**, manette branchée en USB-C. Une Wiimote est utile pour sortir avec HOME. **Ne pas écraser le Nintendont ni ton forwarder existants.**

Le probe affiche les erreurs/détections éventuelles ; photographier l'écran.

## Limitations connues

- La coexistence des deux interfaces via libogc/IOS58 **reste à confirmer sur une vraie Wii U**.
- Le prototype omet les lectures de calibration flash du pilote SDL3. Les axes sont bruts, pas corrigés par la calibration du constructeur. Pour un vrai portage Nintendont, il faudra gérer ces données.
- L'initialisation déclenche uniquement des commandes volatiles (pas de mise à jour ni d'écriture flash), mais une commande inconnue est utilisée dans la séquence publique SDL3. Aucun test matériel n'a encore été réalisé.
- L'API USB est asynchrone ; si le contrôleur ne répond pas, l'outil arrête les nouvelles commandes. Un redémarrage de la vWii peut être nécessaire.
- Un vrai correctif Nintendont nécessitera ensuite la gestion de l'interface vendor dans son noyau, la lecture HID, la correspondance des boutons GameCube, puis tests de gameplay et de sauvegardes.

## Tests du décodeur sans Wii

```sh
gcc -std=c11 -Wall -Wextra -Werror -pedantic -Isource tests/test_protocol.c source/nsogc_protocol.c -o test_protocol
./test_protocol
```

## Sources techniques

- Nintendont, USB kernel : https://github.com/FIX94/Nintendont/blob/master/kernel/usb.c
- Nintendont, HID kernel : https://github.com/FIX94/Nintendont/blob/master/kernel/HID.c
- SDL3 Switch 2 GameCube : https://github.com/libsdl-org/SDL/blob/main/src/joystick/hidapi/SDL_hidapi_switch2.c
- Descripteurs USB : https://github.com/ndeadly/switch2_controller_research/blob/master/descriptors.md
- libogc USB public : https://github.com/devkitPro/libogc/blob/master/libogc/usb.c
