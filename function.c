/**
 * @file function.c
 * @brief Breve descrizione del file.
 *
 * 
 *
 * @authors Simone Arcari, Valeria Villano
 * @date 2023-09-09 (nel formato YYYY-MM-DD)
 */


#include <time.h>
#include <stdio.h>
#include <errno.h>
#include <netdb.h>
#include <signal.h>
#include <unistd.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "double_linked_list_opt.h"
#include "function.h"
#include "gobackn.h"


/* Dichiarazione esterna delle variabili globali */
extern unsigned long n_clt;                    // nummero di client connessi
extern int server_socket;                      // socket UDP di questo server
extern struct sockaddr_in client_address;      // contiene l'indirizzo del client corrente
extern pthread_mutex_t mutex_rcv;              // mutex per la gestione della ricezione
extern pthread_mutex_t mutex_snd;              // mutex per la gestione degli invii
extern list_t client_list;                     // lista client correntemetne connessi al server
extern node_t *pos_client;                     // ultimo client correntemente connesso


/* 
    Verifica che la cartella sia presente in caso contrario la creo
*/
DIR *check_directory(const char *path_name) {
    
    DIR *directory; // puntatore a cartella

RETRY:
    directory = opendir(path_name);


    if (directory == NULL) {
        printf("Errore[%d] opendir(): %s\n",errno , strerror(errno));

        /* se la cartella non esisteva la creo */
        if (errno == ENOENT) {
            if (mkdir(PATH_FILE_FOLDER, 0755) == 0) {       // permessi numerazione ottale |7|7|5|
                printf("Cartella creata con successo\n");
                goto RETRY;

            } else {
                printf("Errore[%d] mkdir(): %s\n", errno , strerror(errno));


                return NULL;
            }

        } else {

            return NULL;
        }
    }

    return directory;
}


/*
    Implementa la logica per avviare un thread
    Utilizza la socket server_socket e l'indirizzo del client client_address
*/
int thread_start(int server_socket, struct sockaddr_in client_address, node_t *pos_client) {
    ClientInfo *client_info;
    pthread_t tid;


    /* Creazione di una struttura ClientInfo per passare le informazioni del client al thread */
    client_info = (ClientInfo *)malloc(sizeof(ClientInfo));
    if (client_info == NULL) {
        printf("Errore[%d] malloc(sizeof(ClientInfo)): %s\n", errno, strerror(errno));


        return EXIT_ERROR;
    }


    client_info->server_socket = server_socket;
    client_info->client_address = client_address;
    client_info->pos_client = pos_client;


    /* Creazione di un nuovo thread per gestire il client */
    if (pthread_create(&tid, NULL, handler_client, (void *)client_info) < 0) {
        printf("Errore[%d] pthread_create(): %s\n", errno, strerror(errno));
        free(client_info);
            

        return EXIT_ERROR;
    }
        

    pthread_detach(tid);    // detach del thread per permettere la terminazione automatica
    
    
    return EXIT_SUCCESS; 
}


/*
    Implementa la logica per accettare una connessione da un client UDP
    Utilizza la socket server_socket e l'indirizzo del client client_address
    NOTA: questa funzione va chiamata senza aver effettuato una lock prima
*/
int accept_client(int server_socket, struct sockaddr_in client_address) {
    char buffer[MAX_BUFFER_SIZE];
    ssize_t bytes_received;


    /* Invio conferma della connessione */
    memset(buffer, 0, MAX_BUFFER_SIZE);
    snprintf(buffer, MAX_BUFFER_SIZE, "connected");
    if (send_msg(server_socket, buffer, strlen(buffer), (struct sockaddr *)&client_address) < 0) {
        printf("Errore[%d] send_msg(): %s\n", errno, strerror(errno));


        return EXIT_ERROR;
    }


    /* Ricezione risposta del server */
    memset(buffer, 0, MAX_BUFFER_SIZE);
    bytes_received = rcv_msg(server_socket, buffer, (struct sockaddr *)&client_address);
    if (bytes_received < 0) {
        printf("Errore[%d] rcv_msg(): %s\n", errno, strerror(errno));
        printf("CLIENT NON CONNESSO\n");


        return EXIT_ERROR;
    }


    /* Verifico il messaggio ricevuto */
    if (strcmp(buffer, "ack") == 0) { 
        printf("CLIENT CONNESSO\n");
        

    } else {
        printf("CLIENT NON CONNESSO\n");


        return EXIT_ERROR;
    }


    return EXIT_SUCCESS;
}


