/**
 * @file gobackn.c
 * @brief Breve descrizione del file.
 *
 * 
 *
 * @authors Simone Arcari, Valeria Villano
 * @date 2023-09-13 (nel formato YYYY-MM-DD)
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
#include "gobackn.h"
#include "function.h"

//#define SERVER


// Dichiarazioni globali
#ifdef SERVER
    extern pthread_mutex_t mutex_rcv;              // mutex per la gestione della ricezione
    extern pthread_mutex_t mutex_snd;              // mutex per la gestione degli invii
#endif


// Funzione per calcolare la checksum dei pacchetti
u_int8_t calculate_checksum(Packet packet) {
    u_int8_t checksum = packet.sequence_number;

    for (size_t i = 0; i < DATA_SIZE; i++) {
        checksum += packet.data[i];

    }

    checksum += packet.data_size;
    checksum += packet.last_pck;

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
    return true;
    return packet.checksum == calculate_checksum(packet);
}

// Funzione per verificare la checksum di un ack
bool verify_ack_checksum(Ack ack) {
    return true;
    return ack.checksum == calculate_ack_checksum(ack);
}

// Funzione per inviare pacchetti
void *send_packets(void *arg) {
    Thread_data *args = (Thread_data*)arg;
    pthread_mutex_t *mutex = args->mutex;

    int socket = args->socket;
    struct sockaddr *addr = args->addr; 

    Packet packet;
    void *buffer_ptr = (void*)args->buffer;     // ad ogni iterazione punta l'inizio della zona di memoria per formare il pacchetto corrente

    size_t data_size;                           // dimensione in byte dei dati effettivamente trasmessi dal pachhetto corrente 
    size_t msg_size = args->msg_size;           // dimensione in byte del messaggio

    int number_of_full_packets;
    int next_sequence_number = 0;
    size_t residue_packet_size;


    mutex_lock(mutex);


    number_of_full_packets = msg_size/DATA_SIZE;      // calcola il numero di pacchetti che avranno taglia massima ovvero DATA_SIZE
    residue_packet_size = msg_size%DATA_SIZE;         // calcola la taglia del pachetto residuo che tipicamente è minore di DATA_SIZE


    // Imposto i timer con sequence_number -1 per indicare che sono inizialmente disattivati
    for (int i = 0; i < WINDOW_SIZE; i++) {    
        args->timers[i].sequence_number = -1;

    }


    // Tengo traccia di quanti pacchetti ho effettivametne (si conta da zero)
    if (residue_packet_size == 0) {
        *(args->last_packet) = number_of_full_packets-1;  // se la divisione è esatta 

    } else {
        *(args->last_packet) = number_of_full_packets;   // se abbiamo una rimanenza di byte aggiungiamo un ultimo pacchetto con campo data_size opportuno

    }


    mutex_unlock(mutex);

    
    while (true) {
        mutex_lock(mutex);

        // Controlla se c'è spazio nel buffer di trasmissione (Finestra di Invio)
        if (next_sequence_number < *(args->sender_base) + WINDOW_SIZE && next_sequence_number <= *(args->last_packet)) {

            // assegno la corretta dimensione al pacchetto corrente
            if (next_sequence_number == *(args->last_packet) && residue_packet_size != 0) {
                data_size = residue_packet_size;    // ultimo pacchetto con campo data_size opportuno

            } else {
                data_size = DATA_SIZE;              // taglia standard

            }

            // crea il pacchetto
            packet.sequence_number = next_sequence_number;                          // imposta il seq_num del pacchetto
            packet.data_size = data_size;                                           // imposta la taglia effettiva dei byte utili di data
            memcpy(packet.data, buffer_ptr, data_size);                             // assegna il campo data
            packet.checksum = calculate_checksum(packet);                           // Calcola la checksum
            packet.last_pck = (next_sequence_number == *(args->last_packet)) ? IS_LAST_PCK:NOT_LAST_PCK;   // indica se questo è lultimo pacchetto della trasmissione
printf("packet.last_pck: %d\n", packet.last_pck);

            // Aggiungi il pacchetto al buffer di ritrasmissione
            args->sender_buffer[next_sequence_number % WINDOW_SIZE] = packet;


            mutex_unlock(mutex);


            // Invia il pacchetto
#ifdef SERVER
            mutex_lock(&mutex_snd);
            sendto(socket, &packet, sizeof(packet), 0, addr, sizeof(*addr));
            mutex_unlock(&mutex_snd);

#else
            printf("pacchetto: %d\n", packet.sequence_number);
            sendto(socket, &packet, sizeof(packet), 0, addr, sizeof(*addr));
#endif


            mutex_lock(mutex);


            // Avvia il timer per il pacchetto appena inviato
            args->timers[packet.sequence_number % WINDOW_SIZE].sequence_number = packet.sequence_number;
            args->timers[packet.sequence_number % WINDOW_SIZE].start_time = time(NULL);

            // Avanza il numero di sequenza e il puntatore al buffer
            next_sequence_number++;
            buffer_ptr += packet.data_size;      // tutti i calcoli sono riferiti all'unità intesa in byte
        }


        // Tutti i pacchetti inviati e riscontrati correttamente
        if (*(args->last_packet_acked) == *(args->last_packet)) {
            mutex_unlock(mutex);
            printf("SND PCK FINITO\n");

            return NULL;
        }

        mutex_unlock(mutex);

    }
}

/*
    Funzione per ricevere gli ack dei pacchetti inviati
    NOTA: questa funzione va chiamata senza aver effettuato una lock prima (la fa la funzione stessa al suo interno)
*/
void *receive_acks(void *arg) {
    Thread_data *args = (Thread_data*)arg;
    pthread_mutex_t *mutex = args->mutex;

    int socket = args->socket;
    Packet packet;
    struct sockaddr addr_recived;
    struct sockaddr *addr = args->addr;
    socklen_t addr_len;

    Ack received_ack;
    time_t current_time;
    int sequence_number_to_retransmit;

    //sleep(1);


    while (true) {
        mutex_lock(mutex);

        // Controlla se il timer è scaduto per un pacchetto
        for (int i = 0; i < WINDOW_SIZE; i++) {

            if (args->timers[i].sequence_number != -1) {  // solo se il timer è attivo
                current_time = time(NULL);

                // Timer scaduto per il pacchetto, ritransmettere il pacchetto
                if (current_time - args->timers[i].start_time >= TIMEOUT_SECONDS) {
                    
                    sequence_number_to_retransmit = args->timers[i].sequence_number;
                    packet = args->sender_buffer[sequence_number_to_retransmit % WINDOW_SIZE];

                    // Effettua la ritrasmissione del pacchetto con sequence_number_to_retransmit
#ifdef SERVER
                    mutex_lock(&mutex_snd);
                    sendto(socket, &packet, sizeof(packet), 0, addr, sizeof(*addr));
                    mutex_unlock(&mutex_snd);
#else
                    sendto(socket, &packet, sizeof(packet), 0, addr, sizeof(*addr));
#endif
                }
            }
        }


        mutex_unlock(mutex);


        // Ricezione Ack
#ifdef SERVER
        mutex_lock(&mutex_rcv);
        recvfrom(socket, &received_ack, sizeof(received_ack), MSG_PEEK, (struct sockaddr *)&addr_recived, &addr_len); // non sto consumando i dati

        // Verifico se l'indirizzo IP e la porta del mittente non corrispondono a quelli previsti
        if (memcmp(addr, &addr_recived, addr_len) != 0) {
            mutex_unlock(&mutex_rcv); // in questo modo il vero destinatario ha la possibilità di consumare i dati
            mutex_lock(mutex);

            continue; // in caso non corrispondano ignoro il messaggio e ritento fino allo scadere del timer
        }

        recvfrom(socket, &received_ack, sizeof(received_ack), 0, (struct sockaddr *)&addr_recived, &addr_len); // ho consumato i dati
        mutex_unlock(&mutex_rcv);
#else
        printf("ack: %d\n", received_ack.sequence_number);
        recvfrom(socket, &received_ack, sizeof(received_ack), 0, NULL, NULL);
#endif
        // Fine ricezione Ack


        mutex_lock(mutex);


        // Verifica la checksum dell'ACK ricevuto
        if (verify_ack_checksum(received_ack)) {

            // ACK ricevuto correttamente, aggiornare il base sender e azzerare il timer
            if (received_ack.sequence_number >= *(args->last_packet_acked)) {

                // Azzera il timer per il pacchetto riscontrato e quelli con seq_num inferiori (Ack Cumulativi)
                for (int i = 0; i < WINDOW_SIZE; i++) {
                    if (args->timers[i].sequence_number <= received_ack.sequence_number) {
                        args->timers[i].sequence_number = -1; // Segna il timer come non attivo

                    }
                }


                *(args->last_packet_acked) = received_ack.sequence_number;
                *(args->sender_base) = received_ack.sequence_number + 1;




                // Aggiorna il base sender sempre se la finestra non è arrivata alla fine
                if (*(args->last_packet_acked) <= *(args->last_packet)-WINDOW_SIZE) {

                }
                
            }

        } else {
            // La checksum dell'ACK non corrisponde, ignora l'ACK

        }


        // Tutti i pacchetti inviati e riscontrati correttamente
        if (*(args->last_packet_acked) == *(args->last_packet)) {
            mutex_unlock(mutex);
            printf("RECV ACKS FINITO\n");
            
            return NULL;
        }

        mutex_unlock(mutex);

    }
}

