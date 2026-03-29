# Demo FTP Q1-Q17

Ce dépôt contient une démonstration complète du projet FTP dans le script :

```bash
scripts/demo_q1_q17.sh
```

La cible Make associée est :

```bash
make demo-q1-q17
```

Cette cible lance simplement :

```bash
bash scripts/demo_q1_q17.sh
```

Le script :

- recompile le projet ;
- prépare des fichiers de démonstration ;
- démarre 2 esclaves et 1 maître ;
- exécute une série de commandes client ;
- vérifie automatiquement les résultats ;
- affiche une trace en direct dans le terminal ;
- écrit aussi des logs dans `logs/` ;
- arrête le cluster et supprime les fichiers temporaires à la fin.

## Pré-requis

Le script suppose que les outils suivants existent :

```bash
bash
make
gcc
python3
stdbuf
grep
awk
sed
tee
dd
head
sha256sum
pgrep
pkill
```

## Utilisation

Depuis la racine du dépôt :

```bash
make demo-q1-q17
```

Ou directement :

```bash
bash scripts/demo_q1_q17.sh
```

## Ce que vous voyez pendant la démo

Le script affiche des traces en direct :

- `[master]` en magenta ;
- `[slave1]` en vert ;
- `[slave2]` en cyan ;
- `[client-...]` en jaune.

Les couleurs ne sont affichées que si la sortie va vers un vrai terminal.

## Logs écrits sur disque

Le script garde aussi des logs dans `logs/` :

```text
logs/demo_master.log
logs/demo_slave1.log
logs/demo_slave2.log
logs/demo_client_q1_q9.log
logs/demo_client_q10.log
logs/demo_client_q13.log
logs/demo_client_q14.log
logs/demo_client_q15_q17.log
logs/demo_client_q16_rm.log
```

Note :

- l'étape de coupure brutale de la connexion réutilise aussi `logs/demo_client_q14.log` dans le script actuel ;
- le script nettoie les fichiers de démonstration à la fin.

## Fichiers de démonstration créés

Le script crée exactement les fichiers suivants :

```bash
printf 'Demonstration FTP Q1-Q7\n' > data_server/demo_q1_q7.txt
printf 'Visible in LS\n' > data_server/demo_q15_seed.txt
dd if=/dev/urandom of=data_server/demo_q8_q9.bin bs=1M count=1 status=none
dd if=/dev/urandom of=data_server/demo_q10_resume.bin bs=1M count=2 status=none
dd if=/dev/urandom of=data_server/demo_q14_reconnect.bin bs=1M count=256 status=none
printf 'upload from client\n' > data_client/demo_q16_upload.txt
```

Puis il supprime d'anciens fichiers de sortie côté client :

```bash
rm -f \
  data_client/demo_q1_q7.txt \
  data_client/demo_q8_q9.bin \
  data_client/demo_q10_resume.bin \
  data_client/demo_q14_reconnect.bin \
  data_server/demo_q16_upload.txt
```

## Déroulement exact de la démo

Le script est découpé en 12 étapes. Voici exactement ce qu'il fait.

### 1. Build du projet

Commandes exécutées :

```bash
make -C "$ROOT_DIR" clean
make -C "$ROOT_DIR" all
```

Si vous le lancez depuis la racine du dépôt, cela revient à :

```bash
make clean
make all
```

### 2. Préparation des fichiers

Commandes exécutées :

```bash
mkdir -p logs data_server data_client
printf 'Demonstration FTP Q1-Q7\n' > data_server/demo_q1_q7.txt
printf 'Visible in LS\n' > data_server/demo_q15_seed.txt
dd if=/dev/urandom of=data_server/demo_q8_q9.bin bs=1M count=1 status=none
dd if=/dev/urandom of=data_server/demo_q10_resume.bin bs=1M count=2 status=none
dd if=/dev/urandom of=data_server/demo_q14_reconnect.bin bs=1M count=256 status=none
printf 'upload from client\n' > data_client/demo_q16_upload.txt
rm -f data_client/demo_q1_q7.txt data_client/demo_q8_q9.bin data_client/demo_q10_resume.bin data_client/demo_q14_reconnect.bin data_server/demo_q16_upload.txt
```

### 3. Démarrage du cluster et validation Q11-Q12

Le script lance en arrière-plan :