/*
    Implementa la logica per chiudere la connessione con il client
    Utilizza la socket server_socket e l'indirizzo del client client_address
    NOTA: questa funzione va chiamata senza aver effettuato una lock prima (la fa la funzione stessa al suo interno)
*/
int close_connection(int server_socket, struct sockaddr_in client_address) {
    char buffer[MAX_BUFFER_SIZE];
    socklen_t addr_len;


    memset(buffer, 0, MAX_BUFFER_SIZE);
    addr_len = sizeof(client_address);


    /* Blocco per implementare una logica di sincronizzazione sicura tra i threads */
    if (mutex_lock(&mutex_snd) < 0) {
        printf("Errore[%d] mutex_lock(): %s\n", errno, strerror(errno));
        

        return EXIT_ERROR;
    }


    /* Invio messaggio di fine connessione */
    memset(buffer, 0, MAX_BUFFER_SIZE);
    snprintf(buffer, MAX_BUFFER_SIZE, "close");
    if (sendto(server_socket, buffer, strlen(buffer), 0, (struct sockaddr *)&client_address, addr_len) < 0) {
        printf("Errore[%d] sendto(): %s\n", errno, strerror(errno));
        mutex_unlock(&mutex_snd);


        return EXIT_ERROR;
    }

    mutex_unlock(&mutex_snd);


    return EXIT_SUCCESS;
}


/*
    Implementa la logica per inviare la lista dei file disponibili al client
    Utilizza la socket server_socket e l'indirizzo del client client_address
    NOTA: questa funzione va chiamata senza aver effettuato una lock prima (la fa la funzione stessa al suo interno)
*/
int send_file_list(int server_socket, struct sockaddr_in client_address) {
    size_t entry_len;
    size_t total_len;
    size_t residual_buffer = MAX_BUFFER_SIZE;
    int extra_char = 1;                         // numero di caratteri extra tra un nome e l'altro (in questo caso solo '\n')
    int regular_files = 0;                      // flag per verificare la presenza di file regolari
    char *buffer, *temp_buffer;
    DIR *directory;
    struct dirent *entry;


    /* Verifico l'esistenza della cartella */
    directory = check_directory(PATH_FILE_FOLDER);
    if (directory == NULL) {
        printf("Errore[%d] check_directory(): %s\n",errno , strerror(errno));

       
        return EXIT_ERROR;
    }


    /* Alloco memoria */
    buffer = (char *)malloc(MAX_BUFFER_SIZE * sizeof(char));
    if (buffer == NULL) {
        printf("Errore[%d] malloc(): %s\n", errno, strerror(errno));


        return EXIT_ERROR;
    }


    memset(buffer, 0, MAX_BUFFER_SIZE);
    temp_buffer = buffer;


    /* Leggo il contenuto della cartella (creo lista file) */
    while ((entry = readdir(directory)) != NULL) {
        entry_len = strlen(entry->d_name);
        total_len = entry_len + extra_char;


        /* Considero solo i file regolari (non contano ad esempio i file "." e "..") */
        if (entry->d_type == DT_REG) {  

            /* Gestione del limite di memoria del buffer */
            if (total_len <= residual_buffer && residual_buffer > 0) {  
                snprintf(temp_buffer, MAX_BUFFER_SIZE, "%s\n", entry->d_name);
                residual_buffer -= total_len;
                temp_buffer += total_len;
                regular_files++;

            }
        }
    }    


    /* Se non trovo file la cartella è vuota */
    if (regular_files == 0) {
        snprintf(buffer, MAX_BUFFER_SIZE, "%s%s%s\n", BOLDRED, "NESSUN FILE PRESENTE", RESET); // notifica il client con questo messaggio


    }


    /* Invio della lista al client */
    if (send_msg(server_socket, buffer, strlen(buffer), (struct sockaddr *)&client_address) < 0) {
        printf("Errore[%d] send_msg(): %s\n", errno, strerror(errno));


        return EXIT_ERROR;
    }


    printf("\n%s%sLista file inviata al client:%s\n", BOLDBLACK, BG_MAGENTA, RESET);
    printf("%s%s%s\n", YELLOW, buffer, RESET);


    closedir(directory);
    free(buffer);


    return EXIT_SUCCESS;
}


