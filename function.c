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
    if (pthread_create(&tid, NULL, handle_client, (void *)client_info) < 0) {
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
    NOTA: questa funzione va chiamata senza aver effettuato una lock prima (la fa la funzione stessa al suo interno)
*/
int accept_client(int server_socket, struct sockaddr_in client_address) {
    char buffer[MAX_BUFFER_SIZE];
    struct sockaddr_in client_address_expected;
    socklen_t addr_len;
    ssize_t bytes_received;
    time_t start_time;
    time_t current_time;
    time_t elapsed_time;
    time_t max_duration = 15;               // durata massima in secondi del timer


    memset(buffer, 0, MAX_BUFFER_SIZE);
    addr_len = sizeof(client_address);
    client_address_expected = client_address;


    /* Blocco per implementare una logica di sincronizzazione sicura tra i threads */
    if (mutex_lock(&mutex_snd) < 0) {
        printf("Errore[%d] mutex_lock(): %s\n", errno, strerror(errno));
        

        return EXIT_ERROR;
    }


    /* Invio conferma della connessione */
    snprintf(buffer, MAX_BUFFER_SIZE, "connected");
    if (sendto(server_socket, buffer, strlen(buffer), 0, (struct sockaddr *)&client_address, sizeof(client_address)) < 0) {
        printf("Errore[%d] sendto(): %s\n", errno, strerror(errno));
        mutex_unlock(&mutex_snd);


        return EXIT_ERROR;
    }


    mutex_unlock(&mutex_snd);
    start_time = time(NULL);     // tempo di inizio del timer


    while (1) {
        memset(buffer, 0, MAX_BUFFER_SIZE);


        /* Blocco per implementare una logica di sincronizzazione sicura tra i threads */
        if (mutex_lock(&mutex_rcv) < 0) {
            printf("Errore[%d] mutex_lock(): %s\n", errno, strerror(errno));
        

            return EXIT_ERROR;
        }


        /* Calcolo il tempo trascorso */
        current_time = time(NULL);
        elapsed_time = current_time - start_time;


        /* Esco dal ciclo se sono passati più di 15 secondi */
        if (elapsed_time >= max_duration) {
            printf("CONNESSIONE NON RIUSCITA, TIMER SCADUTO\n");


            break;
        }


        /* Ricezione messaggio di ACK per connessione a tre vie senza consumare i dati */
        bytes_received = recvfrom(server_socket, buffer, MAX_BUFFER_SIZE, MSG_PEEK, (struct sockaddr *)&client_address, &addr_len);
        if (bytes_received < 0) {
            printf("Errore[%d] recvfrom(): %s\n", errno, strerror(errno));
            mutex_unlock(&mutex_rcv);


            return EXIT_ERROR;
        }


        /* Verifico se l'indirizzo IP e la porta del mittente non corrispondono a quelli previsti */
        if (memcmp(&client_address, &client_address_expected, addr_len) != 0) {
            mutex_unlock(&mutex_rcv); // in questo modo il vero destinatario ha la possibilità di consumare i dati


            continue; // in caso non corrispondano ignoro il messaggio e ritento fino allo scadere del timer
        }


        /* Consumazione effettiva dei dati dalla socket */
        bytes_received = recvfrom(server_socket, buffer, MAX_BUFFER_SIZE, 0, (struct sockaddr *)&client_address, &addr_len);
        if (bytes_received < 0) {
            printf("Errore[%d] recvfrom(): %s\n", errno, strerror(errno));
            mutex_unlock(&mutex_rcv);


            return EXIT_ERROR;
        }


        buffer[bytes_received] = '\0';  // imposto il terminatore di stringa
        break; // se sono arrivato qui esco dal ciclo
    }


    /* Verifico il messaggio ricevuto */
    if (strcmp(buffer, "ack") == 0) { 
        printf("CLIENT CONNESSO\n");
        

    } else {
        printf("CLIENT NON CONNESSO\n");
        mutex_unlock(&mutex_rcv); 


        return EXIT_ERROR;
    }


    /* Sblocco per implementare una logica di sincronizzazione sicura tra i threads */
    mutex_unlock(&mutex_rcv); 


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


    /* Blocco per implementare una logica di sincronizzazione sicura tra i threads */
    if (mutex_lock(&mutex_snd) < 0) {
        printf("Errore[%d] mutex_lock(): %s\n", errno, strerror(errno));
        

        return EXIT_ERROR;
    }


    /* Invio della lista al client */
    if (sendto(server_socket, buffer, strlen(buffer), 0, (struct sockaddr *)&client_address, sizeof(client_address)) < 0) {
        printf("Errore[%d] sendto(): %s\n", errno, strerror(errno));
        mutex_unlock(&mutex_snd);


        return EXIT_ERROR;
    }


    printf("\n%s%sLista file inviata al client:%s\n", BOLDBLACK, BG_MAGENTA, RESET);
    printf("%s%s%s\n", YELLOW, buffer, RESET);


    mutex_unlock(&mutex_snd);
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


    /* Blocco per implementare una logica di sincronizzazione sicura tra i threads */
    if (mutex_lock(&mutex_snd) < 0) {
        printf("Errore[%d] mutex_lock(): %s\n", errno, strerror(errno));
        

        return EXIT_ERROR;
    }    


    /* Verifico l'esistenza della cartella */
    directory = check_directory(PATH_FILE_FOLDER);
    if (directory == NULL) {
        printf("Errore[%d] check_directory(): %s\n", errno , strerror(errno));
        mutex_unlock(&mutex_snd);

       
        return EXIT_ERROR;
    }


    /* Compongo il percorso completo del file */
    snprintf(full_path, MAX_BUFFER_SIZE, "%s/%s", PATH_FILE_FOLDER, filename);


    /* Apro il file in modalità lettura binaria */
    file = fopen(full_path, "rb");
    if (file == NULL) {
        printf("Errore[%d] fopen(): %s\n", errno , strerror(errno));

        if (errno == ENOENT) {
            // inviare comando di file mancante
            printf("FILE INESISTENTE\n");
            mutex_unlock(&mutex_snd);

            return EXIT_ERROR;
        } else {

            mutex_unlock(&mutex_snd);
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
    buffer = (char *)malloc(buffer_size * sizeof(char));
    if (buffer == NULL) {
        printf("Errore[%d] malloc(): %s\n", errno, strerror(errno));
        mutex_unlock(&mutex_snd);


        return EXIT_ERROR;
    }


    memset(buffer, 0, buffer_size);


    printf("DIMENSIONE FILE: %ld BYTE\n", file_size);
    printf("TAGLIA FRAMMENTI FILE: %ld BYTE\n", buffer_size);


    /* Invio dei pacchetti del file al client */
    while ((bytes_read = fread(buffer, 1, buffer_size, file)) > 0) {
        
        if (sendto(server_socket, buffer, bytes_read, 0, (struct sockaddr *)&client_address, sizeof(client_address)) < 0) {
            printf("Errore[%d] sendto(): %s\n", errno, strerror(errno));
            mutex_unlock(&mutex_snd);


            return EXIT_ERROR;
        }
    }


    /* Invio di un pacchetto vuoto come segnale di completamento */
    if (sendto(server_socket, NULL, 0, 0, (struct sockaddr *)&client_address, sizeof(client_address)) < 0) {
        printf("Errore[%d] sendto(): %s\n", errno, strerror(errno));
        mutex_unlock(&mutex_snd);


        return EXIT_ERROR;
    }


    free(buffer);
    fclose(file);
    closedir(directory);
    mutex_unlock(&mutex_snd);
    printf("FILE INVIATO CON SUCCESSO\n");


    return EXIT_SUCCESS;
}


/*
    Implementa la logica per ricevere un file dal client
    Utilizza la socket server_socket, l'indirizzo del client client_address e il nome del file filename
    NOTA: questa funzione va chiamata senza aver effettuato una lock prima (la fa la funzione stessa al suo interno)
*/
int receive_file(int server_socket, struct sockaddr_in client_address, char* filename) {
    socklen_t addr_len;
    ssize_t bytes_received;
    char buffer[MAX_BUFFER_SIZE];
    char full_path[MAX_BUFFER_SIZE];
    FILE *file;
    DIR *directory;
    struct sockaddr_in client_address_expected = client_address;


    /* Blocco per implementare una logica thread safe */
    if (mutex_lock(&mutex_rcv) < 0) {
        printf("Errore[%d] mutex_lock(): %s\n", errno, strerror(errno));
        mutex_unlock(&mutex_rcv); 


        return EXIT_ERROR;
    }


    /* Verifico l'esistenza della cartella */
    directory = check_directory(PATH_FILE_FOLDER);
    if (directory == NULL) {
        printf("Errore[%d] check_directory(): %s\n",errno , strerror(errno));
         mutex_unlock(&mutex_rcv); 

       
        return EXIT_ERROR;
    }


    /* Compongo il percorso completo del file */
    memset(full_path, 0, MAX_BUFFER_SIZE);
    snprintf(full_path, sizeof(full_path), "%s/%s", PATH_FILE_FOLDER, filename);


    /* Apertura del file in modalità scrittura binaria */
    file = fopen(full_path, "wb");
    if (file == NULL) {
        printf("Errore fopen(): %s\n", strerror(errno));
        mutex_unlock(&mutex_rcv); 


        return EXIT_ERROR;
    }


    /* Sblocco per implementare una logica di sincronizzazione sicura tra i threads */
    mutex_unlock(&mutex_rcv); 


    while (1) {
        memset(buffer, 0, MAX_BUFFER_SIZE);


        /* Blocco per implementare una logica thread safe*/
        if (mutex_lock(&mutex_rcv) < 0) {
            printf("Errore[%d] mutex_lock(): %s\n", errno, strerror(errno));
        

            return EXIT_ERROR;
        }
        


        /* Ricezione messaggio senza consumare i dati */
        bytes_received = recvfrom(server_socket, buffer, MAX_BUFFER_SIZE, MSG_PEEK, (struct sockaddr *)&client_address, &addr_len);
        if (bytes_received < 0) {
            printf("Errore[%d] recvfrom(): %s\n", errno, strerror(errno));


            return EXIT_ERROR;
        }


        /* Verifico se l'indirizzo IP e la porta del mittente non corrispondono a quelli previsti */
        if (memcmp(&client_address, &client_address_expected, addr_len) != 0) {
            mutex_unlock(&mutex_rcv); // in questo modo il vero destinatario ha la possibilità di consumare i dati


            continue; // in caso non corrispondano ignoro il messaggio e riprovo
        }


        /* Consumazione effettiva dei dati dalla socket */
        bytes_received = recvfrom(server_socket, buffer, MAX_BUFFER_SIZE, 0, (struct sockaddr *)&client_address, &addr_len);
        if (bytes_received < 0) {
            printf("Errore[%d] recvfrom(): %s\n", errno, strerror(errno));


            return EXIT_ERROR;
        }


        /* Ricezione completata */
        if (bytes_received == 0) {


            break;
        }


        /* Scrivo i dati ricevuti nel file */
        fwrite(buffer, 1, bytes_received, file);


        /* Sblocco per implementare una logica di sincronizzazione sicura tra i threads */
        mutex_unlock(&mutex_rcv);
    }


    fclose(file);   // Chiudo il file
    closedir(directory);
    printf("FILE RICEVUTO CON SUCCESSO\n");
    mutex_unlock(&mutex_rcv);


    return EXIT_SUCCESS;
}


/*
    Implementa la chiamta di sincronizzazione sicura tra i threads a pthread_mutex_lock()
*/
int mutex_lock(pthread_mutex_t *mutex) {
    int res;


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
void *handle_client(void *arg) {
    socklen_t addr_len;
    ssize_t bytes_received;
    int server_socket;
    char buffer[MAX_BUFFER_SIZE];
    pthread_t tid;
    ClientInfo *client_info;
    struct sockaddr_in client_address_recived;
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
        addr_len = sizeof(client_address);


        /* Blocco il mutex prima di leggere dalla socket */
        if (mutex_lock(&mutex_rcv) < 0) {
            printf("Errore[%d] mutex_lock(): %s\n", errno, strerror(errno));
            

            close_connection(server_socket, client_address);    // chiudere la connessione + messaggio errore
            free(client_info);


            thread_kill(tid, pos_client);
        }
        

        /* Ricezione del comando dal client utilizzando MSG_PEEK per non consumare i dati */
        bytes_received = recvfrom(server_socket, buffer, MAX_BUFFER_SIZE, MSG_PEEK, (struct sockaddr *)&client_address_recived, &addr_len);
        if (bytes_received < 0) {
            printf("Errore[%d] recvfrom(): %s\n", errno, strerror(errno));
            
            
            close_connection(server_socket, client_address);    // chiudere la connessione + messaggio errore
            mutex_unlock(&mutex_rcv);
            free(client_info);


           thread_kill(tid, pos_client);
        }


        /* Verifico se l'indirizzo IP e la porta del mittente non corrispondono a quelli previsti */
        if (memcmp(&client_address_recived, &client_address, addr_len) != 0) {
            mutex_unlock(&mutex_rcv); // rilascio il mutex in modo che il messaggio venga consumato da un altro thread


            continue; // in caso non corrispondano ignoro il messaggio
        }


        /* Consumazione effettiva dei dati dalla socket */
        bytes_received = recvfrom(server_socket, buffer, MAX_BUFFER_SIZE, 0, (struct sockaddr *)&client_address_recived, &addr_len);
        if (bytes_received < 0) {
            printf("Errore[%d] recvfrom(): %s\n", errno, strerror(errno));

            close_connection(server_socket, client_address);    // chiudere la connessione + messaggio errore
            mutex_unlock(&mutex_rcv);
            free(client_info);


            thread_kill(tid, pos_client);
        }


        buffer[bytes_received] = '\0';  // imposto il terminatore di stringa


        /* Stampa messaggi ricevuti */
        printf("%sthread[%ld]%s: dati da ", BOLDGREEN, tid, RESET);
        printf("%s%s%s:", CYAN, inet_ntoa(client_address.sin_addr), RESET);
        printf("%sUDP%u%s : ", MAGENTA, ntohs(client_address.sin_port), RESET);
        printf("%s%s%s\n", BOLDYELLOW, buffer, RESET);    
        

        /* Sblocco il mutex dopo aver letto dalla socket */
        mutex_unlock(&mutex_rcv);   


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
            
            if (send_file(server_socket, client_address, filename) < 0) {
                printf("Errore[%d] send_file(): %s\n", errno, strerror(errno));


                close_connection(server_socket, client_address);
                free(client_info);


                thread_kill(tid, pos_client);
            }


        } else if (strncmp(buffer, "put ", 4) == 0) {
            char* filename = buffer + 4;
            if (receive_file(server_socket, client_address, filename) < 0) {
                printf("Errore[%d] receive_file(): %s\n", errno, strerror(errno));


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

printf("pos: %p\n", pos_client);
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