```bash
stdbuf -oL -eL bin/serverFTP 1
stdbuf -oL -eL bin/serverFTP 2
stdbuf -oL -eL bin/masterFTP
```

En vrai, chaque sortie est aussi dupliquée dans un log avec `tee`, puis
préfixée en couleur pour l'affichage terminal.

Le script attend ensuite les messages suivants dans les logs :

```bash
wait_for_log logs/demo_slave1.log "waiting for master on control port"
wait_for_log logs/demo_slave2.log "waiting for master on control port"
wait_for_log logs/demo_master.log "slave 1 registered"
wait_for_log logs/demo_master.log "slave 2 registered"
wait_for_log logs/demo_master.log "listening on port 2121 with 2 registered slaves"
wait_for_log logs/demo_slave1.log "listening on client port 3001"
wait_for_log logs/demo_slave2.log "listening on client port 3002"
```

Cela valide :

- le démarrage des esclaves ;
- l'enregistrement au maître ;
- l'ouverture du port `2121` par le maître ;
- l'ouverture des ports clients `3001` et `3002`.

### 4. Démonstration Q1-Q9 par connexion directe à l'esclave 1

Commande exacte exécutée :

```bash
printf "get %s\nget %s\nbye\n" "demo_q1_q7.txt" "demo_q8_q9.bin" \
  | bin/clientFTP 127.0.0.1 3001
```

Après cela, le script vérifie exactement :

```bash
grep -q "Transfer successfully complete." logs/demo_client_q1_q9.log
test -f data_client/demo_q1_q7.txt
test -f data_client/demo_q8_q9.bin
```

Ce test montre :

- un `get` texte ;
- un `get` binaire ;
- plusieurs commandes sur une seule session ;
- le téléchargement côté client.

### 5. Démonstration Q10 : reprise d'un téléchargement interrompu

Le script tronque d'abord un fichier local :

```bash
head -c 700000 data_server/demo_q10_resume.bin > data_client/demo_q10_resume.bin
```

Puis il exécute :

```bash
printf "get %s\nbye\n" "demo_q10_resume.bin" \
  | bin/clientFTP 127.0.0.1 3001
```

Puis il vérifie exactement :

```bash
SERVER_HASH="$(sha256sum data_server/demo_q10_resume.bin | cut -d ' ' -f1)"
CLIENT_HASH="$(sha256sum data_client/demo_q10_resume.bin | cut -d ' ' -f1)"
[[ "$SERVER_HASH" == "$CLIENT_HASH" ]]
```

Ce test montre que le client reprend le transfert au bon offset.

### 6. Démonstration Q10 : robustesse après coupure brutale d'un client

Le script supprime d'abord l'ancienne copie côté client :

```bash
rm -f data_client/demo_q14_reconnect.bin
```

Puis il ouvre une socket brute avec Python, envoie une requête `GET`, et coupe
la connexion brutalement avec `SO_LINGER`.

Code Python exécuté exactement :

```python
import ctypes
import socket
import struct

FTP_MAX_FILENAME = 256
FTP_MAX_LOGIN = 32
FTP_MAX_PASSWORD = 64

class Request(ctypes.Structure):
    _fields_ = [
        ("version", ctypes.c_uint32),
        ("type", ctypes.c_uint32),
        ("offset", ctypes.c_uint64),
        ("file_size", ctypes.c_uint64),
        ("block_size", ctypes.c_uint32),
        ("filename", ctypes.c_char * FTP_MAX_FILENAME),
        ("login", ctypes.c_char * FTP_MAX_LOGIN),
        ("password", ctypes.c_char * FTP_MAX_PASSWORD),
    ]

request = Request()
request.version = 1
request.type = 1
request.block_size = 4096
request.filename = b"demo_q14_reconnect.bin"

sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.connect(("127.0.0.1", 3001))
sock.sendall(bytes(request))
sock.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, struct.pack("ii", 1, 0))
sock.close()
```

Ensuite le script relance un vrai téléchargement :

```bash
printf "get %s\nbye\n" "demo_q14_reconnect.bin" \
  | bin/clientFTP 127.0.0.1 3001
```

Puis il vérifie exactement :