/*
    ritorna il seq_num dell'ultima entry contigua alle precedenti
    ovvero se: [true , true , true , false , false , true , true] ritonra seq_num = 2 (si conta da zero)
*/
int get_last(bool received_packet[256]) {

    if (received_packet[0] == false) {

        return -1;
    }

    for (int seq_num=1; seq_num<256; seq_num++) {
        if (received_packet[seq_num] == false) {

            return seq_num - 1;
        }
        
    }

    return 255;
}

/*
    Se è noto il seq_num dell'ultimo pacchetto si può verificare che tutte le entry siano state ricevute
*/
bool check_end_transmission(bool received_packet[256], int last_pck) {

    if (last_pck < 0) {

        return false;   // non è ancora noto quanti pacchetti siano effettivamente
    }


    if (get_last(received_packet) == last_pck) {


        printf("last_pck: %d         ", last_pck);
        printf("get_last(received_packet): %d\n", get_last(received_packet));



        return true;
    } else {

        return false;
    }
}

void assembly_msg(Packet receiver_buffer[256], int last_pck, void *buffer) {
    void *buffer_ptr = (void*)buffer;
    Packet packet;
    size_t size;
    
    for (int seq_num = 0; seq_num <= last_pck; seq_num++) {

        packet = receiver_buffer[seq_num];
        size = packet.data_size;

        memcpy(buffer_ptr, &packet.data, size);
        buffer_ptr += size;

    }

}

