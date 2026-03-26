// Interface du point d'entree du client FTP.
#ifndef CLIENTFTP_H
#define CLIENTFTP_H

// Ouvre une connexion au serveur et execute une session FTP interactive.
int ftp_client_run(const char *host, int port);

#endif
