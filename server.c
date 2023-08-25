/*
    gcc server.c -o server -lpthread


    prima o poi passare tutto in una libreria


    compattare il codice in sottofunzioni


    in caso errore chiudere la connessione con il client in questione close verso il client


    vedere commenti di cose da fare nelle funzioni handler
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
#include "singly_linked_list_opt.h"



#define MAX_BUFFER_SIZE 1024


#define SMALL_SIZE  1024        // 1kB valore oltre il quale si considera un file "small" prima di questo valore lo si considera "micro"
#define MEDIUM_SIZE 10240       // 10kB valore oltre il quale si considera un file "medium"
#define LARGE_SIZE  1048576     // 1MB valore oltre il quale si considera un file "large"


#define FRAGMENT_MICRO_SIZE     512     // 512 byte dimensione dei frammenti del dile passati al livello di trasporto
#define FRAGMENT_SMALL_SIZE     1024    // 1kB dimensione dei frammenti del dile passati al livello di trasporto
#define FRAGMENT_MEDIUM_SIZE    8192    // 8kB dimensione dei frammenti del dile passati al livello di trasporto
#define FRAGMENT_LARGE_SIZE     65536   // 64kB dimensione dei frammenti del dile passati al livello di trasporto


#define DEFAULT_PORT 8888
#define PATH_FILE_FOLDER "file_folder_server"   // path per la cartella preposta per contenere i file
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
list_t client_list;
node_t *pos_client = NULL;


/* Struttura per passare dati ai threads*/
typedef struct {
    int server_socket;
    struct sockaddr_in client_address;
    pthread_mutex_t *mutex_pointer;
    node_t *pos_client;
} ClientInfo;


/* Dichiarazione delle funzioni */
DIR *check_directory(const char *path_name);
int accept_client(int server_socket, struct sockaddr_in client_address, pthread_mutex_t *mutex_pointer);
int send_file_list(int server_socket, struct sockaddr_in client_address);
int send_file(int server_socket, struct sockaddr_in client_address, char* filename);
int receive_file(int server_socket, struct sockaddr_in client_address, char* filename, pthread_mutex_t *mutex_pointer);
int mutex_lock(pthread_mutex_t *mutex);
int mutex_unlock(pthread_mutex_t *mutex);
void *handle_client(void *arg);
void handle_ctrl_c(int signum, siginfo_t *info, void *context);





