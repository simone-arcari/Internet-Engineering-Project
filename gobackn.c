#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <pthread.h>


#define WINDOW_SIZE 4
#define DATA_SIZE 256
#define ACK 0b10101011


// Struttura per passagio dati ai threads
typedef struct {
    int fd;
    const void *buf; 
    size_t msg_size;
    int flags;
    struct sockaddr *addr; 
    socklen_t addr_len;
} Thread_data;



// Struttura per i pacchetti
typedef struct {
    u_int8_t sequence_number;
    u_int8_t data[DATA_SIZE];
    u_int8_t checksum;              // Campo per la checksum
} Packet;


// Struttura per gli ack
typedef struct {
    u_int8_t sequence_number;
    u_int8_t ack_code;
    u_int8_t checksum;              // Campo per la checksum
} Ack;


// Dichiarazioni globali
Packet sender_buffer[WINDOW_SIZE];
int sender_base = 0;
int next_sequence_number = 0;


// Mutex per la sincronizzazione tra i thread
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;


// Funzione per calcolare la checksum dei pacchetti
u_int8_t calculate_checksum(Packet packet) {
    u_int8_t checksum = packet.sequence_number;

    for (size_t i = 0; i < DATA_SIZE; i++) {
        checksum += packet.data[i];

    }
    return checksum;
}


// Funzione per calcolare la checksum degli ack
u_int8_t calculate_ack_checksum(Ack ack) {
    u_int8_t checksum;
    
    checksum = ack.ack_code*(ack.sequence_number+1);
    return checksum;
}


// Funzione per verificare la checksum di un pacchetto
bool verify_checksum(Packet packet) {
    return packet.checksum == calculate_checksum(packet);
}


// Funzione per verificare la checksum di un ack
bool verify_ack_checksum(Ack ack) {
    return ack.checksum == calculate_ack_checksum(ack);
}


// Funzione per inviare pacchetti
void *send_packets(void *arg) {
    Packet packet;
    Thread_data *args = (Thread_data*)arg;
    void *buffer = args->buf;                   // contine il messaggio del livello applicativo da dividere in pacchetti
    void *buffer_ptr = buffer;                  // ad ogni iterazione puntare l'inizio della zona di memoria per formare il pacchetto corrente

    
    size_t number_of_packets = args->msg_size/DATA_SIZE;



    while (true) {
        // Controlla se c'è spazio nel buffer di trasmissione
        if (next_sequence_number < sender_base + WINDOW_SIZE) {

            


            packet.sequence_number = next_sequence_number;
            memcpy(packet.data, buffer_ptr, ); 
            packet.checksum = calculate_checksum(packet); // Calcola la checksum

            // Aggiungi il pacchetto al buffer di trasmissione
            sender_buffer[next_sequence_number % WINDOW_SIZE] = packet;

            // Invia il pacchetto
            sendto(args->fd, &packet, sizeof(packet), 0, args->addr, args->addr_len);

            // Avanza il numero di sequenza e il puntatore al buffer
            next_sequence_number++;
            buffer_ptr += sizeof(packet.data)*next_sequence_number;

        }
    }
    return NULL;
}

// Funzione per ricevere un ACK
void *receive_acks(void *arg) {
    Ack received_ack;
    Thread_data *args = (Thread_data*)arg;
 
    while (true) {
        recvfrom(args->fd, &received_ack, sizeof(received_ack), 0, args->addr, args->addr_len);

        // Verifica la checksum dell'ACK ricevuto
        if (verify_ack_checksum(received_ack)) {

            // ACK ricevuto correttamente, aggiornare il base sender
            if (received_ack.sequence_number >= sender_base) {
                sender_base = received_ack.sequence_number + 1;
            }

        } else {
            // La checksum dell'ACK non corrisponde, ignora l'ACK o gestisci l'errore
        }

    }
    return NULL;
}



void send_msg(int fd, const void *buf, size_t n, int flags, struct sockaddr *addr, socklen_t addr_len) {
    pthread_t send_thread, receive_thread;
    Thread_data args = {fd, buf, n , flags, addr, addr_len};






    // Creazione dei thread per l'invio dei pacchetti e la ricezione degli ACK
    pthread_create(&send_thread, NULL, send_packets, &args);
    pthread_create(&receive_thread, NULL, receive_acks, &args);

    // Attendere la terminazione dei thread figli
    pthread_join(send_thread, NULL);
    pthread_join(receive_thread, NULL);
}

