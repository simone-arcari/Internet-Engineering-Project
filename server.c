#include <stdio.h>
#include <errno.h>
#include <netdb.h>
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


/* Struttura per passare dati ai threads*/
typedef struct {
    int client_socket;
    struct sockaddr_in client_address;
} ClientInfo;


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


                exit(EXIT_FAILURE);
            }

        } else {

            exit(EXIT_FAILURE);
        }
    }

    return directory;
}


/*
    Implementa la logica per inviare la lista dei file disponibili al client
    Utilizza la socket server_socket e l'indirizzo del client client_address
*/
void send_file_list(int server_socket, struct sockaddr_in client_address) {
    struct dirent *entry;
    DIR *directory;
    char *buffer, *temp_buffer;
    int residual_buffer = MAX_BUFFER_SIZE;
    int entry_len, total_len;
    int extra_char = 1;     //numero di caratteri extra tra un nome e l'altro (in questo caso solo '\n')
    int regular_files = 0;  // Flag per verificare la presenza di file regolari


    /* Verifico l'esistenza della cartella */
    directory = check_directory(PATH_FILE_FOLDER);
    if (directory == NULL) {
        printf("Errore[%d] check_directory(): %s\n",errno , strerror(errno));

       
        exit(EXIT_FAILURE);
    }


    /* Alloco memoria */
    buffer = (char *)malloc(MAX_BUFFER_SIZE * sizeof(char));
    if (buffer == NULL) {
        printf("Errore[%d] malloc(): %s\n", errno, strerror(errno));


        exit(EXIT_FAILURE);
    }


    /* Leggo il contenuto della cartella (creo lista file) */
    temp_buffer = buffer;
    while ((entry = readdir(directory)) != NULL) {
        entry_len = strlen(entry->d_name);
        total_len = entry_len+extra_char;

        if (entry->d_type == DT_REG) {  /* Considera solo i file regolari (non conta ad esempio i file "." e "..") */
            if (total_len <= residual_buffer && residual_buffer > 0) {  /* Gestione del limite di memoria del buffer */
                sprintf(temp_buffer, "%s\n", entry->d_name);
                residual_buffer -= total_len;
                temp_buffer += total_len;
                regular_files++;

            }
        }
    }    


    /* Se non trovo file la cartella è vuota */
    if (regular_files == 0) {
        sprintf(buffer, "%s%s%s\n", BOLDRED, "NESSUN FILE PRESENTE", RESET);


    }


    /* Invio della lista al client */
    sendto(server_socket, buffer, strlen(buffer), 0, (struct sockaddr *)&client_address, sizeof(client_address));
    printf("\n%s%sLista file inviata al client:%s\n", BOLDBLACK, BG_MAGENTA, RESET);
    printf("%s%s%s\n", GREEN, buffer, RESET);

    closedir(directory);
    free(buffer);
}


/*
    Implementa la logica per inviare un file al client
    Utilizza la socket server_socket, l'indirizzo del client client_address e il nome del file filename
*/
void send_file(int server_socket, struct sockaddr_in client_address, char* filename) {
    DIR *directory;
    FILE *file;
    size_t bytes_read;
    char buffer[MAX_BUFFER_SIZE];


    /* Verifico l'esistenza della cartella */
    directory = check_directory(PATH_FILE_FOLDER);
    if (directory == NULL) {
        printf("Errore[%d] check_directory(): %s\n", errno , strerror(errno));

       
        exit(EXIT_FAILURE);
    }


    /* Compongo il percorso completo del file */
    char full_path[MAX_BUFFER_SIZE]; 
    snprintf(full_path, sizeof(full_path), "%s/%s", PATH_FILE_FOLDER, filename);


    /* Apro il file in modalità lettura binaria */
    file = fopen(full_path, "rb");
    if (file == NULL) {
        printf("Errore[%d] fopen(): %s\n", errno , strerror(errno));


        exit(EXIT_FAILURE);
    }


    /* Invio dei pacchetti del file al client */
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        sendto(server_socket, buffer, bytes_read, 0, (struct sockaddr *)&client_address, sizeof(client_address));
    }


    /* Invio di un pacchetto vuoto come segnale di completamento */
    sendto(server_socket, NULL, 0, 0, (struct sockaddr *)&client_address, sizeof(client_address));


    fclose(file);
    printf("File inviato con successo.\n\n");
}


/*
    Implementa la logica per ricevere un file dal client
    Utilizza la socket server_socket, l'indirizzo del client client_address e il nome del file filename
*/
void receive_file(int server_socket, struct sockaddr_in client_address, char* filename) {
    DIR *directory;
    FILE *file;
    int bytes_received;
    char buffer[MAX_BUFFER_SIZE];


    /* Verifico l'esistenza della cartella */
    directory = check_directory(PATH_FILE_FOLDER);
    if (directory == NULL) {
        printf("Errore[%d] check_directory(): %s\n",errno , strerror(errno));

       
        exit(EXIT_FAILURE);
    }


    /* Compongo il percorso completo del file */
    char full_path[MAX_BUFFER_SIZE]; 
    snprintf(full_path, sizeof(full_path), "%s/%s", PATH_FILE_FOLDER, filename);


    /* Apertura del file in modalità scrittura binaria */
    file = fopen(full_path, "wb");
    if (file == NULL) {
        printf("Errore fopen(): %s\n", strerror(errno));


        exit(EXIT_FAILURE);
    }


    while (1) {
        bytes_received = recvfrom(server_socket, buffer, sizeof(buffer), 0, NULL, NULL);
        if (bytes_received < 0) {
            perror("Errore nella recvfrom");
            exit(EXIT_FAILURE);
        }


        /* Ricezione completata */
        if (bytes_received == 0) {


            break;
        }


        /* Scrivo i dati ricevuti nel file */
        fwrite(buffer, 1, bytes_received, file);
    }


    fclose(file);   // Chiudo il file
    printf("File ricevuto con successo.\n\n");
}