/*
    Implementa la logica per inviare un file al client
    Utilizza la socket server_socket, l'indirizzo del client client_address e il nome del file filename
    NOTA: questa funzione va chiamata senza aver effettuato una lock prima (la fa la funzione stessa al suo interno)
*/
int send_file(int server_socket, struct sockaddr_in client_address, char* filename) {
    DIR *directory;
    FILE *file;
    size_t bytes_read;
    long file_size;
    long buffer_size;
    char *buffer;
    char full_path[MAX_BUFFER_SIZE];


    /* Verifico l'esistenza della cartella */
    directory = check_directory(PATH_FILE_FOLDER);
    if (directory == NULL) {
        printf("Errore[%d] check_directory(): %s\n", errno , strerror(errno));

       
        return EXIT_ERROR;
    }


    /* Compongo il percorso completo del file */
    snprintf(full_path, MAX_BUFFER_SIZE, "%s/%s", PATH_FILE_FOLDER, filename);


    /* Apro il file in modalità lettura binaria */
    file = fopen(full_path, "rb");
    if (file == NULL || filename[0] == '\0') {
        printf("Errore[%d] fopen(): %s\n", errno , strerror(errno));

        if (errno == ENOENT) {
            // Invio frammento vuoto
            printf("FILE INESISTENTE\n");
            if (send_msg(server_socket, NULL, 0, (struct sockaddr *)&client_address) < 0) {
                printf("Errore[%d] send_msg(): %s\n", errno, strerror(errno));
                return EXIT_ERROR;
            }

            return EXIT_ERROR;
        } else {

            return EXIT_ERROR;
        }
    }


    /* Ricavo le dimesioni del file */
    fseek(file, 0, SEEK_END);   // Posiziono il cursore alla fine del file
    file_size = ftell(file);    // Ottengo la posizione corrente del cursore (che è la dimensione del file)
    fseek(file, 0, SEEK_SET);   // Riporta il cursore all'inizio del file


    /* Assegno dimensione del buffer */
    if (file_size < SMALL_SIZE) {
        buffer_size = FRAGMENT_MICRO_SIZE;


    } else if (file_size < MEDIUM_SIZE) {
        buffer_size = FRAGMENT_SMALL_SIZE;


    } else if (file_size < LARGE_SIZE) {
        buffer_size = FRAGMENT_MEDIUM_SIZE;


    } else {
        buffer_size = FRAGMENT_LARGE_SIZE;


    }


    /* Alloco memoria */
    buffer = (char *)malloc(buffer_size);
    if (buffer == NULL) {
        printf("Errore[%d] malloc(): %s\n", errno, strerror(errno));
        //mutex_unlock(&mutex_snd);


        return EXIT_ERROR;
    }


    memset(buffer, 0, buffer_size);


    printf("DIMENSIONE FILE: %ld BYTE\n", file_size);
    printf("TAGLIA FRAMMENTI FILE: %ld BYTE\n", buffer_size);


    /* Invio dei pacchetti del file al client */
    while ((bytes_read = fread(buffer, 1, buffer_size, file)) > 0) {
        
        printf("%s<--- INIZIO INVIO FRAMMENTO --->%s\n", BLUE, RESET);
        if (send_msg(server_socket, buffer, bytes_read, (struct sockaddr *)&client_address) < 0) {
            printf("Errore[%d] send_msg(): %s\n", errno, strerror(errno));


            return EXIT_ERROR;
        }      
        printf("%s<--- FINE INVIO FRAMMENTO --->%s\n", BLUE, RESET);

    }


    /* Invio di un frammento vuoto come segnale di completamento */
    printf("%s<--- INIZIO INVIO FRAMMENTO VUOTO --->%s\n", GREEN, RESET);
    if (send_msg(server_socket, NULL, 0, (struct sockaddr *)&client_address) < 0) {
        printf("Errore[%d] send_msg(): %s\n", errno, strerror(errno));


        return EXIT_ERROR;
    }
    printf("%s<--- FINE INVIO FRAMMENTO VUOTO --->%s\n", GREEN, RESET);


    free(buffer);
    fclose(file);
    closedir(directory);
    printf("FILE INVIATO CON SUCCESSO\n");


    return EXIT_SUCCESS;
}


