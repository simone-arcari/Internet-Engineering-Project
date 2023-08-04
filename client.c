#include <stdio.h>
#include <errno.h>
#include <netdb.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define MAX_BUFFER_SIZE 1024
#define DEFAULT_PORT 8888
#define IP_SERVER "192.168.1.22"

void request_file_list(int server_socket, struct sockaddr_in server_address) {
    // Implementa la logica per richiedere la lista dei file disponibili al server
    // Utilizza la socket server_socket e l'indirizzo del server server_address
}

void request_file(int server_socket, struct sockaddr_in server_address, char* filename) {
    // Implementa la logica per richiedere un file al server
    // Utilizza la socket server_socket, l'indirizzo del server server_address e il nome del file filename
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
        //perror("Errore");
        printf("Errore socket(): %s\n", strerror(errno)); 


        exit(EXIT_FAILURE);
    }

    // Configurazione dell'indirizzo del server
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(DEFAULT_PORT);
    if (inet_pton(AF_INET, IP_SERVER, &server_address.sin_addr) < 0) {
        //perror("Errore");
        printf("Errore inet_pton(): %s\n", strerror(errno)); 


        exit(EXIT_FAILURE);
    }

    while (1) {
        // Richiesta di input da parte dell'utente
        printf("Inserisci un comando (list, get <nome_file>, put <nome_file>): ");
        fgets(buffer, sizeof(buffer), stdin);

        // Invio del comando al server
        sendto(client_socket, buffer, strlen(buffer), 0, (struct sockaddr *)&server_address, sizeof(server_address));

        // Verifica del comando inviato
        if (strcmp(buffer, "list") == 0) {
            // Richiesta della lista dei file disponibili al server
            request_file_list(client_socket, server_address);
        } else if (strncmp(buffer, "get", 3) == 0) {
            // Estrapolazione del nome del file dalla richiesta "get"
            char* filename = buffer + 4;

            // Richiesta del file al server
            request_file(client_socket, server_address, filename);
        } else if (strncmp(buffer, "put", 3) == 0) {
            // Estrapolazione del nome del file dalla richiesta "put"
            char* filename = buffer + 4;

            // Invio del file al server
            send_file(client_socket, server_address, filename);
        } else {
            // Comando non valido, ricevo un messaggio di errore dal server
            bytes_received = recvfrom(client_socket, buffer, sizeof(buffer), 0, NULL, NULL);
            if (bytes_received < 0) {
                perror("Errore nella ricezione del messaggio di errore dal server");
                exit(1);
            }

            buffer[bytes_received] = '\0';
            printf("Errore: %s\n", buffer);
        }
    }

    // Chiusura della socket del client
    //close(client_socket);

    return 0;
}