/*
    NOTA: il massimo numero di byte che può essere gestito dipende dal massimo numero di sequence_number rappresentabile;
    in particolare abbiamo 1 byte ovvero 8 bit quindi 256 possibili sequence_number distinti, ogni pacchetto dei 256 
    possibili può portare DATA_SIZE byte, ergo max_byte = 256*DATA_SIZE, in caso di dimesioni superiori sarà compito
    del livello applicativo gestire la segmetazione dei messaggi per il mittente e il riassemblaggio per il destinatario
*/
void send_msg(int socket, void *buffer, size_t msg_size, struct sockaddr *addr) {
    pthread_t send_thread, receive_thread;
    pthread_mutex_t mutex;


    Packet sender_buffer[WINDOW_SIZE];
    int sender_base = 0;
    int last_packet_acked = -1;
    int last_packet = -2;
    Timer timers[WINDOW_SIZE];


    // Inizializzo il mutex per gestrire l'accesso alle variabili condivise 
    if (pthread_mutex_init(&mutex, NULL) != 0) {
        printf("Errore[%d] pthread_mutex_init(): %s\n", errno , strerror(errno));

       
        exit(EXIT_FAILURE);
    }


    Thread_data args = {socket, buffer, msg_size, addr, sender_buffer, &sender_base, &last_packet_acked, &last_packet, timers, &mutex};


    // Creazione dei thread per l'invio dei pacchetti e la ricezione degli ACK
    pthread_create(&receive_thread, NULL, receive_acks, &args);
    pthread_create(&send_thread, NULL, send_packets, &args);


    // Attendere la terminazione del thread figlio
    pthread_join(send_thread, NULL);
    printf("FINE THREADS\n");


    return;
}

