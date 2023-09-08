/*
    gcc server.c -o server -lpthread


    ritorno errori al client 


    scrivere quali funzioni voglio o no la lock prima di essere chiamate


    lock sulle sendto anche nei sendto di close_connection


    wait in close_connection(forse non serve se uno fa bene i mutex)

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
#include "function.h"


#define DEFAULT_PORT 8888


/* Variabili globali */
unsigned long n_clt = 0;  // nummero di client connessi
int server_socket;
struct sockaddr_in client_address;
struct sockaddr_in server_address;
struct sigaction sa;
pthread_mutex_t mutex_rcv;      // mutex per la gestione della ricezione
pthread_mutex_t mutex_snd;      // mutex per la gestione degli invii
list_t client_list;
node_t *pos_client = NULL;


void server_setup() {

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
    if (pthread_mutex_init(&mutex_rcv, NULL) != 0) {
        printf("Errore[%d] pthread_mutex_init(): %s\n", errno , strerror(errno));

       
        exit(EXIT_FAILURE);
    }


    /* Inizializzo il mutex */
    if (pthread_mutex_init(&mutex_snd, NULL) != 0) {
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
}


int main(int __attribute__((unused)) argc, char *argv[]) {
    socklen_t addr_len;
    ssize_t bytes_received;
    char buffer[MAX_BUFFER_SIZE];
    bool check_list;        // per tenere traccia se un client è nella lista dei client connessi
    bool check_connect_msg; // per tenere traccia se il messagio ricevuto è una connect


    /* Incapsula tutto il codice per inizializzare il server e le sue risorse */
    server_setup();


    printf("%sServer in ascolto sulla porta %d...%s\n\n", BOLDGREEN, DEFAULT_PORT, RESET);
    

    while (1) {
        memset(buffer, 0, MAX_BUFFER_SIZE);
        addr_len = sizeof(client_address);


        /* Blocco il mutex prima di leggere dalla socket */
        if (mutex_lock(&mutex_rcv) < 0) {
            printf("Errore[%d] mutex_lock(): %s\n", errno, strerror(errno));
        

            exit(EXIT_FAILURE);
        }
        

        /* Ricezione del comando(di connessione) dal client utilizzando MSG_PEEK per non consumare i dati */
        bytes_received = recvfrom(server_socket, buffer, MAX_BUFFER_SIZE, MSG_PEEK, (struct sockaddr *)&client_address, &addr_len);
        if (bytes_received < 0) {
            printf("Errore[%d] recvfrom(): %s\n", errno, strerror(errno));
            mutex_unlock(&mutex_rcv);


            continue;   // in caso di errore non terminiamo ma torniamo ad scoltare
        }
        

        buffer[bytes_received] = '\0';  // imposto il terminatore di stringa


        /* Verifico che il client sia o no nella lista dei client connessi */
        check_list = is_in_list(client_list, client_address);


        /* Verifico che sia o no una richiesta di connessione */
        check_connect_msg = ( memcmp(buffer, "connect", strlen("connect")) == 0 );


        /* Verifico che il messaggio sia per un thread */
        if (check_connect_msg == false && check_list == true) {
            mutex_unlock(&mutex_rcv);


            continue;   // se non è una richiesta ignoro il messaggio
        }


        /* Consumazione effettiva dei dati dalla socket */
        bytes_received = recvfrom(server_socket, buffer, MAX_BUFFER_SIZE, 0, (struct sockaddr *)&client_address, &addr_len);
        if (bytes_received < 0) {
            printf("Errore[%d] recvfrom(): %s\n", errno, strerror(errno));
            mutex_unlock(&mutex_rcv);


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


            /* Inserimento del client nella lista dei client connessi al server */
            n_clt++;
            pos_client = insert(client_list, pos_client, client_address); // pos viene incremtato dalla funzione stessa ogni volta
            printf("CLIENT INSERITO IN LISTA: %s%ld client connessi%s\n", BOLDBLUE, n_clt, RESET);
            n_clt <= 30 ? print_list(client_list):0;        // stampo la lista solo se non è troppo lunga

            /* Avvio un thread per servire il client */
            if (thread_start(server_socket, client_address, pos_client) < 0) {
                printf("Errore[%d] thread_start(): %s\n", errno, strerror(errno));
                printf("CLIENT RIMOSSO DALLA LISTA: %s%ld client connessi%s\n", BOLDBLUE, n_clt, RESET);
                remove_node(client_list, pos_client);
                n_clt--;
                n_clt <= 30 ? print_list(client_list):0;
                mutex_unlock(&mutex_rcv);


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
        mutex_unlock(&mutex_rcv);
    }

    

    return EXIT_SUCCESS;
}