```bash
SERVER_HASH="$(sha256sum data_server/demo_q14_reconnect.bin | cut -d ' ' -f1)"
CLIENT_HASH="$(sha256sum data_client/demo_q14_reconnect.bin | cut -d ' ' -f1)"
[[ "$SERVER_HASH" == "$CLIENT_HASH" ]]
```

Ce test montre que le service continue à fonctionner après une coupure brutale.

### 7. Démonstration Q13 : redirection du client par le maître

Le script supprime d'abord l'ancienne copie locale :

```bash
rm -f data_client/demo_q1_q7.txt
```

Puis il exécute :

```bash
printf "get %s\nbye\n" "demo_q1_q7.txt" \
  | bin/clientFTP 127.0.0.1
```

Puis il vérifie exactement :

```bash
grep -q "Redirection vers le slave" logs/demo_client_q13.log
grep -q "redirected client" logs/demo_master.log
test -f data_client/demo_q1_q7.txt
```

Ce test montre :

- que le client passe par le maître ;
- que le maître choisit un esclave ;
- que le client se reconnecte vers cet esclave.

### 8. Démonstration Q14 : reconnexion automatique après panne d'un worker

Le script supprime d'abord l'ancienne copie locale :

```bash
rm -f data_client/demo_q14_reconnect.bin
```

Puis il lance le client en arrière-plan :

```bash
printf "get %s\nbye\n" "demo_q14_reconnect.bin" \
  | bin/clientFTP 127.0.0.1
```

Ensuite il attend la première redirection :

```bash
wait_for_log logs/demo_client_q14.log "Redirection vers le slave" 2
```

Puis il récupère le numéro du slave choisi :

```bash
REDIRECTED_SLAVE=$(grep -m1 "Redirection vers le slave" logs/demo_client_q14.log | grep -oE 'slave [0-9]+' | cut -d' ' -f2)
```

Puis il choisit le processus parent de l'esclave :

```bash
if [[ "$REDIRECTED_SLAVE" -eq 1 ]]; then
  REDIRECTED_PARENT_PID="${PIDS[0]}"
else
  REDIRECTED_PARENT_PID="${PIDS[1]}"
fi
```

Puis il cherche un worker enfant et le tue :

```bash
sleep 0.5
REDIRECTED_WORKER_PID="$(pgrep -P "$REDIRECTED_PARENT_PID" | head -n 1 || true)"
if [[ -n "$REDIRECTED_WORKER_PID" ]]; then
  kill -KILL "$REDIRECTED_WORKER_PID" 2>/dev/null || true
fi
```

Puis il attend la fin du client et vérifie l'intégrité :

```bash
SERVER_HASH="$(sha256sum data_server/demo_q14_reconnect.bin | cut -d ' ' -f1)"
CLIENT_HASH="$(sha256sum data_client/demo_q14_reconnect.bin | cut -d ' ' -f1)"
[[ "$SERVER_HASH" == "$CLIENT_HASH" ]]
```

Ce test montre que le client sait :

- détecter la panne ;
- se reconnecter ;
- recevoir une nouvelle redirection ;
- reprendre le téléchargement.

### 9. Redémarrage propre du cluster avant Q15-Q17

Le script arrête d'abord le cluster courant :

```bash
kill -INT <pids enregistrés>
pkill -INT serverFTP
pkill -INT masterFTP
wait <pids enregistrés>
```

Puis il relance :

```bash
stdbuf -oL -eL bin/serverFTP 1
stdbuf -oL -eL bin/serverFTP 2
stdbuf -oL -eL bin/masterFTP
```

### 10. Démonstration Q15-Q17 : `ls`, `auth`, `put`

Commande exacte exécutée :

```bash
printf "ls\nput %s\nauth admin wrong\nauth admin srftp\nput %s\nls\nbye\n" "demo_q16_upload.txt" "demo_q16_upload.txt" \
  | bin/clientFTP 127.0.0.1
```

Cela veut dire exactement :

```text
ls
put demo_q16_upload.txt
auth admin wrong
auth admin srftp
put demo_q16_upload.txt
ls
bye
```

Puis le script vérifie exactement :

```bash
grep -q "demo_q15_seed.txt" logs/demo_client_q15_q17.log
grep -q "AUTH_REQUIRED" logs/demo_client_q15_q17.log
grep -q "AUTH_FAILED" logs/demo_client_q15_q17.log
grep -q "Authentication successful." logs/demo_client_q15_q17.log
grep -q "Upload successfully complete." logs/demo_client_q15_q17.log
grep -q "demo_q16_upload.txt" logs/demo_client_q15_q17.log
test -f data_server/demo_q16_upload.txt
```

