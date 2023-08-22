/*
    gcc server.c -o server -lpthread

    connessione a tre vie e timer alarm per il server

    assicurarsi di chi siano i dati in arrivo

    
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
#include <pthread.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>



#define MAX_BUFFER_SIZE 1024
#define DEFAULT_PORT 8888
#define PATH_FILE_FOLDER "file_folder_server"   // Path per la cartella preposta per i file
#define EXIT_ERROR -1


#define RESET     "\033[0m"
#define BOLD      "\033[1m"
#define DIM       "\033[2m"
#define UNDERLINE "\033[4m"
#define BLINK     "\033[5m"
#define REVERSE   "\033[7m"
#define HIDDEN    "\033[8m"

#define BLACK     "\033[30m"
#define RED       "\033[31m"
#define GREEN     "\033[32m"
#define YELLOW    "\033[33m"
#define BLUE      "\033[34m"
#define MAGENTA   "\033[35m"
#define CYAN      "\033[36m"
#define WHITE     "\033[37m"

#define BOLDBLACK     "\033[1m\033[30m"
#define BOLDRED       "\033[1m\033[31m"
#define BOLDGREEN     "\033[1m\033[32m"
#define BOLDYELLOW    "\033[1m\033[33m"
#define BOLDBLUE      "\033[1m\033[34m"
#define BOLDMAGENTA   "\033[1m\033[35m"
#define BOLDCYAN      "\033[1m\033[36m"
#define BOLDWHITE     "\033[1m\033[37m"

#define BG_BLACK   "\033[40m"
#define BG_RED     "\033[41m"
#define BG_GREEN   "\033[42m"
#define BG_YELLOW  "\033[43m"
#define BG_BLUE    "\033[44m"
#define BG_MAGENTA "\033[45m"
#define BG_CYAN    "\033[46m"
#define BG_WHITE   "\033[47m"


/* Variabili globali */
int server_socket;
struct sockaddr_in client_address;
pthread_mutex_t mutex;


/* Struttura per passare dati ai threads*/
typedef struct {
    int server_socket;
    struct sockaddr_in client_address;
    pthread_mutex_t *mutex_pointer;
} ClientInfo;


/* Dichiarazione delle funzioni */
DIR *check_directory(const char *__name);
int accept_client(int server_socket, struct sockaddr_in client_address, pthread_mutex_t *mutex_pointer);
int send_file_list(int server_socket, struct sockaddr_in client_address, pthread_mutex_t *mutex_pointer);
int send_file(int server_socket, struct sockaddr_in client_address, char* filename, pthread_mutex_t *mutex_pointer);
int receive_file(int server_socket, struct sockaddr_in client_address, char* filename, pthread_mutex_t *mutex_pointer);
void *handle_client(void *arg);
void handle_ctrl_c(int signum, siginfo_t *info, void *context);