/*
    Implementa la logica per ricevere un file dal client
    Utilizza la socket server_socket, l'indirizzo del client client_address e il nome del file filename
    NOTA: questa funzione va chiamata senza aver effettuato una lock prima (la fa la funzione stessa al suo interno)
*/
int receive_file(int socket,  struct sockaddr_in address, char* filename) {
    DIR *directory;
    FILE *file;
    int bytes_received;
    int num_frammenti = 0;
    char buffer[MAX_BUFFER_FILE_SIZE];
    char full_path[MAX_BUFFER_SIZE]; 


    /* Verifico l'esistenza della cartella */
    directory = check_directory(PATH_FILE_FOLDER);
    if (directory == NULL) {
        printf("Errore[%d] check_directory(): %s\n",errno , strerror(errno));

       
        return EXIT_ERROR;
    }


    /* Compongo il percorso completo del file */
    snprintf(full_path, sizeof(full_path), "%s/%s", PATH_FILE_FOLDER, filename);


    /* Apertura del file in modalità scrittura binaria */
    file = fopen(full_path, "wb");
    if (file == NULL) {
        printf("Errore fopen(): %s\n", strerror(errno));


        return EXIT_ERROR;
    }


    while (1) {
        printf("%s<--- INIZIO RICEZIONE FRAMMENTO[%d] --->%s\n", BLUE, num_frammenti, RESET);
        
        memset(buffer, 0, MAX_BUFFER_FILE_SIZE);
        bytes_received = rcv_msg(socket, buffer, (struct sockaddr *)&address);
        if (bytes_received < 0) {
            printf("Errore rcv_msg(): %s\n", strerror(errno));


            return EXIT_ERROR;
        }

        printf("bytes_received: %d\n", bytes_received);
        printf("%s<--- FINE RICEZIONE FRAMMENTO[%d] --->%s\n", BLUE, num_frammenti, RESET);
        num_frammenti++;

        /* Ricezione completata */
        if (bytes_received == 0) {

            if (num_frammenti == 1) {   // se il primo frammento è vuoto è un segnale di errore

                return EXIT_ERROR;
            }

            break;
        }

        /* Scrivi i dati ricevuti nel file */
        fwrite(buffer, 1, bytes_received, file);
    }


    fclose(file);   // Chiudo il file
    closedir(directory);
    printf("FILE RICEVUTO CON SUCCESSO\n\n");


    return EXIT_SUCCESS;
}


/*
    Implementa la chiamta di sincronizzazione sicura tra i threads a pthread_mutex_lock()
*/
int mutex_lock(pthread_mutex_t *mutex) {
    int res;

    if(mutex == &mutex_rcv) return 0;

    do {
        errno = 0; // azzeramento di errno
        res = pthread_mutex_lock(mutex);
            
        if (res != 0 && errno != EINTR) {
            printf("Errore[%d] pthread_mutex_lock(): %s\n", errno, strerror(errno));
        

            return EXIT_ERROR;
        }
    } while (errno == EINTR);


    return EXIT_SUCCESS;
}


/*
    Implementa la chiamta di sincronizzazione sicura tra i threads a pthread_mutex_unlock()
*/
int mutex_unlock(pthread_mutex_t *mutex) {
    int res;

    do {
        errno = 0; // azzeramento di errno
        res = pthread_mutex_unlock(mutex);
            
        if (res != 0 && errno != EINTR) {
            printf("Errore[%d] pthread_mutex_unlock(): %s\n", errno, strerror(errno));
        

            return EXIT_ERROR;
        }
    } while (errno == EINTR);


    return EXIT_SUCCESS;
}


