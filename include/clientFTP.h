// Interface du point d'entree du client FTP.
#ifndef CLIENTFTP_H
#define CLIENTFTP_H

// Ouvre une connexion au serveur et execute une unique commande interactive.
int ftp_client_run(const char *host);

#endif
