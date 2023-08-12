#include <stdio.h>
#include <errno.h>
#include <netdb.h>
#include <unistd.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>



#define MAX_BUFFER_SIZE 1024
#define DEFAULT_PORT 8888
//#define IP_SERVER "192.168.1.22"    // PC FISSO
//#define IP_SERVER "192.168.1.27"    // PC PORTATILE
#define IP_SERVER "10.13.46.152"     // PER PROVE FUORI CASA



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



void request_file_list(int client_socket, struct sockaddr_in server_address) {
    // Implementa la logica per richiedere la lista dei file disponibili al server
    // Utilizza la socket client_socket e l'indirizzo del server server_address
    int bytes_received;
    char buffer[MAX_BUFFER_SIZE];


    bytes_received = recvfrom(client_socket, buffer, sizeof(buffer), 0, NULL, NULL);
    if (bytes_received < 0) {
        printf("Errore recvfrom(): %s\n", strerror(errno));


        exit(EXIT_FAILURE);
    }       

    buffer[bytes_received] = '\0';
    printf("\n%s%sLista file ricevuta dal server:%s\n", BOLDBLACK, BG_MAGENTA, RESET);
    printf("%s%s%s", GREEN, buffer, RESET);
}


void request_file(int client_socket, struct sockaddr_in server_address, char* filename) {
    // Implementa la logica per richiedere un file al server
    // Utilizza la socket server_socket, l'indirizzo del server server_address e il nome del file filename
    FILE *file;
    int bytes_received;
    char buffer[MAX_BUFFER_SIZE];


    // Apertura del file in modalità scrittura binaria
    file = fopen(filename, "wb");
    if (file == NULL) {
        printf("Errore fopen(): %s\n", strerror(errno));


        exit(EXIT_FAILURE);
    }


    while (1) {
        bytes_received = recvfrom(client_socket, buffer, sizeof(buffer), 0, NULL, NULL);
        if (bytes_received < 0) {
            perror("Errore nella recvfrom");
            exit(EXIT_FAILURE);
        }


        // Ricezione completata
        if (bytes_received == 0) {


            break;
        }

        // Scrivi i dati ricevuti nel file
        fwrite(buffer, 1, bytes_received, file);
    }


    // Chiudi il file
    fclose(file);


    printf("File ricevuto con successo.\n");
}


void receive_file(int server_socket, struct sockaddr_in server_address, char* filename) {
    // Implementa la logica per ricevere un file dal server
    // Utilizza la socket server_socket, l'indirizzo del server server_address e il nome del file filename
}


void send_file(int client_socket, struct sockaddr_in client_address, char* filename) {
    // Implementa la logica per inviare un file al server
    // Utilizza la socket client_socket, l'indirizzo del client client_address e il nome del file filename
}




int main() {
    int client_socket, bytes_received;
    struct sockaddr_in server_address;
    char buffer[MAX_BUFFER_SIZE];



    // Creazione della socket del client
    client_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (client_socket < 0) {
        printf("Errore socket(): %s\n", strerror(errno)); 


        exit(EXIT_FAILURE);
    }



    // Configurazione dell'indirizzo del server
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(DEFAULT_PORT);
    if (inet_pton(AF_INET, IP_SERVER, &server_address.sin_addr) < 0) {
        printf("Errore inet_pton(): %s\n", strerror(errno)); 


        exit(EXIT_FAILURE);
    }



    while (1) { /*Richiesta di input da parte dell'utente*/
        printf("Inserisci un comando (list, get <nome_file>, put <nome_file>): %s", BOLDYELLOW);
        fgets(buffer, MAX_BUFFER_SIZE, stdin);
        buffer[strlen(buffer)-1] = '\0';
        printf("%s", RESET);


        // Invio del comando al server
        sendto(client_socket, buffer, strlen(buffer), 0, (struct sockaddr *)&server_address, sizeof(server_address));


        // Verifica del comando inviato
        if (strcmp(buffer, "list") == 0) {  /*Richiesta della lista dei file disponibili al server*/
            request_file_list(client_socket, server_address);



        } else if (strncmp(buffer, "get ", 4) == 0) {    /*Estrapolazione del nome del file dalla richiesta "get"*/          
            char* filename = buffer + 4;
            request_file(client_socket, server_address, filename);  // Richiesta del file al server



        } else if (strncmp(buffer, "put", 3) == 0) {    /*Estrapolazione del nome del file dalla richiesta "put"*/
            char* filename = buffer + 4;
            send_file(client_socket, server_address, filename); // Invio del file al server



        } else if (strcmp(buffer, "close") == 0) {  /*Chiusura programma e chiusura della socket del client*/
            if (close(client_socket) < 0) {
                perror("Errore nella chiusura della socket");


                exit(EXIT_FAILURE);
            }

            exit(EXIT_SUCCESS);


        } else {    /*Comando non valido, ricevo un messaggio di errore dal server*/
            /*
            bytes_received = recvfrom(client_socket, buffer, sizeof(buffer), 0, NULL, NULL);
            if (bytes_received < 0) {
                perror("Errore nella ricezione del messaggio di errore dal server");
                exit(1);
            }

            buffer[bytes_received] = '\0';
            printf("Errore: %s\n", buffer);
            */
        }
    }



    return EXIT_SUCCESS;
}
