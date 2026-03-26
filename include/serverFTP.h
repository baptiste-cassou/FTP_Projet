// Interface du point d'entree du serveur FTP
#ifndef SERVERFTP_H
#define SERVERFTP_H

// Initialise un serveur esclave, attend le maitre, puis lance les workers FTP.
int ftp_server_run(int slave_id);

#endif