/*
    Questa funzione si occuppa di ricevere i pacchetti e assemblarli correttamente in base al loro numero di sequenza e inviare gli ack ai pacchetti

    NOTA: il massimo numero di byte che può essere gestito dipende dal massimo numero di sequence_number rappresentabile;
    in particolare abbiamo 1 byte ovvero 8 bit quindi 256 possibili sequence_number distinti, ogni pacchetto dei 256 
    possibili può portare DATA_SIZE byte, ergo max_byte = 256*DATA_SIZE, in caso di dimesioni superiori sarà compito
    del livello applicativo gestire la segmetazione dei messaggi per il mittente e il riassemblaggio per il destinatario
*/
void rcv_msg(int socket, void *buffer, struct sockaddr *addr, socklen_t *addr_len_ptr) {
    Packet packet;
    Ack ack;

    Packet receiver_buffer[256];
    bool received_packet[256];
    
    struct sockaddr addr_recived;
    socklen_t addr_len;

    int current;
    int last_packet_acked = -1;
    int last_pck = -1;


    while (true) {

        // Ricezione Pacchetto
#ifdef SERVER
        mutex_lock(&mutex_rcv);
        recvfrom(socket, &packet, sizeof(packet), MSG_PEEK, (struct sockaddr *)&addr_recived, addr_len_ptr); // non sto consumando i dati
        addr_len = *addr_len_ptr;

        // Verifico se l'indirizzo IP e la porta del mittente non corrispondono a quelli previsti
        if (memcmp(addr, &addr_recived, addr_len) != 0) {
            mutex_unlock(&mutex_rcv); // in questo modo il vero destinatario ha la possibilità di consumare i dati

            continue; // in caso non corrispondano ignoro il messaggio e ritento fino allo scadere del timer
        }

        recvfrom(socket, &packet, sizeof(packet), 0, (struct sockaddr *)&addr_recived, addr_len_ptr); // ho consumato i dati
        mutex_unlock(&mutex_rcv);
#else
        printf("pacchetto: %d\n", packet.sequence_number);
        recvfrom(socket, &packet, sizeof(packet), 0, NULL, NULL); // ho consumato i dati


#endif
        // Fine ricezione pacchetto


        // Verifica la checksum del pacchetto ricevuto
        if (verify_checksum(packet)) {

            // Pacchetto ricevuto correttamente, aggiornare il receiver_buffer 
            receiver_buffer[packet.sequence_number] = packet;
            last_pck = (packet.last_pck == IS_LAST_PCK) ? packet.sequence_number:-1;
            printf("last_pck: %d\n", last_pck);
            printf("packet.last_pck: %d\n", packet.last_pck);




            // Da qui si implemeta la funzionalità che permette al mittente di sfruttare gli ack cumulativi
            received_packet[packet.sequence_number] = true;     // marco la rispettiva entry come ricevuta ed archiviata
            current = get_last(received_packet);

            if (current > last_packet_acked) {
                last_packet_acked = current;


                // crea il pacchetto di ack
                ack.sequence_number = last_packet_acked;
                ack.ack_code = ACK;
                ack.checksum = calculate_ack_checksum(ack);

                // Invia il pacchetto
#ifdef SERVER
                mutex_lock(&mutex_snd);
                sendto(socket, &ack, sizeof(ack), 0, addr, sizeof(*addr));
                mutex_unlock(&mutex_snd);
#else
                printf("ack: %d\n", ack.sequence_number);
                sendto(socket, &ack, sizeof(ack), 0, addr, sizeof(*addr));
#endif

            }

        } else {
            // La checksum del pacchetto non corrisponde, ignora il pacchetto (il mittente lo ritrasmetterà allo scadere del timer)

        }


        // Verifica che tutti i pacchetti siano stati ricevuti
        if (check_end_transmission(received_packet, last_pck)) {

            // Assembla il messaggio
            assembly_msg(receiver_buffer, last_pck, buffer);

printf("msg: %s\n", (char*)buffer);
printf("FINE\n");
           
            
            return;
        }
    }
}





