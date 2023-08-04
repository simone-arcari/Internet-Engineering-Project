#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>


#define MAX_BUFFER_SIZE 1024
#define DEFAULT_PORT 8888

typedef struct {
    int client_socket;
    struct sockaddr_in client_address;
} ClientInfo;

void send_file_list(int client_socket, struct sockaddr_in client_address) {
    // Implementa la logica per inviare la lista dei file disponibili al client
    // Utilizza la socket client_socket e l'indirizzo del client client_address
}
void send_file(int client_socket, struct sockaddr_in client_address, char* filename) {
    // Implementa la logica per inviare un file al client
    // Utilizza la socket client_socket, l'indirizzo del client client_address e il nome del file filename
}
void receive_file(int client_socket, struct sockaddr_in client_address, char* filename) {
    // Implementa la logica per ricevere un file dal client
    // Utilizza la socket client_socket, l'indirizzo del client client_address e il nome del file filename
}
void *handle_client(void *arg) {
    ClientInfo *client_info = (ClientInfo *)arg;
    int client_socket = client_info->client_socket;
    struct sockaddr_in client_address = client_info->client_address;
    char buffer[MAX_BUFFER_SIZE];
    
    // Gestisci la richiesta del client

    // Verifica del comando ricevuto
        if (strcmp(buffer, "list") == 0) {
            // Invio della lista dei file disponibili al client
            send_file_list(client_socket, client_address);
        } else if (strncmp(buffer, "get", 3) == 0) {
            // Estrapolazione del nome del file dalla richiesta "get"
            char* filename = buffer + 4;

            // Invio del file al client
            send_file(client_socket, client_address, filename);
        } else if (strncmp(buffer, "put", 3) == 0) {
            // Estrapolazione del nome del file dalla richiesta "put"
            char* filename = buffer + 4;

            // Ricezione del file dal client
            receive_file(client_socket, client_address, filename);
        } else {
            // Comando non valido, invio un messaggio di errore al client
            strcpy(buffer, "Comando non valido");
            sendto(server_socket, buffer, strlen(buffer), 0, (struct sockaddr *)&client_address, sizeof(client_address));
        }
    
    // Chiudi la socket del client
    close(client_socket);
    free(client_info);
    
    return NULL;
}

int main() {
    int server_socket, client_socket, addr_len, bytes_received;
    struct sockaddr_in server_address, client_address;
    char buffer[MAX_BUFFER_SIZE];
    pthread_t tid;
    
    // Creazione della socket del server
    server_socket = socket(AF_INET, SOCK_DGRAM,  IPPROTO_UDP);
    if (server_socket < 0) {
        //perror("Errore");
        printf("Errore socket(): %s\n", strerror(errno));   // Controllare variabile globale errno qui
        

        exit(1);
    }
    
    // Configurazione dell'indirizzo del server
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(DEFAULT_PORT);
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);     // il server accetta richieste su ogni interfaccia di rete 
    
    // Associazione dell'indirizzo del server alla socket
    if (bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {   
        //perror("Errore");
        printf("Errore bind(): %s\n", strerror(errno));
        

        exit(1);
    }
    
    printf("Server in ascolto sulla porta %d\n", DEFAULT_PORT);
    
    while (1) {
        addr_len = sizeof(client_address);
        
        // Ricezione del comando dal client
        bytes_received = recvfrom(server_socket, buffer, sizeof(buffer), 0, (struct sockaddr *)&client_address, &addr_len);
        if (bytes_received < 0) {
            perror("Errore nella ricezione del messaggio dal client");

            // Controllare variabileglobale errno qui

            exit(1);
        }
        
        buffer[bytes_received] = '\0';
        
        // Creazione di una struttura ClientInfo per passare le informazioni del client al thread
        ClientInfo *client_info = (ClientInfo *)malloc(sizeof(ClientInfo));
        client_info->client_socket = client_socket;
        client_info->client_address = client_address;
        
        // Creazione di un nuovo thread per gestire il client
        if (pthread_create(&tid, NULL, handle_client, (void *)client_info) != 0) {
            perror("Errore nella creazione del thread");

            // Controllare variabileglobale errno qui

            exit(1);
        }
        
        // Detach del thread per permettere la terminazione automatica
        pthread_detach(tid);
    }
    
    // Chiusura della socket del server
    close(server_socket);
    
    return 0;
}
