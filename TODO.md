# TODO - Projet FTP par questions du sujet

## Étape I : Serveur FTP de base

- [x] **Q1 - Définir `typereq_t`**  
  Créer un type énuméré pour les requêtes client (au minimum `GET`, puis extensible à `PUT`, `LS`, etc.).

- [x] **Q2 - Définir `request_t`**  
  Créer la structure de requête contenant au moins le type de requête et le nom de fichier.

- [x] **Q3 - Construire le squelette client/serveur FTP**  
  Partir de l'exemple echo, renommer en `clientFTP`/`serverFTP`, utiliser le port `2121`, et mettre en place un pool de processus configuré avec `NB_PROC`.

- [x] **Q4 - Terminaison propre du serveur (`SIGINT`)**  
  Installer un handler de signal, transmettre `SIGINT` aux fils, fermer les sockets, et éviter les processus zombies. 

- [x] **Q5 - Dossiers de travail client/serveur**  
  Utiliser explicitement `data_server/` pour les fichiers servis et `data_client/` pour les fichiers reçus.

- [x] **Q6 - Traitement côté serveur**  
  Implémenter :
  - une requête par connexion (étape I),
  - réception et décodage de `request_t`,
  - envoi de `response_t` (succès/erreur),
  - traitement de `GET`,
  - transfert de fichiers texte et binaires.

- [x] **Q7 - Traitement côté client**  
  Encoder la saisie en `request_t`, envoyer la requête, recevoir `response_t`, sauvegarder le fichier, afficher erreurs ou succès avec statistiques de transfert.

## Étape II : Amélioration du serveur FTP

- [x] **Q8 - Découpage du fichier en blocs**  
  Passer à un transfert par blocs de taille fixe et définir clairement le protocole (taille, nombre/fin des blocs).

- [x] **Q9 - Plusieurs demandes par connexion**  
  Garder la connexion ouverte pour plusieurs commandes, fermeture explicite par `bye`.

- [ ] **Q10 - Gestion des pannes côté client + reprise**  
  Conserver l'avancement localement et reprendre un transfert interrompu à partir de l'offset correct.

## Étape III : Répartition de charge

- [ ] **Q11 - Définir `NB_SLAVES` et ports esclaves**  
  Choisir un nombre fixe d'esclaves et des ports dédiés, distincts de `2121`.

- [ ] **Q12 - Interconnexion maître/esclaves**  
  Faire établir au maître les connexions vers les esclaves au démarrage et stocker les informations nécessaires.

- [ ] **Q13 - Adapter la connexion client**  
  Après connexion au maître, faire recevoir au client les infos de l'esclave, puis basculer la session FTP vers cet esclave.

- [ ] **Q14 - Bonus panne esclave**  
  Proposer/implémenter un protocole de redirection vers un esclave sain en cas de panne.

## Étape IV : Opérations avancées

- [ ] **Q15 - Commande `ls`**  
  Retourner le contenu du répertoire courant du serveur (via `fork`/`exec`/`dup2` ou `popen`).

- [ ] **Q16 - Commandes `rm` et `put`**  
  Permettre suppression et téléversement, puis répercuter les changements vers les autres esclaves (cohérence éventuelle).

- [ ] **Q17 - Authentification**  
  Mettre en place login/mot de passe et refuser `put`/`rm` sans authentification préalable.

## Vérification et rendu

- [ ] Vérifier chaque étape avec des tests reproductibles (succès, erreurs, fichiers binaires, pannes/reprise).
- [ ] Finaliser le compte-rendu (architecture, protocole, tests, limites).
- [ ] Préparer une démonstration propre (`make all`, lancement serveur/client, scénario de test).