/*
    Implementa il Thread che si occupa di gestire il relativo client
*/
void *handler_client(void *arg) {
    ssize_t bytes_received;
    int server_socket;
    char buffer[MAX_BUFFER_SIZE];
    pthread_t tid;
    ClientInfo *client_info;
    struct sockaddr_in client_address;
    node_t *pos_client;


    tid = pthread_self();
    client_info = (ClientInfo *)arg;
    client_address = client_info->client_address;
    server_socket = client_info->server_socket;
    pos_client = client_info->pos_client;


    printf("THREAD[%s%ld%s] AVVIATO\n",BOLDGREEN, tid, RESET);


    /* Tentativo di connessione */
    if (accept_client(server_socket, client_address) < 0) {
        printf("Errore[%d] accept_client() [connessione rifiutata]: %s\n", errno, strerror(errno));


        close_connection(server_socket, client_address);
        free(client_info);


        thread_kill(tid, pos_client);
    }


    /* Ciclo infinito / Corpo principale del thread */
    while (1) {
        memset(buffer, 0, MAX_BUFFER_SIZE);

        bytes_received = rcv_msg(server_socket, buffer, (struct sockaddr *)&client_address);
        if (bytes_received < 0) {
            printf("Errore[%d] rcv_msg(): %s\n", errno, strerror(errno));
            close_connection(server_socket, client_address);    // chiudere la connessione
            free(client_info);

            thread_kill(tid, pos_client);
        }



        /* Stampa messaggi ricevuti */
        printf("%sthread[%ld]%s: dati da ", BOLDGREEN, tid, RESET);
        printf("%s%s%s:", CYAN, inet_ntoa(client_address.sin_addr), RESET);
        printf("%sUDP%u%s : ", MAGENTA, ntohs(client_address.sin_port), RESET);
        printf("%s%s%s\n", BOLDYELLOW, buffer, RESET);    
        


        /* Gestore dei comandi */
        if (strcmp(buffer, "list") == 0) {
            if (send_file_list(server_socket, client_address) < 0) {
                printf("Errore[%d] send_file_list(): %s\n", errno, strerror(errno));
                

                close_connection(server_socket, client_address);    // chiudere la connessione + messaggio errore
                free(client_info);
                
                
                thread_kill(tid, pos_client);
            }


        } else if (strncmp(buffer, "get ", 4) == 0) {
            char* filename = buffer + 4;

            /* Verifico se è presente il nome del file */
            if (strlen(buffer) == 4) {
                printf("NOME FILE MANCANTE\n");

                close_connection(server_socket, client_address);
                free(client_info);

                thread_kill(tid, pos_client);
            }


            /* Verifico se il nome fornito inizia con uno spazio */
            if (filename[0] == ' ') {
                printf("NOME NON VALIDO\n");
                
                
                close_connection(server_socket, client_address);
                free(client_info);

                thread_kill(tid, pos_client);
            }
            
            /* Invio il file */
            if (send_file(server_socket, client_address, filename) < 0) {
                printf("Errore[%d] send_file(): %s\n", errno, strerror(errno));
                printf("FILE NON INVIATO CORRETTAMENTE\n");


                close_connection(server_socket, client_address);
                free(client_info);


                thread_kill(tid, pos_client);
            }


        } else if (strncmp(buffer, "put ", 4) == 0) {
            char* filename = buffer + 4;

            /* Verifico se è presente il nome del file */
            if (strlen(buffer) == 4) {
                printf("NOME FILE MANCANTE\n");

                close_connection(server_socket, client_address);
                free(client_info);

                thread_kill(tid, pos_client);
            }


            /* Verifico se il nome fornito inizia con uno spazio */
            if (filename[0] == ' ') {
                printf("NOME NON VALIDO\n");
                
                
                close_connection(server_socket, client_address);
                free(client_info);

                thread_kill(tid, pos_client);
            }
            
            /* Ricevo il file */
            if (receive_file(server_socket, client_address, filename) < 0) {
                printf("Errore[%d] receive_file(): %s\n", errno, strerror(errno));
                printf("FILE NON RICEVUTO CORRETTAMENTE\n");


                close_connection(server_socket, client_address);
                free(client_info);


                thread_kill(tid, pos_client);
            }


        } else if (strcmp(buffer, "close") == 0) {


            thread_kill(tid, pos_client);
            free(client_info);

        } else {
            printf("%sComando non riconosciuto%s\n", RED, RESET);


        } 
    }
}


/*
    Funzione per incapsulare comandi frequenti
*/
void thread_kill(pthread_t tid, node_t *pos_client) {
    n_clt--;
    printf("CLIENT RIMOSSO DALLA LISTA: %s%ld client connessi%s\n", BOLDBLUE, n_clt, RESET);
    remove_node(client_list, pos_client);
    n_clt <= 30 ? print_list(client_list):0;


    printf("THREAD[%s%ld%s] TERMINATO\n", BOLDGREEN, tid, RESET);
    pthread_exit(NULL);   
}


/*
    Funzione di gestione del segnale SIGINT
*/
void handle_ctrl_c(int __attribute__((unused)) signum, siginfo_t __attribute__((unused)) *info, void __attribute__((unused)) *context) {
    struct sockaddr_in client_address;
    node_t *pos = tail(client_list);


    printf("\b\b%sSegnale Ctrl+C. %sExiting...%s\n", BOLDGREEN, BOLDYELLOW, RESET);
    

    /* Chiudo la connessione con tutti i client */
    while (is_empty(client_list) == false) {

        client_address = get_value(pos);
        close_connection(server_socket, client_address);

        n_clt--;
        printf("CLIENT RIMOSSO DALLA LISTA: %s%ld client connessi%s\n", BOLDBLUE, n_clt, RESET);
        pos = remove_node(client_list, pos);
        n_clt <= 30 ? print_list(client_list):0;

    }
    

    pthread_mutex_destroy(&mutex_rcv); // Distrugge il mutex


    if (close(server_socket) < 0) {
        printf("Errore close(): %s\n", strerror(errno)); 


        exit(EXIT_FAILURE);
    }

    printf("SERVER SPENTO CON SUCCESSO\n");
    exit(EXIT_SUCCESS);
}