int main(int __attribute__((unused)) argc, char *argv[]) {
    socklen_t addr_len;
    ssize_t bytes_received;
    char buffer[MAX_BUFFER_SIZE];
    struct sockaddr_in server_address;
    struct sigaction sa;
    bool check_list;        // per tenere traccia se un client è nella lista dei client connessi
    bool check_connect_msg; // per tenere traccia se il messagio ricevuto è una connect


    /* Inizializzo la lista dei client */
    client_list = (list_t)create_list();
    if (client_list == NULL) {
        printf("Errore[%d] create_list(): %s\n", errno , strerror(errno));
        

        exit(EXIT_FAILURE);
    }


    /* Configurazione di sigaction */
    sa.sa_sigaction = handle_ctrl_c;    // funzione di gestione del segnale
    sa.sa_flags = SA_SIGINFO;           // abilita l'uso di siginfo_t


    /* Imposta l'handler per SIGINT (Ctrl+C) */
    if (sigaction(SIGINT, &sa, NULL) < 0) {
        printf("Errore[%d] sigaction(): %s\n", errno , strerror(errno));
        

        exit(EXIT_FAILURE);
    }
    

    /* Inizializzo il mutex */
    if (pthread_mutex_init(&mutex, NULL) != 0) {
        printf("Errore[%d] pthread_mutex_init(): %s\n", errno , strerror(errno));

       
        exit(EXIT_FAILURE);
    }


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


        printf("SERVER IN ESECUZIONE\n");


        /* Blocco il mutex prima di leggere dalla socket */
        if (mutex_lock(&mutex) < 0) {
            printf("Errore[%d] pthread_mutex(): %s\n", errno, strerror(errno));
        

            exit(EXIT_FAILURE);
        }
        

        /* Ricezione del comando(di connessione) dal client utilizzando MSG_PEEK per non consumare i dati */
        bytes_received = recvfrom(server_socket, buffer, MAX_BUFFER_SIZE, MSG_PEEK, (struct sockaddr *)&client_address, &addr_len);
        if (bytes_received < 0) {
            printf("Errore[%d] recvfrom(): %s\n", errno, strerror(errno));
            mutex_unlock(&mutex);


            continue;   // in caso di errore non terminiamo ma torniamo ad scoltare
        } 
        

        buffer[bytes_received] = '\0';  // imposto il terminatore di stringa


        /* Verifico che il client sia o no nella lista dei client connessi */
        check_list = is_in_list(client_list, client_address);


        /* Verifico che sia o no una richiesta di connessione */
        check_connect_msg = ( memcmp(buffer, "connect", strlen("connect")) == 0 );


        /* Verifico che il messaggio sia per un thread */
        if (check_connect_msg == false && check_list == true) {
            mutex_unlock(&mutex);


            continue;   // se non è una richiesta ignoro il messaggio
        }


        /* Consumazione effettiva dei dati dalla socket */
        bytes_received = recvfrom(server_socket, buffer, MAX_BUFFER_SIZE, 0, (struct sockaddr *)&client_address, &addr_len);
        if (bytes_received < 0) {
            printf("Errore[%d] recvfrom(): %s\n", errno, strerror(errno));
            mutex_unlock(&mutex);


            continue;   // in caso di errore non terminiamo
        } 


        /* Stampa messaggi ricevuti */
        printf("%s%s%s: dati da ", BOLDGREEN, argv[0], RESET);
        printf("%s%s%s:", CYAN, inet_ntoa(client_address.sin_addr), RESET);
        printf("%sUDP%u%s : ", MAGENTA, ntohs(client_address.sin_port), RESET);
        printf("%s%s%s\n", BOLDYELLOW, buffer, RESET);


        /* Gestore delle casistiche */
        if (check_connect_msg == true && check_list == false) {   /* Caso 1: è una connect e non è già in lista */
            printf("TENTATIVO DI CONNESSIONE\n");


            /* Controllo per evitare segmetion fault */
            if (is_empty(client_list)) {
                pos_client = NULL;  

            }


            /* Inserimento del client nella lista dei client connessi */
            pos_client = insert(client_list, pos_client, client_address); // pos viene incremtato dalla funzione stessa ogni volta
            printf("CLIENT INSERITO IN LISTA\n");
            print_list(client_list);


            /* Tentativo di connessione */
            if (accept_client(server_socket, client_address, &mutex) < 0) {
                printf("Errore[%d] accept_client() [connessione rifiutata]: %s\n", errno, strerror(errno));
                printf("CLIENT RIMOSSO DALLA LISTA\n");
                remove_node(client_list, pos_client);
                print_list(client_list);

                mutex_unlock(&mutex);
                    

                continue;   // in caso di errore non terminiamo
            }

        } else if (check_connect_msg == true && check_list == true) {   /* Caso 2: è una connect ma è già in lista */
            printf("%sUn client ha tentato una connessione essendo già connesso%s\n\n", RED, RESET);


        } else if (check_connect_msg == false && check_list == false) { /* Caso 3: non è una connect e non è già in lista */
            printf("%sUn client ha inviato un messaggio senza essere connesso%s\n\n", RED, RESET);


        } else {
            printf("%sComando non riconosciuto%s\n\n", RED, RESET);

        } 


        /* Sblocco il mutex dopo aver letto dalla socket ed accettato la conessione*/
        mutex_unlock(&mutex);
    }

    

    return EXIT_SUCCESS;
}




