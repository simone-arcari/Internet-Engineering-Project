/**
 * @file client.c
 * @brief Breve descrizione del file.
 *
 * 
 *
 * @authors Simone Arcari, Valeria Villano
 * @date 2023-09-09 (nel formato YYYY-MM-DD)
 */


#include <stdio.h>
#include <errno.h>
#include <netdb.h>
#include <signal.h>
#include <dirent.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "gobackn.h"



#define MAX_BUFFER_SIZE 1024            // 1kB per invio ricezione messaggi livello applicativo
#define MAX_BUFFER_FILE_SIZE 32768      // 32kB per ricezione invio file (il server deve inviare frammenti file massimo da 32kB)

#define DEFAULT_PORT 8888
#define IP_SERVER "127.0.0.1"

#define PATH_FILE_FOLDER "file_folder_client"   // Path per la cartella preposta per i file

#define SMALL_SIZE  1024        // 1kB  valore oltre il quale si considera un file "small" prima di questo valore lo si considera "micro"
#define MEDIUM_SIZE 10240       // 10kB valore oltre il quale si considera un file "medium"
#define LARGE_SIZE  1048576     // 1MB  valore oltre il quale si considera un file "large"

#define FRAGMENT_MICRO_SIZE     1024    // 1kB     dimensione dei frammenti del file passati al livello di trasporto 
#define FRAGMENT_SMALL_SIZE     8192    // 8kB     dimensione dei frammenti del file passati al livello di trasporto
#define FRAGMENT_MEDIUM_SIZE    16384   // 16kB    dimensione dei frammenti del file passati al livello di trasporto
#define FRAGMENT_LARGE_SIZE     32768   // 32kB    dimensione dei frammenti del file passati al livello di trasporto (il client deve poter ricever frammenti file fino a 32kB)

#define EXIT_ERROR -1
#define TIMEOUT_CONNECTION 10

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


int client_socket;
struct sockaddr_in server_address;
pthread_mutex_t mutex;

struct sigaction sa1;
struct sigaction sa2;


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
    Implementa la logica per richiedere una connessione ad un server UDP
    Utilizza la socket client_socket e l'indirizzo del client server_address
*/
int connect_server(int client_socket, struct sockaddr_in server_address) {
    int bytes_received;
    char buffer[MAX_BUFFER_SIZE] = "connect";


    printf("TENTATIVO DI CONNESSIONE\n");


    /* Invio del comando al server con comunicazione non affidabile */
    sendto(client_socket, buffer, strlen(buffer), 0, (struct sockaddr *)&server_address, sizeof(server_address));


    /* Imposto scadenza del timer */
    alarm(TIMEOUT_CONNECTION);


    memset(buffer, 0, MAX_BUFFER_SIZE);
    bytes_received = rcv_msg(client_socket, buffer, (struct sockaddr *)&server_address);
    if (bytes_received < 0) {
        printf("Errore rcv_msg(): %s\n", strerror(errno));


        return EXIT_ERROR;
    }


    /* Reset scadenza del timer */
    alarm(0);


    /* Esito della connessione */
    if (strcmp(buffer, "connected") == 0) {
        
        /* Invio del comando al server */
        snprintf(buffer, MAX_BUFFER_SIZE, "ack");
        if (send_msg(client_socket, buffer, strlen(buffer), (struct sockaddr *)&server_address) < 0) {
            printf("Errore[%d] send_msg(): %s\n", errno, strerror(errno));


            return EXIT_ERROR;
        }

        printf("SERVER CONNESSO\n");
        return EXIT_SUCCESS;
        
    } else {

        printf("SERVER NON CONNESSO\n");
        return EXIT_ERROR;
    } 
}


/*
    Implementa la logica per richiedere la lista dei file disponibili al server
    Utilizza la socket client_socket e l'indirizzo del server server_address
*/
int receive_file_list(int client_socket) {
    int bytes_received;
    char buffer[MAX_BUFFER_FILE_SIZE];


    /* Ricevo la lista */
    memset(buffer, 0, MAX_BUFFER_FILE_SIZE);
    bytes_received = rcv_msg(client_socket, buffer, (struct sockaddr *)&server_address);
    if (bytes_received < 0) {
        printf("Errore rcv_msg(): %s\n", strerror(errno));


        return EXIT_ERROR;
    }       


    buffer[bytes_received] = '\0';  // imposto il terminatore di stringa
    printf("\n%s%sLista file inviata dal server:%s\n", BOLDBLACK, BG_MAGENTA, RESET);
    printf("%s%s%s\n", YELLOW, buffer, RESET);


    return EXIT_SUCCESS;
}