int main(int argc, char *argv[]) {
    int addr_len;
    int bytes_received;
    struct sockaddr_in server_address;
    char buffer[MAX_BUFFER_SIZE];
    struct sigaction sa;
    

    /* Configurazione di sigaction */
    sa.sa_sigaction = handle_ctrl_c;    // funzione di gestione del segnale
    sa.sa_flags = SA_SIGINFO;           // abilita l'uso di siginfo_t


    /* Imposta l'handler per SIGINT (Ctrl+C) */
    if (sigaction(SIGINT, &sa, NULL) < 0) {
        printf("Errore[%d] sigaction(): %s\n", errno , strerror(errno));
        

        exit(EXIT_FAILURE);
    }
    

    /* Inizializzo il mutex */
    pthread_mutex_init(&mutex, NULL);


    /* Verifico l'esistenza della cartella preposta per contenere i file */
    if (check_directory(PATH_FILE_FOLDER) == NULL) {
        printf("Errore[%d] check_directory(): %s\n", errno , strerror(errno));

       
        exit(EXIT_FAILURE);
    }
    

    /* Creazione della socket UDP del server */
    server_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (server_socket < 0) {
        printf("Errore[%d] socket(): %s\n", errno, strerror(errno));
        

        exit(EXIT_FAILURE);
    }
    

    /* Configurazione dell'indirizzo del server UDP */
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(DEFAULT_PORT);
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);     // il server accetta richieste su ogni interfaccia di rete 
    

    /* Associazione dell'indirizzo del server alla socket */
    if (bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {   
        printf("Errore[%d] bind(): %s\n", errno, strerror(errno));
        

        exit(EXIT_FAILURE);
    }
    

    printf("%sServer in ascolto sulla porta %d...%s\n\n", BOLDGREEN, DEFAULT_PORT, RESET);
    

    while (1) {
        memset(buffer, 0, MAX_BUFFER_SIZE);
        addr_len = sizeof(client_address);


        /* Blocco il mutex prima di leggere dalla socket */
        pthread_mutex_lock(&mutex);
        

        /* Ricezione del comando(di connessione) dal client utilizzando MSG_PEEK per non consumare i dati */
        bytes_received = recvfrom(server_socket, buffer, MAX_BUFFER_SIZE, MSG_PEEK, (struct sockaddr *)&client_address, &addr_len);
        if (bytes_received < 0) {
            printf("Errore[%d] recvfrom(): %s\n", errno, strerror(errno));
            pthread_mutex_unlock(&mutex);


            continue;   // in caso di errore non terminiamo
        } 
        

        buffer[bytes_received] = '\0';  // imposto il terminatore di stringa


        /* Verifico che sia una richiesta di connessione */ 
        if (memcmp(buffer, "connect", strlen("connect")) != 0) {
            pthread_mutex_unlock(&mutex);


            continue;   // se non è una richiesta ignoro il messaggio
        }


        /* Consumazione effettiva dei dati dalla socket */
        bytes_received = recvfrom(server_socket, buffer, MAX_BUFFER_SIZE, 0, (struct sockaddr *)&client_address, &addr_len);
        if (bytes_received < 0) {
            printf("Errore[%d] recvfrom(): %s\n", errno, strerror(errno));
            pthread_mutex_unlock(&mutex);


            continue;   // in caso di errore non terminiamo
        } 


        /* Stampa messaggi ricevuti */
        printf("%s%s%s: dati da ", BOLDGREEN, argv[0], RESET);
        printf("%s%s%s:", CYAN, inet_ntoa(client_address.sin_addr), RESET);
        printf("%sUDP%u%s : ", MAGENTA, ntohs(client_address.sin_port), RESET);
        printf("%s%s%s\n", BOLDYELLOW, buffer, RESET);


        /* Gestore del comando */
        if (strcmp(buffer, "connect") == 0) {
            printf("TENTATIVO DI CONNESSIONE\n");

            if (accept_client(server_socket, client_address, &mutex) < 0) {
                printf("Errore[%d] accept_client() [connessione rifiutata]: %s\n", errno, strerror(errno));
                pthread_mutex_unlock(&mutex);
                

                continue;   // in caso di errore non terminiamo
            } 

        } else {
            printf("%sComando non riconosciuto%s\n\n", RED, RESET);


        }


        /* Sblocco il mutex dopo aver letto dalla socket ed accettato la conessione*/
        pthread_mutex_unlock(&mutex);
    }

    

    return EXIT_SUCCESS;
}




