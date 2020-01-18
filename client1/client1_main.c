/*
 * Developed by Rinaldo Clemente <s259536@studenti.polito.it>, May 2019.
 * Client TCP che si connette ad un server con indirizzo e porta specificati
 * come primo e secondo parametro sulla riga di comando.
 * Richiede uno o più files come successivi argomenti sulla riga di comando e li mostra
 * seguendo un protocollo definito.
 */

#include <errno.h>
#include <string.h>
#include "../errlib.h"
#include "../sockwrap.h"

#define MAXBUFL 4096		 /* Lunghezza buffer. */
#define MSG_ERROR "-ERR\r"     /* Risposta negativa dal server. */
#define MSG_GET "GET "		 /* Messaggio di richiesta dal client. */
#define MSG_OK "+OK\r\n"	 /* Risposta positiva dal server. */
#define TIMEOUT 15		 /* TIMEOUT per la Select() (sec). */

/* Prototipi di funzione. */
void doRequest(int argc, char *argv[], int sockfd);

/* Variabili globali. */
char *prog_name;

int main(int argc, char *argv[])
{
        /* Per la libreria errlib. */
        prog_name = argv[0];

        int sockfd;

        if (argc < 4)
                err_quit("usage: %s <dest_host> <dest_port> <filename1> <filename2> ...", prog_name);
        else
        {
                /* tcp_connect() crea una socket TCP e si connette al server. */
                sockfd = tcp_connect(argv[1], argv[2]);

                /* Crea una richiesta di file sulla socket socketfd. */
                doRequest(argc, argv, sockfd);

                /* Chiude correttamente la socket. */
                Close(sockfd);

                /* Programma terminato correttamente. */
                return 0;
        }
}

void doRequest(int argc, char *argv[], int sockfd)
{
        char buffer[MAXBUFL]; /* Buffer usato lato client. */
        int i;                /* Contatore per il loop. */

        /* Settaggio del timer. */
	fd_set read_set;
	struct timeval tval;
	tval.tv_sec = TIMEOUT;	  	 /* Numero di secondi. */
	tval.tv_usec = 0;		 /* Numero di microsecondi. */
	FD_ZERO(&read_set);
	FD_SET(sockfd, &read_set);

	for (i = 3; i < argc; i++)
        {
                /* Calcola e salva la lunghezza del filename */
                size_t length = strlen(argv[i]);

                /* Cancelliamo tutti i byte del buffer. */
                memset(buffer, 0, MAXBUFL);

                /* Creiamo il comando per richiedere il file seguendo il protocollo. */
                strcpy(buffer, MSG_GET);
                strncat(buffer, argv[i], length);
                strncat(buffer, "\r\n", 2);

                /* Inviamo il comando. */
                Writen(sockfd, buffer, strlen(buffer));

                if (Select(FD_SETSIZE, &read_set, NULL, NULL, &tval) > 0)
                {
                        /* Riceviamo la risposta dal server. */
                        Recv(sockfd, buffer, 5, 0);

                        /* strncmp() ritorna 0 se la risposta è positiva. */
                        if (strncmp(buffer, MSG_OK, 5) == 0)
                        {
                    				u_int32_t file_bytes;

                    				/* Tutti i descrittori che non sono pronti al ritorno della select()
                    				   avranno bit del descriptor set puliti.
                    				   Riportiamo per sicurezza i bit che ci interessano a 1. */
                    				if (Select(FD_SETSIZE, &read_set, NULL, NULL, &tval) > 0)
                      			{
                                /* Riceviamo il numero di byte del file richiesto tramite la socket. */
                                Recv(sockfd, &file_bytes, 4, 0);
                                file_bytes = ntohl(file_bytes);
                    				}

                    				/* select() ritorna 0 (timeout). */
                    				else
                    				{
                    				        printf("(%s) - timeout waiting for data from server\n", prog_name);

                    				        return;
                    				}

                                FILE *fPtr;
                                int n;                                 /* Numero di byte ricevuti. */
                                u_int32_t remaining_data = file_bytes; /* Dati da leggere, inizialmente uguali al numero di byte del file. */

                                char *temp = argv[i];

                                /* Se viene richiesto il file in un path cerchiamo se nel nome del file
				   c'è "/", se c'è andiamo a cercare l'ultima occorrenza di "/" e poi
				   prendiamo il nome del file. */

                                if (strstr(argv[i], "/") != NULL)

                                        temp = (strrchr(argv[i], '/')) + 1;

                                fPtr = Fopen(temp, "w");


                        				int var=0;
                                                        while (remaining_data>0)
                                                        {
                        					/*Se non è stato inviato nessun dato nel precedente ciclo di while, c'è un problema lato server. */
                        					if(var==remaining_data){
                        						err_msg("\n(%s) error - server side, closing..", prog_name);
                                                                	return;
                        					}

                        					var= remaining_data;
                        					if(remaining_data<sizeof(buffer)){
                        						n=Recv(sockfd,buffer,remaining_data,0);
                        						fwrite(buffer, sizeof(char), n, fPtr);
                        						remaining_data=0;

                        					}
                        					else{
                        						n=Recv(sockfd, buffer, sizeof(buffer), 0);
                        			                        fwrite(buffer, sizeof(char), n, fPtr);
                        						remaining_data -= n;
                        					}



                                                                /* Teniamo traccia della percentuale di dati sccaricati. */
                                                                printf("\rDownloading: %lu%%     ", (unsigned long)(file_bytes - remaining_data) * 100 / file_bytes);


                                                        }
                        				Fclose(fPtr);


                        				char timestamp[10]="";
                        				u_int32_t timest;

                        				/* Riceviamo tramite sockfd la data dell'ultima modifica (timestamp). */
                        				if (Select(FD_SETSIZE, &read_set, NULL, NULL, &tval) > 0)
                                        		{

                        				        Recv(sockfd, &timestamp, 4, 0);
                        				}
                        				else
                        				{
                        				        printf("(%s) - timeout waiting for data from server\n", prog_name);

                        				        return;
                        				}

                        		                timest = ntohl(*(uint32_t *)timestamp);

	                        printf("\nReceived file %s\nReceived file size %u\nReceived file timestamp %u\n", temp, file_bytes, timest);

                        }

                        /* strncmp() è uguale a 0 se riceviamo una risposta negativa dal server. */
                        else if (strncmp(buffer, MSG_ERROR, 5) == 0)
                        {
                                Recv(sockfd, buffer, 1, 0);
                                if (strncmp(buffer, "\n", 1) == 0)
                                {
                                	err_msg("(%s) error - server side, closing..", prog_name);
                                        return;
                                }
                        }

                        /* Se si riceve altro. */
                        else
                        {
                                err_msg("(%s) error - invalid response, closing..", prog_name);

                                return;
                        }
                }
                /* select() ritorna 0 (timeout). */
                else
                {
                        printf("(%s) - timeout waiting for data from server\n", prog_name);

                        return;
                }
        }

        return; /* Torniamo alla funzione chiamante. */
}
