// Interface du point d'entree du serveur FTP
#ifndef SERVERFTP_H
#define SERVERFTP_H

// Initialise le serveur, lance les workers et attend l'arret par signal
int ftp_server_run(void);

#endif
