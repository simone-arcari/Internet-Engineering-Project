#include <stdio.h>
#include <errno.h>
#include <netdb.h>
#include <signal.h>
#include <dirent.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>



#define MAX_BUFFER_SIZE 1024
#define DEFAULT_PORT 8888
//#define IP_SERVER "192.168.1.22"    // PC FISSO
//#define IP_SERVER "192.168.1.27"    // PC PORTATILE
#define IP_SERVER "10.13.46.152"     // PER PROVE FUORI CASA
#define PATH_FILE_FOLDER "file_folder_client"   // Path per la cartella preposta per i file
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


int client_socket;
struct sockaddr_in server_address;



/* Verifica che la cartella sia presente in caso contrario la creo */
DIR *check_directory(const char *__name) {
    DIR *directory;


RETRY:
    directory = opendir(PATH_FILE_FOLDER);


    if (directory == NULL) {
        printf("Errore[%d] opendir(): %s\n",errno , strerror(errno));

        /* se la cartella non esisteva la creo */
        if (errno == ENOENT) {
            if (mkdir(PATH_FILE_FOLDER, 0755) == 0) {   // Permessi numerazione ottalle |7|7|5|
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


    /* Invio del comando al server */
    sendto(client_socket, buffer, strlen(buffer), 0, (struct sockaddr *)&server_address, sizeof(server_address));


    /* Imposto scadenza del timer */
    alarm(5);


    /* Risposta del server */
    bytes_received = recvfrom(client_socket, buffer, sizeof(buffer), 0, NULL, NULL);
    if (bytes_received < 0) {
        printf("Errore recvfrom(): %s\n", strerror(errno));


        return EXIT_ERROR;
    } 


    buffer[bytes_received] = '\0';  // imposto il terminatore di stringa


    /* Reset scadenza del timer */
    alarm(0);


    /* Esito della connessione */
    if (strcmp(buffer, "connected") == 0) {
        printf("SERVER CONNESSO\n");

        
        /* Invio del comando al server */
        snprintf(buffer, MAX_BUFFER_SIZE, "ack");
        sendto(client_socket, buffer, strlen(buffer), 0, (struct sockaddr *)&server_address, sizeof(server_address));


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
int receive_file_list(int client_socket, struct sockaddr_in server_address) {
    int bytes_received;
    char buffer[MAX_BUFFER_SIZE];


    bytes_received = recvfrom(client_socket, buffer, sizeof(buffer), 0, NULL, NULL);
    if (bytes_received < 0) {
        printf("Errore recvfrom(): %s\n", strerror(errno));


        return EXIT_ERROR;
    }       


    buffer[bytes_received] = '\0';  // imposto il terminatore di stringa
    printf("\n%s%sLista file ricevuta dal server:%s\n", BOLDBLACK, BG_MAGENTA, RESET);
    printf("%s%s%s\n", GREEN, buffer, RESET);


    return EXIT_SUCCESS;
}


/*
    Implementa la logica per ricevere un file dal server
    Utilizza la socket client_socket, l'indirizzo del client server_address e il nome del file filename
*/
int download_file(int client_socket, struct sockaddr_in server_address, char* filename) {
    DIR *directory;
    FILE *file;
    int bytes_received;
    char buffer[MAX_BUFFER_SIZE];


    /* Verifico l'esistenza della cartella */
    directory = check_directory(PATH_FILE_FOLDER);
    if (directory == NULL) {
        printf("Errore[%d] check_directory(): %s\n",errno , strerror(errno));

       
        return EXIT_ERROR;
    }


    /* Compongo il percorso completo del file */
    char full_path[MAX_BUFFER_SIZE]; 
    snprintf(full_path, sizeof(full_path), "%s/%s", PATH_FILE_FOLDER, filename);


    /* Apertura del file in modalità scrittura binaria */
    file = fopen(full_path, "wb");
    if (file == NULL) {
        printf("Errore fopen(): %s\n", strerror(errno));


        return EXIT_ERROR;
    }


    while (1) {
        bytes_received = recvfrom(client_socket, buffer, sizeof(buffer), 0, NULL, NULL);
        if (bytes_received < 0) {
            printf("Errore recvfrom(): %s\n", strerror(errno));


            return EXIT_ERROR;
        }


        /* Ricezione completata */
        if (bytes_received == 0) {


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
int upload_file(int client_socket, struct sockaddr_in server_address, char* filename) {
    DIR *directory;
    FILE *file;
    size_t bytes_read;
    char buffer[MAX_BUFFER_SIZE];


    /* Verifico l'esistenza della cartella */
    directory = check_directory(PATH_FILE_FOLDER);
    if (directory == NULL) {
        printf("Errore[%d] check_directory(): %s\n",errno , strerror(errno));

       
        return EXIT_ERROR;
    }

/*
    // Compongo il percorso completo del file
    char full_path[MAX_BUFFER_SIZE]; 
    snprintf(full_path, sizeof(full_path), "%s/%s", PATH_FILE_FOLDER, filename);
*/

    /* Apro il file in modalità lettura binaria */
    file = fopen(filename, "rb");
    if (file == NULL) {
        printf("Errore[%d] fopen(): %s\n",errno , strerror(errno));


        return EXIT_ERROR;
    }


    /* Invio dei pacchetti del file al client */
    while ((bytes_read = fread(buffer, 1, MAX_BUFFER_SIZE, file)) > 0) {
        sendto(client_socket, buffer, bytes_read, 0, (struct sockaddr *)&server_address, sizeof(server_address));
    }


    /* Invio di un pacchetto vuoto come segnale di completamento */
    sendto(client_socket, NULL, 0, 0, (struct sockaddr *)&server_address, sizeof(server_address));


    fclose(file);
    closedir(directory);
    printf("FILE INVIATO CON SUCCESSO\n\n");


    return EXIT_SUCCESS;
}


/*
    Funzione di gestione del segnale SIGINT
*/
void handle_ctrl_c(int signum, siginfo_t *info, void *context) {
    char buffer[MAX_BUFFER_SIZE] = "close";


    printf("\b\b%sSegnale Ctrl+C. %sExiting...%s\n", BOLDGREEN, BOLDYELLOW, RESET);
    

    /* Invio del comando al server */
    //sendto(client_socket, buffer, strlen(buffer), 0, (struct sockaddr *)&server_address, sizeof(server_address));


    if (close(client_socket) < 0) {
        printf("Errore close(): %s\n", strerror(errno)); 


        exit(EXIT_FAILURE);
    }

    exit(EXIT_SUCCESS);
}


/*
    Funzione di gestione del segnale SIGALARM
*/
void handle_alarm(int signum) {
    printf("CONNESSIONE NON RIUSCITA, TIMER SCADUTO\n");
    printf("%sExiting...%s\n", BOLDYELLOW, RESET);


    exit(EXIT_SUCCESS);
}


int main(int argc, char *argv[]) {
    int bytes_received;
    char buffer[MAX_BUFFER_SIZE];
    struct sigaction sa1;
    struct sigaction sa2;
    

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


    /* Per debug */
    if (argc == 2) {
        sprintf(buffer, "%s", argv[1]);
        goto JUMP;

    } else if (argc == 3) {
        sprintf(buffer, "%s %s", argv[1], argv[2]);
        goto JUMP;

    }


    /*Richiesta di input da parte dell'utente*/
    while (1) { 
        printf("%s%s%s: ", BOLDGREEN, argv[0], RESET);
        printf("Inserisci un comando (list, get <nome_file>, put <path/nome_file>): %s", BOLDYELLOW);
        fgets(buffer, MAX_BUFFER_SIZE, stdin);
        buffer[strlen(buffer)-1] = '\0';
        printf("%s", RESET);

JUMP:
        /* Invio del comando al server */
        sendto(client_socket, buffer, strlen(buffer), 0, (struct sockaddr *)&server_address, sizeof(server_address));


        /* Gestore del comando inviato */
        if (strcmp(buffer, "list") == 0) {  /* Richiesta della lista dei file disponibili al server*/
            if (receive_file_list(client_socket, server_address) < 0) {


                // stampa errore ma non terminare
            }



        } else if (strncmp(buffer, "get ", 4) == 0) {    /* Download file dal server*/          
            char* filename = buffer + 4;
            if (download_file(client_socket, server_address, filename) < 0) {


                // stampa errore ma non terminare
            }



        } else if (strncmp(buffer, "put ", 4) == 0) {    /* Upload file al server */
            char* filename = buffer + 4;
            if (upload_file(client_socket, server_address, filename) < 0) {


                // stampa errore ma non terminare
            }



        } else if (strcmp(buffer, "close") == 0) {  /*Chiusura programma e chiusura della socket del client*/
            if (close(client_socket) < 0) {
                printf("Errore close(): %s\n", strerror(errno)); 


                exit(EXIT_FAILURE);
            }

            exit(EXIT_SUCCESS);
        }
    }



    return EXIT_SUCCESS;
}
