/*
 * Developed by Rinaldo Clemente <s259536@studenti.polito.it>, May 2019.
 *
 * Server TCP sequenziale che ascolta su porta specificata come primo parametro.
 * Accetta trasferimenti di file dopo aver stabilito una connessione TCP con il client.
 * Risponde inviando i file richiesti, seguendo un protocollo definito.
 */

#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include "../errlib.h"
#include "../sockwrap.h"

#define MAXBUFL 4096		 /* Lunghezza buffer. */
#define MSG_ERROR "-ERR\r\n"     /* Risposta negativa dal server. */
#define MSG_GET "GET "		 /* Messaggio di richiesta dal client. */
#define MSG_OK "+OK\r\n"	 /* Risposta positiva dal server. */
#define TIMEOUT 15		 /* TIMEOUT per la Select() (sec). */

/* Prototipi di funzione. */
void manageRequest(int connfd, struct sockaddr_storage cliaddr, socklen_t clilen);

/* Variabili globali. */
char *prog_name;

int main(int argc, char *argv[])
{
	/* Per la libreria errlib. */
	prog_name = argv[0];

	int listenfd;

	if (argc < 2)
		err_quit("usage: %s <port>", prog_name);
	else
	{
		/* La listen crea la socket TCP, fa la bind sulla porta e permette connessioni da accettare. */
		listenfd = tcp_listen(NULL, argv[1], NULL);

		int connfd; /* Socket connessa. */

		struct sockaddr_storage cliaddr; /* Indirizzo Client. */

		/* Server loop. */
		for (;;)
		{
			socklen_t clilen = sizeof(cliaddr); /* Lunghezza Client. */

			connfd = Accept(listenfd, (struct sockaddr *)&cliaddr, &clilen);

			printf("(%s) --- accepted connection from client [%s]\n", prog_name, sock_ntop((struct sockaddr *)&cliaddr, clilen));

			/* Processa la richiesta */
			manageRequest(connfd, cliaddr, clilen);
			if (close(connfd) != 0)
				err_ret("(%s) error - close() failed with client [%s]", prog_name, sock_ntop((struct sockaddr *)&cliaddr, clilen));
		}
	}
	/* Programma terminato correttamente. */
	return 0;
}