/* 
    Verifica che la cartella sia presente in caso contrario la creo
*/
DIR *check_directory(const char *__name) {
    DIR *directory;


RETRY:
    directory = opendir(PATH_FILE_FOLDER);


    if (directory == NULL) {
        printf("Errore[%d] opendir(): %s\n",errno , strerror(errno));

        /* se la cartella non esisteva la creo */
        if (errno == ENOENT) {
            if (mkdir(PATH_FILE_FOLDER, 0755) == 0) {   // permessi numerazione ottale |7|7|5|
                printf("Cartella creata con successo\n\n");
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
    Implementa la logica per accettare una connessione da un client UDP
    Utilizza la socket server_socket e l'indirizzo del client client_address
*/
int accept_client(int server_socket, struct sockaddr_in client_address, pthread_mutex_t *mutex_pointer) {
    int addr_len;
    int bytes_received;
    ClientInfo *client_info;
    struct sockaddr_in client_address_expected = client_address;
    pthread_t tid;
    time_t start_time;
    time_t current_time;
    time_t elapsed_time;
    int max_duration = 5;               // durata massima in secondi del timer
    char buffer[MAX_BUFFER_SIZE];


    memset(buffer, 0, MAX_BUFFER_SIZE);
    addr_len = sizeof(client_address);


    /* Invio conferma della connessione */
    snprintf(buffer, MAX_BUFFER_SIZE, "connected");
    if (sendto(server_socket, buffer, strlen(buffer), 0, (struct sockaddr *)&client_address, sizeof(client_address)) < 0) {
        printf("Errore[%d] sendto(): %s\n", errno, strerror(errno));


        return EXIT_ERROR;
    }


    start_time = time(NULL);     // tempo di inizio del timer

    while (1) {
        memset(buffer, 0, MAX_BUFFER_SIZE);


        /* Calcolo il tempo trascorso */
        time_t current_time = time(NULL);
        elapsed_time = current_time - start_time;


        /* Esco dal ciclo se sono passati più di 5 secondi */
        if (elapsed_time >= max_duration) {
            printf("CONNESSIONE NON RIUSCITA, TIMER SCADUTO\n");


            break;
        }


        /* Ricezione messaggio di ACK per connessione a tre vie senza consumare i dati*/
        bytes_received = recvfrom(server_socket, buffer, MAX_BUFFER_SIZE, MSG_PEEK, (struct sockaddr *)&client_address, &addr_len);
        if (bytes_received < 0) {
            printf("Errore[%d] recvfrom(): %s\n", errno, strerror(errno));


            return EXIT_ERROR;
        }


        /* Verifico se l'indirizzo IP e la porta del mittente non corrispondono a quelli previsti */
        if (memcmp(&client_address, &client_address_expected, addr_len) != 0) {


            continue; // in caso non corrispondano ignoro il messaggio e riteno fino allo scadere del timer
        }


        /* Consumazione effettiva dei dati dalla socket */
        bytes_received = recvfrom(server_socket, buffer, MAX_BUFFER_SIZE, 0, (struct sockaddr *)&client_address, &addr_len);
        if (bytes_received < 0) {
            printf("Errore[%d] recvfrom(): %s\n", errno, strerror(errno));


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


        return EXIT_ERROR;
    }


    /* Creazione di una struttura ClientInfo per passare le informazioni del client al thread */
    client_info = (ClientInfo *)malloc(sizeof(ClientInfo));
    if (client_info == NULL) {
        printf("Errore[%d] malloc(sizeof(ClientInfo)): %s\n", errno, strerror(errno));


        return EXIT_ERROR;
    }


    client_info->server_socket = server_socket;
    client_info->client_address = client_address;
    client_info->mutex_pointer = mutex_pointer;


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
    Implementa la logica per inviare la lista dei file disponibili al client
    Utilizza la socket server_socket e l'indirizzo del client client_address
*/
int send_file_list(int server_socket, struct sockaddr_in client_address, pthread_mutex_t *mutex_pointer) {
    struct dirent *entry;
    DIR *directory;
    char *buffer, *temp_buffer;
    int residual_buffer = MAX_BUFFER_SIZE;
    int entry_len, total_len;
    int extra_char = 1;     // numero di caratteri extra tra un nome e l'altro (in questo caso solo '\n')
    int regular_files = 0;  // flag per verificare la presenza di file regolari


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


    /* Leggo il contenuto della cartella (creo lista file) */
    temp_buffer = buffer;
    while ((entry = readdir(directory)) != NULL) {
        entry_len = strlen(entry->d_name);
        total_len = entry_len+extra_char;


        /* Considera solo i file regolari (non conta ad esempio i file "." e "..") */
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
        snprintf(buffer, MAX_BUFFER_SIZE, "%s%s%s\n", BOLDRED, "NESSUN FILE PRESENTE", RESET);


    }


    /* Invio della lista al client */
    if (sendto(server_socket, buffer, strlen(buffer), 0, (struct sockaddr *)&client_address, sizeof(client_address)) < 0) {
        printf("Errore[%d] sendto(): %s\n", errno, strerror(errno));


        return EXIT_ERROR;
    }


    printf("\n%s%sLista file inviata al client:%s\n", BOLDBLACK, BG_MAGENTA, RESET);
    printf("%s%s%s\n", GREEN, buffer, RESET);


    closedir(directory);
    free(buffer);


    return EXIT_SUCCESS;
}


/*
    Implementa la logica per inviare un file al client
    Utilizza la socket server_socket, l'indirizzo del client client_address e il nome del file filename
*/
int send_file(int server_socket, struct sockaddr_in client_address, char* filename, pthread_mutex_t *mutex_pointer) {
    DIR *directory;
    FILE *file;
    size_t bytes_read;
    char buffer[MAX_BUFFER_SIZE];


    /* Verifico l'esistenza della cartella */
    directory = check_directory(PATH_FILE_FOLDER);
    if (directory == NULL) {
        printf("Errore[%d] check_directory(): %s\n", errno , strerror(errno));

       
        return EXIT_ERROR;
    }


    /* Compongo il percorso completo del file */
    char full_path[MAX_BUFFER_SIZE]; 
    snprintf(full_path, sizeof(full_path), "%s/%s", PATH_FILE_FOLDER, filename);


    /* Apro il file in modalità lettura binaria */
    file = fopen(full_path, "rb");
    if (file == NULL) {
        printf("Errore[%d] fopen(): %s\n", errno , strerror(errno));


        return EXIT_ERROR;
    }


    memset(buffer, 0, MAX_BUFFER_SIZE);


    /* Invio dei pacchetti del file al client */
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        
        if (sendto(server_socket, buffer, bytes_read, 0, (struct sockaddr *)&client_address, sizeof(client_address)) < 0) {
            printf("Errore[%d] sendto(): %s\n", errno, strerror(errno));


            return EXIT_ERROR;
        }
    }


    /* Invio di un pacchetto vuoto come segnale di completamento */
    if (sendto(server_socket, NULL, 0, 0, (struct sockaddr *)&client_address, sizeof(client_address)) < 0) {
        printf("Errore[%d] sendto(): %s\n", errno, strerror(errno));


        return EXIT_ERROR;
    }


    fclose(file);
    closedir(directory);
    printf("FILE INVIATO CON SUCCESSO\n\n");


    return EXIT_SUCCESS;
}


/*
    Implementa la logica per ricevere un file dal client
    Utilizza la socket server_socket, l'indirizzo del client client_address e il nome del file filename
*/
int receive_file(int server_socket, struct sockaddr_in client_address, char* filename, pthread_mutex_t *mutex_pointer) {
    DIR *directory;
    FILE *file;
    int bytes_received;
    char buffer[MAX_BUFFER_SIZE];
    char full_path[MAX_BUFFER_SIZE];


    /* Verifico l'esistenza della cartella */
    directory = check_directory(PATH_FILE_FOLDER);
    if (directory == NULL) {
        printf("Errore[%d] check_directory(): %s\n",errno , strerror(errno));

       
        return EXIT_ERROR;
    }


    /* Compongo il percorso completo del file */
    memset(full_path, 0, MAX_BUFFER_SIZE);
    snprintf(full_path, sizeof(full_path), "%s/%s", PATH_FILE_FOLDER, filename);


    /* Apertura del file in modalità scrittura binaria */
    file = fopen(full_path, "wb");
    if (file == NULL) {
        printf("Errore fopen(): %s\n", strerror(errno));


        return EXIT_ERROR;
    }


    memset(buffer, 0, MAX_BUFFER_SIZE);


    while (1) {
        bytes_received = recvfrom(server_socket, buffer, sizeof(buffer), 0, NULL, NULL);
        if (bytes_received < 0) {
            printf("Errore recvfrom(): %s\n", strerror(errno));


            return EXIT_ERROR;
        }


        /* Ricezione completata */
        if (bytes_received == 0) {


            break;
        }


        /* Scrivo i dati ricevuti nel file */
        fwrite(buffer, 1, bytes_received, file);
    }


    fclose(file);   // Chiudo il file
    closedir(directory);
    printf("FILE RICEVUTO CON SUCCESSO\n\n");


    return EXIT_SUCCESS;
}


/*
    Implementa il Thread che si occupa di gestire il relativo client
*/
void *handle_client(void *arg) {
    ClientInfo *client_info = (ClientInfo *)arg;
    struct sockaddr_in client_address;
    struct sockaddr_in client_address_expected = client_info->client_address;
    char buffer[MAX_BUFFER_SIZE];
    int addr_len;
    int bytes_received;
    int server_socket = client_info->server_socket;
    pthread_t tid = pthread_self();
    pthread_mutex_t *mutex_pointer = client_info->mutex_pointer;
        

    printf("THREAD[%ld] AVVIATO\n", tid);


    while (1) {
        memset(buffer, 0, MAX_BUFFER_SIZE);
        addr_len = sizeof(client_address);


        /* Blocco il mutex prima di leggere dalla socket */
        pthread_mutex_lock(mutex_pointer);


        /* Ricezione del comando dal client utilizzando MSG_PEEK per non consumare i dati */
        bytes_received = recvfrom(server_socket, buffer, MAX_BUFFER_SIZE, MSG_PEEK, (struct sockaddr *)&client_address, &addr_len);
        if (bytes_received < 0) {
            printf("Errore[%d] recvfrom(): %s\n", errno, strerror(errno));


            pthread_exit(NULL);
        }


        /* Verifico se l'indirizzo IP e la porta del mittente non corrispondono a quelli previsti */
        if (memcmp(&client_address, &client_address_expected, addr_len) != 0) {
            pthread_mutex_unlock(mutex_pointer);


            continue; // in caso non corrispondano ignoro il messaggio
        }


        /* Consumazione effettiva dei dati dalla socket */
        bytes_received = recvfrom(server_socket, buffer, MAX_BUFFER_SIZE, 0, (struct sockaddr *)&client_address, &addr_len);
        if (bytes_received < 0) {
            printf("Errore[%d] recvfrom(): %s\n", errno, strerror(errno));
            pthread_mutex_unlock(mutex_pointer);


            pthread_exit(NULL);
        }


        buffer[bytes_received] = '\0';  // imposto il terminatore di stringa


        /* Stampa messaggi ricevuti */
        printf("%sthread[%ld]%s: dati da ", BOLDGREEN, tid, RESET);
        printf("%s%s%s:", CYAN, inet_ntoa(client_address.sin_addr), RESET);
        printf("%sUDP%u%s : ", MAGENTA, ntohs(client_address.sin_port), RESET);
        printf("%s%s%s\n", BOLDYELLOW, buffer, RESET);    
        

        /* Gestore dei comandi */
        if (strcmp(buffer, "list") == 0) {
            if (send_file_list(server_socket, client_address, &mutex) < 0) {
                free(client_info);

                // chiudere la connessione + messaggio errore


                pthread_exit(NULL);
            }


        } else if (strncmp(buffer, "get ", 4) == 0) {
            char* filename = buffer + 4;
            if (send_file(server_socket, client_address, filename, &mutex) < 0) {
                free(client_info);

                // chiudere la connessione + messaggio errore


                pthread_exit(NULL);
            }


        } else if (strncmp(buffer, "put ", 4) == 0) {
            char* filename = buffer + 4;
            if (receive_file(server_socket, client_address, filename, &mutex) < 0) {
                free(client_info);

                // chiudere la connessione + messaggio errore


                pthread_exit(NULL);
            }


        } else if (strcmp(buffer, "close") == 0) {
            printf("THREAD[%ld] TERMINATO\n", tid);
            free(client_info);


            pthread_exit(NULL);

        } else {
            printf("%sComando non riconosciuto%s\n\n", RED, RESET);


        } 


        /* Sblocco il mutex dopo aver letto dalla socket */
        pthread_mutex_unlock(mutex_pointer);   
    }
}


/*
    Funzione di gestione del segnale SIGINT
*/
void handle_ctrl_c(int signum, siginfo_t *info, void *context) {
    printf("\b\b%sSegnale Ctrl+C. %sExiting...%s\n", BOLDGREEN, BOLDYELLOW, RESET);
    
    
    // invia segnale a tutti i client (serve lista client)

    pthread_mutex_destroy(&mutex); // Distrugge il mutex

    if (close(server_socket) < 0) {
        printf("Errore close(): %s\n", strerror(errno)); 


        exit(EXIT_FAILURE);
    }

    exit(EXIT_SUCCESS);
}