void *handle_client(void *arg) {
    ClientInfo *client_info = (ClientInfo *)arg;
    int client_socket = client_info->client_socket;
    struct sockaddr_in client_address = client_info->client_address;
    char buffer[MAX_BUFFER_SIZE];
    

    /* Gestore dei comandi */
    if (strcmp(buffer, "list") == 0) {
        send_file_list(client_socket, client_address);



    } else if (strncmp(buffer, "get ", 4) == 0) {
        char* filename = buffer + 4;
        send_file(client_socket, client_address, filename);



    } else if (strncmp(buffer, "put ", 4) == 0) {
        char* filename = buffer + 4;
        receive_file(client_socket, client_address, filename);



    } else {
        printf("%sComando non riconosciuto%s\n\n", RED, RESET);



    }   
    
    // Chiudi la socket del client
    //close(client_socket);
    //free(client_info);
    
    return NULL;
}



int main(int argc, char *argv[]) {
    int server_socket, client_socket, addr_len, bytes_received;
    struct sockaddr_in server_address, client_address;
    char buffer[MAX_BUFFER_SIZE];
    pthread_t tid;
    

    /* Verifico l'esistenza della cartella preposta per contenere i file */
    if (check_directory(PATH_FILE_FOLDER) == NULL) {
        printf("Errore[%d] check_directory(): %s\n",errno , strerror(errno));

       
        exit(EXIT_FAILURE);
    }
    


    /* Creazione della socket UDP del server */
    server_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (server_socket < 0) {
        printf("Errore[%d] socket(): %s\n", errno, strerror(errno));   // Controllare variabile globale errno qui
        

        exit(EXIT_FAILURE);
    }
    


    /* Configurazione dell'indirizzo del server UDP*/
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(DEFAULT_PORT);
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);     // il server accetta richieste su ogni interfaccia di rete 
    


    /* Associazione dell'indirizzo del server alla socket */
    if (bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {   
        printf("Errore[%d] bind(): %s\n", errno, strerror(errno));
        

        exit(EXIT_FAILURE);
    }
    

    printf("%sServer in ascolto sulla porta %d...%s\n\n", BOLDRED, DEFAULT_PORT, RESET);
    

    while (1) {
        memset(buffer, 0, MAX_BUFFER_SIZE);
        addr_len = sizeof(client_address);
        

/*
        /* Ricezione del comando dal client /
        bytes_received = recvfrom(server_socket, buffer, MAX_BUFFER_SIZE, 0, (struct sockaddr *)&client_address, &addr_len);
        if (bytes_received < 0) {
            printf("Errore[%d] recvfrom(): %s\n", errno, strerror(errno));


            exit(EXIT_FAILURE);
        }
        

        buffer[bytes_received] = '\0';  // imposto il terminatore di stringa


        /* Stampa messaggi ricevuti /
        printf("%s%s%s: dati da ", BOLDGREEN, argv[0], RESET);
        printf("%s%s%s:", CYAN, inet_ntoa(client_address.sin_addr), RESET);
        printf("%sUDP%u%s : ", MAGENTA, ntohs(client_address.sin_port), RESET);
        printf("%s%s%s\n", BOLDYELLOW, buffer, RESET);



        /* Gestore dei comandi /
        if (strcmp(buffer, "list") == 0) {
            send_file_list(server_socket, client_address);



        } else if (strncmp(buffer, "get ", 4) == 0) {
            char* filename = buffer + 4;
            send_file(server_socket, client_address, filename);



        } else if (strncmp(buffer, "put ", 4) == 0) {
            char* filename = buffer + 4;
            receive_file(server_socket, client_address, filename);



        } else {
            printf("%sComando non riconosciuto%s\n\n", RED, RESET);



        }
 */       



    



        // HO SBAGLIATO PERCHE STIAMO IN UDP E NON TCP (CORREGERE)

        // Accetta la connessione dal client
        client_socket = accept(server_socket, (struct sockaddr *)&client_address, &addr_len);
        if (client_socket == -1) {
            printf("Errore[%d] accept(): %s\n", errno, strerror(errno));


            continue;
        }




        
        // Creazione di una struttura ClientInfo per passare le informazioni del client al thread
        ClientInfo *client_info = (ClientInfo *)malloc(sizeof(ClientInfo));
        if (client_info == NULL) {
            printf("Errore[%d] malloc(sizeof(ClientInfo)): %s\n", errno, strerror(errno));


            exit(EXIT_FAILURE);
        }

        client_info->client_socket = client_socket;
        client_info->client_address = client_address;
        





        // Creazione di un nuovo thread per gestire il client
        if (pthread_create(&tid, NULL, handle_client, (void *)client_info) != 0) {
            printf("Errore[%d] pthread_create(): %s\n", errno, strerror(errno));
            close(client_socket);
            freee(client_info);
            

            continue;
        }
        
        // Detach del thread per permettere la terminazione automatica
        pthread_detach(tid);
    }

    

    return EXIT_SUCCESS;
}