Puis il repère le slave source du `put` :

```bash
PUT_SOURCE_SLAVE="$(grep -m1 "Redirection vers le slave" logs/demo_client_q15_q17.log | grep -oE 'slave [0-9]+' | cut -d' ' -f2)"
```

Puis il vérifie la trace de réplication sur l'autre esclave :

```bash
if [[ "$PUT_SOURCE_SLAVE" -eq 1 ]]; then
  grep -q "applied replicated PUT 'demo_q16_upload.txt'" logs/demo_slave2.log
else
  grep -q "applied replicated PUT 'demo_q16_upload.txt'" logs/demo_slave1.log
fi
```

Ce test montre :

- `ls` ;
- refus de `put` sans auth ;
- échec d'auth avec mauvais mot de passe ;
- succès d'auth ;
- succès de `put` ;
- visibilité du nouveau fichier ;
- réplication du `put`.

### 11. Démonstration Q16 : suppression après authentification

Commande exacte exécutée :

```bash
printf "ls\nauth admin srftp\nrm %s\nls\nbye\n" "demo_q16_upload.txt" \
  | bin/clientFTP 127.0.0.1
```

Cela veut dire exactement :

```text
ls
auth admin srftp
rm demo_q16_upload.txt
ls
bye
```

Puis le script vérifie exactement :

```bash
grep -q "demo_q16_upload.txt" logs/demo_client_q16_rm.log
grep -q "File 'demo_q16_upload.txt' removed." logs/demo_client_q16_rm.log
test ! -f data_server/demo_q16_upload.txt
```

Puis il repère le slave source du `rm` :

```bash
RM_SOURCE_SLAVE="$(grep -m1 "Redirection vers le slave" logs/demo_client_q16_rm.log | grep -oE 'slave [0-9]+' | cut -d' ' -f2)"
```

Puis il vérifie la trace de réplication sur l'autre esclave :

```bash
if [[ "$RM_SOURCE_SLAVE" -eq 1 ]]; then
  grep -q "applied replicated RM 'demo_q16_upload.txt'" logs/demo_slave2.log
else
  grep -q "applied replicated RM 'demo_q16_upload.txt'" logs/demo_slave1.log
fi
```

Ce test montre :

- succès de l'auth ;
- suppression du fichier ;
- disparition réelle du fichier ;
- réplication du `rm`.

### 12. Résumé final affiché par le script

Le script affiche exactement :

```text
- Q1-Q10: GET, multi-command sessions, resume and abrupt client disconnect handling still work
- Q11-Q12: master registers the slaves before listening
- Q13: client is redirected automatically by the master
- Q14: client reconnects and resumes after slave failure
- Q15: LS lists server files
- Q16: PUT uploads and RM removes files, then propagates them to the other slave
- Q17: PUT/RM are protected by AUTH
PASS
```

## Arrêt et nettoyage faits automatiquement

À la fin, même en cas d'erreur, le script exécute un nettoyage via `trap cleanup EXIT`.

Il fait :

```bash
kill -INT <pids enregistrés>
pkill -INT serverFTP
pkill -INT masterFTP
wait <pids enregistrés>
```

Puis il supprime les fichiers de démonstration :

```bash
rm -f \
  data_server/demo_q1_q7.txt \
  data_server/demo_q8_q9.bin \
  data_server/demo_q10_resume.bin \
  data_server/demo_q14_reconnect.bin \
  data_server/demo_q16_upload.txt \
  data_server/demo_q15_seed.txt

rm -f \
  data_client/demo_q1_q7.txt \
  data_client/demo_q8_q9.bin \
  data_client/demo_q10_resume.bin \
  data_client/demo_q14_reconnect.bin \
  data_client/demo_q16_upload.txt
```

## En résumé

Si vous voulez juste lancer la démonstration :

```bash
make demo-q1-q17
```

Si vous voulez voir exactement ce qu'elle fait, ce fichier vous donne :

- les commandes exactes ;
- les vérifications exactes ;
- les fichiers créés ;
- les logs écrits ;
- les points fonctionnels testés.
