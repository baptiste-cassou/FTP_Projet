## 1. Q11 : préparation de l'architecture maître / esclaves

### `include/ftp_shared.h`
- J'ai ajouté `NB_SLAVES`.
- J'ai ajouté `FTP_MASTER_PORT`.
- J'ai ajouté `FTP_SLAVE_CLIENT_BASE_PORT` et `FTP_SLAVE_CTRL_BASE_PORT`.
- J'ai ajouté les macros `FTP_SLAVE_CLIENT_PORT(id)` et `FTP_SLAVE_CTRL_PORT(id)`.
- J'ai nettoyé les commentaires liés aux ports.

### `include/serverFTP.h`
- J'ai changé la signature de `ftp_server_run()` pour qu'elle prenne un `slave_id`.

### `src/serverFTP.c`
- J'ai fait en sorte que `serverFTP` prenne maintenant un identifiant d'esclave en argument.
- J'ai ajouté la validation de `slave_id` dans `main`.
- J'ai fait calculer le port client à partir de `FTP_SLAVE_CLIENT_PORT(slave_id)`.
- J'ai évité le conflit avec le port du maître.

### `Makefile`
- J'ai gardé un binaire `masterFTP` séparé.
- J'ai ajouté `SLAVE_ID ?= 1`.
- J'ai mis à jour `run-server` pour lancer `serverFTP` avec un identifiant d'esclave.

### `src/clientFTP.c`
- J'ai remplacé l'ancien usage de `FTP_PORT` par `FTP_MASTER_PORT`.

## 2. Q12 : interconnexion maître / esclaves

### `include/ftp_shared.h`
- J'ai ajouté `FTP_MAX_HOST`.
- J'ai ajouté la structure `slave_hello_t` utilisée pour la connexion de contrôle entre maître et esclaves.

### `src/serverFTP.c`
- J'ai ajouté un socket d'écoute de contrôle par esclave.
- J'ai ajouté un socket de connexion de contrôle avec le maître.
- J'ai ajouté une phase d'attente du maître avant le lancement des workers FTP.
- J'ai fait envoyer un `slave_hello_t` au maître avec :
  - la version du protocole ;
  - l'identifiant de l'esclave ;
  - le port client ;
  - le port de contrôle ;
  - le nom d'hôte.
- J'ai ajouté la fermeture propre des descripteurs de contrôle sur `SIGINT`.
- J'ai fermé les descripteurs de contrôle dans les processus fils avant la boucle de service client.

### `src/server_master.c`
- J'ai remplacé le squelette vide par une vraie implémentation de `masterFTP`.
- J'ai ajouté une structure locale pour mémoriser les esclaves enregistrés.
- J'ai fait une connexion séquentielle du maître à chaque esclave sur son port de contrôle.
- J'ai ajouté la réception et la validation de `slave_hello_t`.
- J'ai stocké les informations de chaque esclave dans un tableau.
- J'ai fait en sorte que le maître n'écoute sur `FTP_MASTER_PORT` qu'après l'enregistrement complet des esclaves.
- J'ai ajouté un handler `SIGINT` pour fermer les sockets ouverts.
- J'ai laissé un comportement temporaire côté client :
  - le maître accepte la connexion ;
  - il journalise qu'une redirection Q13 est encore en attente ;
  - il ferme ensuite la connexion.

## 3. Démonstration Q1 à Q12

### `include/clientFTP.h`
- J'ai changé la signature de `ftp_client_run()` pour accepter un port.

### `src/clientFTP.c`
- J'ai fait en sorte que le client accepte désormais `./bin/clientFTP <host> [port]`.
- J'ai gardé `FTP_MASTER_PORT` comme port par défaut.
- J'ai permis une connexion directe à un esclave pour la démonstration de Q1 à Q10 tant que la Q13 n'est pas implémentée.
- J'ai corrigé le `main` pour qu'il retourne le code de retour de `ftp_client_run()`.

### `scripts/demo_q1_q12.sh`
- J'ai créé un script de démonstration complet.
- Ce script :
  - compile le projet ;
  - crée des fichiers de démonstration côté serveur ;
  - démarre deux esclaves ;
  - démarre le maître ;
  - vérifie l'enregistrement des esclaves dans les logs ;
  - lance un client directement sur l'esclave 1 pour démontrer Q1 à Q9 ;
  - simule une reprise de téléchargement pour Q10 ;
  - compare les `sha256` pour valider la reprise ;
  - nettoie les processus et les fichiers temporaires à la fin.
- J'ai rendu le script exécutable.

### `Makefile`
- J'ai ajouté la cible `demo-q1-q12`.
- J'ai redirigé `run-all` vers `demo-q1-q12`.

## 4. Compte-rendu

### `CR.tex`
- J'ai ajouté la Q11 dans le tableau d'avancement et dans l'étape 3.
- J'ai refactoré le plan général du compte-rendu pour suivre :
  - `Introduction`
  - `Architecture`
  - `Client et serveur et description du protocole`
  - `Etape 1`
  - `Etape 2`
  - `Etape 3`
  - `Etape 4`
- J'ai ajouté la Q12 dans le tableau d'avancement.
- J'ai mis à jour l'encadré d'état synthétique.
- J'ai ajouté une sous-section `Question 12 réalisée` dans l'étape 3.
- J'ai précisé explicitement que la redirection client relève encore de la Q13.