/* 
    Verifica che la cartella sia presente in caso contrario la creo
*/
DIR *check_directory(const char *path_name) {
    DIR *directory;


RETRY:
    directory = opendir(path_name);


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
    socklen_t addr_len;
    ssize_t bytes_received;
    char buffer[MAX_BUFFER_SIZE];
    struct sockaddr_in client_address_expected;
    pthread_t tid;
    ClientInfo *client_info;
    time_t start_time;
    time_t current_time;
    time_t elapsed_time;
    time_t max_duration = 5;               // durata massima in secondi del timer
    node_t *pos = pos_client;              // salvo la posizione prima che venga modificata


    memset(buffer, 0, MAX_BUFFER_SIZE);
    addr_len = sizeof(client_address);
    client_address_expected = client_address;


    /* Invio conferma della connessione */
    snprintf(buffer, MAX_BUFFER_SIZE, "connected");
    if (sendto(server_socket, buffer, strlen(buffer), 0, (struct sockaddr *)&client_address, sizeof(client_address)) < 0) {
        printf("Errore[%d] sendto(): %s\n", errno, strerror(errno));


        return EXIT_ERROR;
    }


    /* Sblocco per implementare una logica di sincronizzazione sicura tra i threads */
    mutex_unlock(mutex_pointer); 


    start_time = time(NULL);     // tempo di inizio del timer

    while (1) {
        memset(buffer, 0, MAX_BUFFER_SIZE);


        /* Blocco per implementare una logica di sincronizzazione sicura tra i threads */
        if (mutex_lock(&mutex) < 0) {
            printf("Errore[%d] mutex_lock(): %s\n", errno, strerror(errno));
        

            return EXIT_ERROR;
        }


        /* Calcolo il tempo trascorso */
        current_time = time(NULL);
        elapsed_time = current_time - start_time;


        /* Esco dal ciclo se sono passati più di 5 secondi */
        if (elapsed_time >= max_duration) {
            printf("CONNESSIONE NON RIUSCITA, TIMER SCADUTO\n");


            break;
        }


        /* Ricezione messaggio di ACK per connessione a tre vie senza consumare i dati */
        bytes_received = recvfrom(server_socket, buffer, MAX_BUFFER_SIZE, MSG_PEEK, (struct sockaddr *)&client_address, &addr_len);
        if (bytes_received < 0) {
            printf("Errore[%d] recvfrom(): %s\n", errno, strerror(errno));


            return EXIT_ERROR;
        }


        /* Verifico se l'indirizzo IP e la porta del mittente non corrispondono a quelli previsti */
        if (memcmp(&client_address, &client_address_expected, addr_len) != 0) {
            mutex_unlock(mutex_pointer); // in questo modo il vero destinatario ha la possibilità di consumare i dati


            continue; // in caso non corrispondano ignoro il messaggio e ritento fino allo scadere del timer
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
    client_info->pos_client = pos;


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
int send_file_list(int server_socket, struct sockaddr_in client_address) {
    size_t entry_len;
    size_t total_len;
    size_t residual_buffer = MAX_BUFFER_SIZE;
    int extra_char = 1;     // numero di caratteri extra tra un nome e l'altro (in questo caso solo '\n')
    int regular_files = 0;  // flag per verificare la presenza di file regolari
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


    /* Leggo il contenuto della cartella (creo lista file) */
    temp_buffer = buffer;
    while ((entry = readdir(directory)) != NULL) {
        entry_len = strlen(entry->d_name);
        total_len = entry_len + extra_char;


        /* Considero solo i file regolari (non conta ad esempio i file "." e "..") */
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
    if (file == NULL) {
        printf("Errore[%d] fopen(): %s\n", errno , strerror(errno));

        if (errno == ENOENT) {
            // inviare comando di file mancante
            printf("FILE INESISTENTE\n");

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
    buffer = (char *)malloc(buffer_size * sizeof(char));
    if (buffer == NULL) {
        printf("Errore[%d] malloc(): %s\n", errno, strerror(errno));


        return EXIT_ERROR;
    }


    memset(buffer, 0, buffer_size);


    printf("DIMENSIONE FILE: %ld BYTE\n", file_size);
    printf("TAGLIA FRAMMENTI FILE: %ld BYTE\n", buffer_size);


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


    free(buffer);
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
    socklen_t addr_len;
    ssize_t bytes_received;
    char buffer[MAX_BUFFER_SIZE];
    char full_path[MAX_BUFFER_SIZE];
    FILE *file;
    DIR *directory;
    struct sockaddr_in client_address_expected = client_address;


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


    /* Sblocco per implementare una logica di sincronizzazione sicura tra i threads */
    mutex_unlock(mutex_pointer); 


    while (1) {
        memset(buffer, 0, MAX_BUFFER_SIZE);


        /* Blocco per implementare una logica affidapthread_mutex_lock */
        if (mutex_lock(mutex_pointer) < 0) {
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
            mutex_unlock(mutex_pointer); // in questo modo il vero destinatario ha la possibilità di consumare i dati


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
        mutex_unlock(mutex_pointer);
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
    pthread_mutex_t *mutex_pointer;
    ClientInfo *client_info;
    struct sockaddr_in client_address;
    struct sockaddr_in client_address_expected;
    node_t *pos_client;


    tid = pthread_self();
    client_info = (ClientInfo *)arg;
    client_address_expected = client_info->client_address;
    server_socket = client_info->server_socket;
    mutex_pointer = client_info->mutex_pointer;
    pos_client = client_info->pos_client;
        

    printf("THREAD[%ld] AVVIATO\n", tid);


    while (1) {
        memset(buffer, 0, MAX_BUFFER_SIZE);
        addr_len = sizeof(client_address);


        /* Blocco il mutex prima di leggere dalla socket */
        if (mutex_lock(mutex_pointer) < 0) {
            printf("Errore[%d] mutex_lock(): %s\n", errno, strerror(errno));
            free(client_info);
        

            pthread_exit(NULL);
        }
        

        /* Ricezione del comando dal client utilizzando MSG_PEEK per non consumare i dati */
        bytes_received = recvfrom(server_socket, buffer, MAX_BUFFER_SIZE, MSG_PEEK, (struct sockaddr *)&client_address, &addr_len);
        if (bytes_received < 0) {
            printf("Errore[%d] recvfrom(): %s\n", errno, strerror(errno));
            mutex_unlock(mutex_pointer);
            free(client_info);


            pthread_exit(NULL);
        }


        /* Verifico se l'indirizzo IP e la porta del mittente non corrispondono a quelli previsti */
        if (memcmp(&client_address, &client_address_expected, addr_len) != 0) {
            mutex_unlock(mutex_pointer); // rilascio il mutex in modo che il messaggio venga consumato da un altro thread


            continue; // in caso non corrispondano ignoro il messaggio
        }


        /* Consumazione effettiva dei dati dalla socket */
        bytes_received = recvfrom(server_socket, buffer, MAX_BUFFER_SIZE, 0, (struct sockaddr *)&client_address, &addr_len);
        if (bytes_received < 0) {
            printf("Errore[%d] recvfrom(): %s\n", errno, strerror(errno));
            mutex_unlock(mutex_pointer);
            free(client_info);


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
            if (send_file_list(server_socket, client_address) < 0) {
                printf("THREAD[%ld] TERMINATO\n", tid);
                mutex_unlock(mutex_pointer);
                free(client_info);

                // chiudere la connessione + messaggio errore
                // inviare comando di fine connessione


                pthread_exit(NULL);
            }


        } else if (strncmp(buffer, "get ", 4) == 0) {
            char* filename = buffer + 4;
            if (send_file(server_socket, client_address, filename) < 0) {
                printf("THREAD[%ld] TERMINATO\n", tid);
                mutex_unlock(mutex_pointer);
                free(client_info);

                // chiudere la connessione + messaggio errore
                // inviare comando di fine connessione


                pthread_exit(NULL);
            }


        } else if (strncmp(buffer, "put ", 4) == 0) {
            char* filename = buffer + 4;
            if (receive_file(server_socket, client_address, filename, mutex_pointer) < 0) {
                printf("THREAD[%ld] TERMINATO\n", tid);
                mutex_unlock(mutex_pointer);
                free(client_info);

                // chiudere la connessione + messaggio errore
                // inviare comando di fine connessione


                pthread_exit(NULL);
            }


        } else if (strcmp(buffer, "close") == 0) {
            printf("THREAD[%ld] TERMINATO\n", tid);
            printf("CLIENT RIMOSSO DALLA LISTA\n");
            remove_node(client_list, pos_client);
            print_list(client_list);
            free(client_info);


            mutex_unlock(mutex_pointer);
            pthread_exit(NULL);

        } else {
            printf("%sComando non riconosciuto%s\n\n", RED, RESET);


        } 


        /* Sblocco il mutex dopo aver letto dalla socket */
        mutex_unlock(mutex_pointer);   
    }
}


/*
    Funzione di gestione del segnale SIGINT
*/
void handle_ctrl_c(int __attribute__((unused)) signum, siginfo_t __attribute__((unused)) *info, void __attribute__((unused)) *context) {
    printf("\b\b%sSegnale Ctrl+C. %sExiting...%s\n", BOLDGREEN, BOLDYELLOW, RESET);
    
    
    // invia segnale a tutti i client (serve lista client)

    pthread_mutex_destroy(&mutex); // Distrugge il mutex

    if (close(server_socket) < 0) {
        printf("Errore close(): %s\n", strerror(errno)); 


        exit(EXIT_FAILURE);
    }

    exit(EXIT_SUCCESS);
}