void manageRequest(int connfd, struct sockaddr_storage cliaddr, socklen_t clilen)
{
	/* Settaggio del timer. */
	fd_set read_set;
	struct timeval tval;
	tval.tv_sec = TIMEOUT;	  	 /* Numero di secondi. */
	tval.tv_usec = 0;		 /* Numero di microsecondi. */
	FD_ZERO(&read_set);
	FD_SET(connfd, &read_set);

	int nByteRead; /* Numero di byte ricevuti dalla connfd. */

	for (;;)
	{
		char buffer[MAXBUFL]; /* Buffer utilizzato lato server. */

		/* Cancelliamo tutti i byte del buffer. */
		memset(buffer, 0, MAXBUFL);

		if (select(FD_SETSIZE, &read_set, NULL, NULL, &tval) > 0)
		{
			/* Riceviamo dal socket connesso. */
			if ((nByteRead = recv(connfd, buffer, 4, 0)) == 0)
			{
			printf("(%s) --- connection closed by party [%s]\n", prog_name, sock_ntop((struct sockaddr *)&cliaddr, clilen));
				break;
			}
			else if (nByteRead < 0)
			{
				err_ret("(%s) error - recv() failed with client [%s]", prog_name, sock_ntop((struct sockaddr *)&cliaddr, clilen));
				break;
			}
			else
			{
				/* strncmp() è uguale a 0 se riceviamo un messaggio di richiesta dal client. */
				if (strncmp(buffer, MSG_GET, 4) == 0)
				{
					memset(buffer, 0, MAXBUFL);

					/* Tutti i descrittori che non sono pronti al ritorno della select()
					   avranno bit del descriptor set puliti.
					   Riportiamo per sicurezza i bit che ci interessano a 1. */

					FD_SET(connfd, &read_set);

					if (select(FD_SETSIZE, &read_set, NULL, NULL, &tval) > 0)
					{
						/* Leggiamo il nome del file più \r\n, \0 è aggiunto dalla funzione. */
						nByteRead = readline_unbuffered(connfd, buffer, MAXBUFL);

						if (nByteRead == 0)
						{
							err_msg("(%s) error - connection closed by party [%s]", prog_name, sock_ntop((struct sockaddr *)&cliaddr, clilen));
							break;
						}
						else if (nByteRead < 0)
						{
							err_ret("(%s) error - readline_unbuffered() failed with client [%s]", prog_name, sock_ntop((struct sockaddr *)&cliaddr, clilen));
							break;
						}
						else
						{
							/* Prendiamo solo il nome del file, senza altri caratteri. */
							char *token = strtok(buffer, "\r");

							printf("(%s) --- received string '%s' from client [%s]\n", prog_name, token, sock_ntop((struct sockaddr *)&cliaddr, clilen));

							char filename[MAXBUFL];

							strcpy(filename, token);

							/* Controlliamo se il file è accessibile. */
							if ((access(filename, F_OK)) != -1)
							{
								/* File esiste. */

								printf("(%s) --- client [%s] asked to send file '%s'\n", prog_name, sock_ntop((struct sockaddr *)&cliaddr, clilen), filename);

								memset(buffer, 0, MAXBUFL);
								strncpy(buffer, MSG_OK, 5);

								if ((sendn(connfd, buffer, 5, MSG_NOSIGNAL)) != 5)
								{
									err_ret("(%s) error - sendn() failed with client [%s]", prog_name, sock_ntop((struct sockaddr *)&cliaddr, clilen));

									if ((close(connfd)) == 0)
										break;
									else
									{
										err_ret("(%s) error - close() failed with client [%s]", prog_name, sock_ntop((struct sockaddr *)&cliaddr, clilen));
										break;
									}
								}

								struct stat stat_buf;

								/* Otteniamo informazioni dal file (byte, timestamp). */
								if ((stat(filename, &stat_buf)) != 0)
								{
									err_ret("(%s) error - stat() of the file '%s' failed with client [%s]", prog_name, filename, sock_ntop((struct sockaddr *)&cliaddr, clilen));

									if ((close(connfd)) == 0)
										break;
									else
									{
										err_ret("(%s) error - close() failed with client [%s]", prog_name, sock_ntop((struct sockaddr *)&cliaddr, clilen));
										break;
									}
								}

								/* Inviamo il numero di byte. */

								u_int32_t file_dim = htonl(stat_buf.st_size);

								if ((sendn(connfd, &file_dim, 4, MSG_NOSIGNAL)) != 4)
								{
									err_ret("(%s) error - sendn() failed with client [%s]", prog_name, sock_ntop((struct sockaddr *)&cliaddr, clilen));

									if ((close(connfd)) == 0)
										break;
									else
									{
										err_ret("(%s) error - close() failed with client [%s]", prog_name, sock_ntop((struct sockaddr *)&cliaddr, clilen));
										break;
									}
								}

								int n = 0;				    	/* Numero di byte. */
								int i;						/* Numero di byte letti. */
								u_int32_t remaining_data = ntohl(file_dim); 	/* Dati rimasti da inviare. */

								FILE *fPtr;
								if ((fPtr = fopen(filename, "r")) == NULL)
								{
									err_msg("(%s) error - fopen of '%s' failed with client [%s]: %s", prog_name, filename, sock_ntop((struct sockaddr *)&cliaddr, clilen), strerror(errno));

									if ((close(connfd)) == 0)
										break;
									else
									{
										err_ret("(%s) error - close() failed with client [%s]", prog_name, sock_ntop((struct sockaddr *)&cliaddr, clilen));
										break;
									}
								}

								/* Leggiamo dal file puntato da fPtr MAXBUF elementi di dati, ognuno di un byte
								   e li salviamo nel buffer. */
								i = fread(buffer, sizeof(char), MAXBUFL, fPtr);

								while ((n = sendn(connfd, buffer, i, MSG_NOSIGNAL)) > 0)
								{
									memset(buffer, 0, MAXBUFL);

									i = fread(buffer, sizeof(char), MAXBUFL, fPtr);

									/* Teniamo conto dei dati rimasti. */
									remaining_data -= n;

									/* Se il file è stato inviato correttamente. */
									if (remaining_data <= 0)
									{
										printf("(%s) --- sent file '%s' to client [%s]\n", prog_name, filename, sock_ntop((struct sockaddr *)&cliaddr, clilen));

										if ((fclose(fPtr)) == 0)
											break;
										else
										{
											err_ret("(%s) error - fclose() failed with client [%s]", prog_name, sock_ntop((struct sockaddr *)&cliaddr, clilen));
											break;
										}
									}
								}
								/* Se il file non è stato inviato correttamente chiudiamo la connessione per evitare loop infiniti. */
								if (remaining_data > 0)
								{
									err_ret("(%s) error - sendn() failed with client [%s]", prog_name, sock_ntop((struct sockaddr *)&cliaddr, clilen));

									if ((fclose(fPtr)) == 0)
									{
										if ((close(connfd)) == 0)
											break;
										else
										{
											err_ret("(%s) error - close() failed with client [%s]", prog_name, sock_ntop((struct sockaddr *)&cliaddr, clilen));
											break;
										}
									}
									else
									{
										err_ret("(%s) error - fclose() failed with client [%s]", prog_name, sock_ntop((struct sockaddr *)&cliaddr, clilen));

										if ((close(connfd)) == 0)
											break;
										else
										{
											err_ret("(%s) error - close() failed with client [%s]", prog_name, sock_ntop((struct sockaddr *)&cliaddr, clilen));
											break;
										}
									}
								}

								/* Invio timestamp. */
								u_int32_t timestamp = htonl(stat_buf.st_mtime);

								if ((sendn(connfd, &timestamp, 4, MSG_NOSIGNAL)) != 4)
								{
									err_ret("(%s) error - sendn() failed with client [%s]", prog_name, sock_ntop((struct sockaddr *)&cliaddr, clilen));

									if ((close(connfd)) == 0)
										break;
									else
									{
										err_ret("(%s) error - close() failed with client [%s]", prog_name, sock_ntop((struct sockaddr *)&cliaddr, clilen));
										break;
									}
								}
							}
							else
							{
								/* File non esistente. */

								err_msg("(%s) error - access() failed with client [%s]: %s", prog_name, sock_ntop((struct sockaddr *)&cliaddr, clilen), strerror(errno));

								if ((sendn(connfd, MSG_ERROR, sizeof(char) * 6, MSG_NOSIGNAL)) != 6)
									err_ret("(%s) error - sendn() failed with client [%s]", prog_name, sock_ntop((struct sockaddr *)&cliaddr, clilen));

								if ((close(connfd)) == 0)
									break;
								else
								{
									err_ret("(%s) error - close() failed with client [%s]", prog_name, sock_ntop((struct sockaddr *)&cliaddr, clilen));
									break;
								}
							}
						}
					}
					else if (select(FD_SETSIZE, &read_set, NULL, NULL, &tval) == 0)
					{
						printf("(%s) Timeout waiting for data from client [%s]: connection with client will be closed\n", prog_name, sock_ntop((struct sockaddr *)&cliaddr, clilen));

						if ((close(connfd)) == 0)
							break;
						else
						{
							err_ret("(%s) error - close() failed with client [%s]", prog_name, sock_ntop((struct sockaddr *)&cliaddr, clilen));
							break;
						}
					}
					else
					{
						err_ret("(%s) error - select() failed with client [%s]", prog_name, sock_ntop((struct sockaddr *)&cliaddr, clilen));

						if ((close(connfd)) == 0)
							break;
						else
						{
							err_ret("(%s) error - close() failed with client [%s]", prog_name, sock_ntop((struct sockaddr *)&cliaddr, clilen));
							break;
						}
					}
				}

				/* Se non è un messaggio di GET. */
				else
				{
					err_msg("(%s) error - illegal command from client [%s]", prog_name, sock_ntop((struct sockaddr *)&cliaddr, clilen));

					if ((sendn(connfd, MSG_ERROR, sizeof(char) * 6, MSG_NOSIGNAL)) != 6)
						err_ret("(%s) error - sendn() failed with client [%s]", prog_name, sock_ntop((struct sockaddr *)&cliaddr, clilen));

					if ((close(connfd)) == 0)
						break;
					else
					{
						err_ret("(%s) error - close() failed with client [%s]", prog_name, sock_ntop((struct sockaddr *)&cliaddr, clilen));
						break;
					}
				}

			}
		}
		/* select() ritorna 0 (timeout). */
		else if (select(FD_SETSIZE, &read_set, NULL, NULL, &tval) == 0)
		{
			printf("(%s) Timeout waiting for data from client [%s]: connection with client will be closed\n", prog_name, sock_ntop((struct sockaddr *)&cliaddr, clilen));

			if ((close(connfd)) == 0)
				break;
			else
			{
				err_ret("(%s) error - close() failed with client [%s]", prog_name, sock_ntop((struct sockaddr *)&cliaddr, clilen));
				break;
			}
		}
		/* select() ritorna -1 (errore). */
		else
		{
			err_ret("(%s) error - select() failed with client [%s]", prog_name, sock_ntop((struct sockaddr *)&cliaddr, clilen));

			if ((close(connfd)) == 0)
				break;
			else
			{
				err_ret("(%s) error - close() failed with client [%s]", prog_name, sock_ntop((struct sockaddr *)&cliaddr, clilen));
				break;
			}
		}
	}
	return; /* Torniamo alla funzione chiamante. */
}