/*
    Implementa la logica per ricevere un file dal server
    Utilizza la socket client_socket, l'indirizzo del client server_address e il nome del file filename
*/
int download_file(int client_socket, char* filename) {
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
        bytes_received = rcv_msg(client_socket, buffer, (struct sockaddr *)&server_address);
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
    Implementa la logica per inviare un file al server
    Utilizza la socket client_socket, l'indirizzo del client server_address e il nome del file filename
*/
int upload_file(int server_socket, struct sockaddr_in client_address, char* filename) {
    DIR *directory;
    FILE *file;
    size_t bytes_read;
    long file_size;
    long buffer_size;
    char *buffer;


    /* Verifico l'esistenza della cartella */
    directory = check_directory(PATH_FILE_FOLDER);
    if (directory == NULL) {
        printf("Errore[%d] check_directory(): %s\n", errno , strerror(errno));

       
        return EXIT_ERROR;
    }


    /* Apro il file in modalità lettura binaria */
    file = fopen(filename, "rb");
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

    /* Invio di un pacchetto vuoto come segnale di completamento */
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


void *receive_thread(void __attribute__((unused)) *arg) {
    char buffer[MAX_BUFFER_SIZE];
    int bytes_received = 0;


    while (1) {
        memset(buffer, 0, MAX_BUFFER_SIZE);

        /* Verifico se dispongo del permesso per proseguire */
        //if (mutex_lock(&mutex) < 0) {
     //      printf("Errore[%d] mutex_lock(): %s\n", errno, strerror(errno));
        

    //        exit(EXIT_FAILURE);
     //   }

        
        /* Ricezione messaggi senza consumare i dati */
        bytes_received = recvfrom(client_socket, buffer, MAX_BUFFER_SIZE, MSG_PEEK, NULL, NULL);
        if (bytes_received < 0) {
            printf("Errore[%d] recvfrom(): %s\n", errno, strerror(errno));


            exit(EXIT_FAILURE);
        }


        buffer[bytes_received] = '\0';


        /* Controllo se il messaggio è un comando di terminazione */
        if (strcmp(buffer, "close") == 0) {
            printf("\n\b\b%sSegnale chiusura connessione da parte del server. %sExiting...%s\n", BOLDGREEN, BOLDYELLOW, RESET);
            
                    
            exit(EXIT_SUCCESS);
        }
    }
    

}


/*
    Funzione di gestione del segnale SIGINT
*/
void handle_ctrl_c(int __attribute__((unused)) signum, siginfo_t __attribute__((unused)) *info, void __attribute__((unused)) *context) {
    char buffer[MAX_BUFFER_SIZE] = "close";


    printf("\n\b\b%sSegnale Ctrl+C. %sExiting...%s\n", BOLDGREEN, BOLDYELLOW, RESET);
    

    /* Invio del comando al server */
    if (send_msg(client_socket, buffer, strlen(buffer), (struct sockaddr *)&server_address) < 0) {
        printf("Errore[%d] send_msg(): %s\n", errno, strerror(errno));


        exit(EXIT_FAILURE);
    }    

    if (close(client_socket) < 0) {
        printf("Errore close(): %s\n", strerror(errno)); 


        exit(EXIT_FAILURE);
    }

    exit(EXIT_SUCCESS);
}


/*
    Funzione di gestione del segnale SIGALARM
*/
void handle_alarm(int __attribute__((unused)) signum) {
    printf("CONNESSIONE NON RIUSCITA, TIMER SCADUTO\n");
    printf("%sExiting...%s\n", BOLDYELLOW, RESET);


    exit(EXIT_SUCCESS);
}


void client_setup() {
    pthread_t tid;

    /* Configurazione di sigaction per Ctrl+c */
    sa1.sa_sigaction = handle_ctrl_c;    // funzione di gestione del segnale
    sa1.sa_flags = SA_SIGINFO;           // abilita l'uso di siginfo_t


    /* Configurazione di sigaction per il timer di fine tentativo di connessione */
    sa2.sa_handler = handle_alarm;      // Funzione di gestione del segnale
    sigemptyset(&sa2.sa_mask);          // Pulisco la maschera dei segnali


    /* Imposto l'handler per SIGINT (Ctrl+C) */
    if (sigaction(SIGINT, &sa1, NULL) < 0) {
        printf("Errore[%d] sigaction(): %s\n", errno , strerror(errno));
        

        exit(EXIT_FAILURE);
    }


    /* Imposto l'handler per SIGALRM */
    if (sigaction(SIGALRM, &sa2, NULL) == -1) {
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
        printf("Errore[%d] check_directory(): %s\n",errno , strerror(errno));

       
        exit(EXIT_FAILURE);
    }


    /* Creazione della socket UDP del client */
    client_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (client_socket < 0) {
        printf("Errore socket(): %s\n", strerror(errno)); 


        exit(EXIT_FAILURE);
    }


    /* Configurazione dell'indirizzo del server */
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(DEFAULT_PORT);
    if (inet_pton(AF_INET, IP_SERVER, &server_address.sin_addr) < 0) {
        printf("Errore inet_pton(): %s\n", strerror(errno)); 


        exit(EXIT_FAILURE);
    }


    /* Connessione al server */
    if (connect_server(client_socket, server_address) < 0) {
        printf("Errore connect_server(): %s\n", strerror(errno)); 


        exit(EXIT_FAILURE);
    }


    /* Blocco il mutex prima di avviare il thread per la ricezione del segnale di fine connessione */
    if (mutex_lock(&mutex) < 0) {
        printf("Errore[%d] mutex_lock(): %s\n", errno, strerror(errno));
        

        exit(EXIT_FAILURE);
    }


    /* Creo un thread per la ricezione asincrona di un eventuale messaggio di fine connessione */
    if (pthread_create(&tid, NULL, receive_thread, NULL) != 0) {
        printf("Errore pthread_create(): %s\n", strerror(errno)); 


        exit(EXIT_FAILURE);
    }
}


int main(int argc, char *argv[]) {
    char buffer[MAX_BUFFER_SIZE];
    

    /* Incapsula tutto il codice per inizializzare il client e le sue risorse */
    client_setup();
    

    /* Per debug */
    if (argc == 2) {
        sprintf(buffer, "%s", argv[1]);
        goto JUMP;

    } else if (argc == 3) {
        sprintf(buffer, "%s %s", argv[1], argv[2]);
        goto JUMP;

    }


    /* Richiesta di input da parte dell'utente */
    while (true) { 
        printf("%s%s%s: ", BOLDGREEN, argv[0], RESET);
        printf("Inserisci un comando (list, get <nome_file>, put <path/nome_file>): %s", BOLDYELLOW);
        fgets(buffer, MAX_BUFFER_SIZE, stdin);
        buffer[strlen(buffer)-1] = '\0';
        printf("%s", RESET);

JUMP:

        /* Invio del comando al server */
        if (send_msg(client_socket, buffer, strlen(buffer), (struct sockaddr *)&server_address) < 0) {
            printf("Errore[%d] send_msg(): %s\n", errno, strerror(errno));


            exit(EXIT_FAILURE);
        }
        

        /* Gestore del comando inviato */
        if (strcmp(buffer, "list") == 0) {               /* Richiesta della lista dei file disponibili al server*/
            if (receive_file_list(client_socket) < 0) {


                exit(EXIT_FAILURE);
            }



        } else if (strncmp(buffer, "get ", 4) == 0) {    /* Download file dal server*/         
            char* filename = buffer + 4;

            /* Verifico se è presente il nome del file */
            if (strlen(buffer) == 4) {
                printf("NOME FILE MANCANTE\n");
                
                
                exit(EXIT_FAILURE);
            }

            /* Verifico se il nome fornito inizia con uno spazio */
            if (filename[0] == ' ') {
                printf("NOME NON VALIDO\n");
                
                
                exit(EXIT_FAILURE);
            }

            /* Ricevo il file */
            if (download_file(client_socket, filename) < 0) {


                exit(EXIT_FAILURE);
            }



        } else if (strncmp(buffer, "put ", 4) == 0) {    /* Upload file al server */
            char* filename = buffer + 4;

            /* Verifico se è presente il nome del file */
            if (strlen(buffer) == 4) {
                printf("NOME FILE MANCANTE\n");
                
                
                exit(EXIT_FAILURE);
            }

            /* Verifico se il nome fornito inizia con uno spazio */
            if (filename[0] == ' ') {
                printf("NOME NON VALIDO\n");
                
                
                exit(EXIT_FAILURE);
            }

            /* Invio il file */
            if (upload_file(client_socket, server_address, filename) < 0) {


                exit(EXIT_FAILURE);
            }



        } else if (strcmp(buffer, "close") == 0) {       /* Chiusura programma e chiusura della socket del client */
            if (close(client_socket) < 0) {
                printf("Errore close(): %s\n", strerror(errno)); 


                exit(EXIT_FAILURE);
            }
            printf("\n\b\b%sSegnale chiusura connessione inviato al server. %sExiting...%s\n", BOLDGREEN, BOLDYELLOW, RESET);

            exit(EXIT_SUCCESS);
        }


        mutex_unlock(&mutex); // diamo il permesso a receive_thread di verificare la presenza del segnale di fine connessione 
    }


    exit(EXIT_SUCCESS);
}
