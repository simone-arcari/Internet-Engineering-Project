/**
 * @file gobackn.c
 * @brief Breve descrizione del file.
 *
 * 
 *
 * @authors Simone Arcari, Valeria Villano
 * @date 2023-09-13 (nel formato YYYY-MM-DD)
 * 
 * 



gdb ./server
layout next    invio invio invio
b nome funzione
r (runna)
n (step)

 *          
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
#include "transport_protocol.h"
#include "function.h"


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
    checksum += packet.last_pck_flag;

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
bool check_end_transmission(bool received_packet[256], int last_packet) {

    if (last_packet < 0) {

        return false;   // non è ancora noto quanti pacchetti siano effettivamente
    }


    if (get_last(received_packet) == last_packet) {

        return true;
    } else {

        return false;
    }
}

ssize_t assembly_msg(Packet receiver_buffer[256], int last_pck, void *buffer) {
    void *buffer_ptr = (void*)buffer;
    Packet packet;
    size_t size;
    ssize_t bytes_received = 0;
    
    for (int seq_num = 0; seq_num <= last_pck; seq_num++) {
        

        packet = receiver_buffer[seq_num];
        size = packet.data_size;

        memcpy(buffer_ptr, &packet.data, size);
        buffer_ptr += size;

        bytes_received += size;
    }

    return bytes_received;
}

/*
    Funzione per inviare pacchetti

    NOTA: il massimo numero di byte che può essere gestito dipende dal massimo numero di sequence_number rappresentabile;
    in particolare abbiamo 1 byte ovvero 8 bit quindi 256 possibili sequence_number distinti, ogni pacchetto dei 256 
    possibili può portare DATA_SIZE byte, ergo max_byte = 256*DATA_SIZE, in caso di dimesioni superiori sarà compito
    del livello applicativo gestire la segmetazione dei messaggi per il mittente e il riassemblaggio per il destinatario
*/
int send_msg(int socket, void *buffer, size_t msg_size, struct sockaddr *addr) {

    Packet packet;
    Packet sender_buffer[WINDOW_SIZE];

    int sender_base = 0;
    int last_packet_acked = -1;
    int last_packet = -2;
    int number_of_full_packets;
    int next_sequence_number = 0;
    size_t data_size;                           // dimensione in byte dei dati effettivamente trasmessi dal pachhetto corrente 
    size_t residue_packet_size;

    Timer timers[WINDOW_SIZE];
    bool max_timeout_flag = false;
    
    void *buffer_ptr = buffer;     // ad ogni iterazione punta l'inizio della zona di memoria per formare il pacchetto corrente

    pthread_mutex_t mutex;
    pthread_t receive_thread, timeout_thread;



    number_of_full_packets = msg_size/DATA_SIZE;      // calcola il numero di pacchetti che avranno taglia massima ovvero DATA_SIZE
    residue_packet_size = msg_size%DATA_SIZE;         // calcola la taglia del pachetto residuo che tipicamente è minore di DATA_SIZE

    // Imposto i timer con sequence_number -1 per indicare che sono inizialmente disattivati
    for (int i = 0; i < WINDOW_SIZE; i++) {    
        timers[i].sequence_number = -1;
        timers[i].num_timeout_fail = 0;

    }

    // Tengo traccia di quanti pacchetti ho effettivametne (si conta da zero)
    if (msg_size == 0) {
        last_packet = 0;                           // se si vuole inviare un pacchetto vuoto

    } else if (residue_packet_size == 0) {
        last_packet = number_of_full_packets-1;    // se la divisione è esatta 

    } else {
        last_packet = number_of_full_packets;      // se abbiamo una rimanenza di byte aggiungiamo un ultimo pacchetto con campo data_size opportuno

    }

    Thread_data args = {socket, buffer, msg_size, addr, sender_buffer, &sender_base, &last_packet_acked, last_packet, timers, &max_timeout_flag, &mutex};

    // Inizializzo il mutex per gestrire l'accesso alle variabili condivise 
    if (pthread_mutex_init(&mutex, NULL) != 0) {
        printf("Errore[%d] pthread_mutex_init(): %s\n", errno , strerror(errno));

       
        return EXIT_FAILURE;
    }


    while (true) {
        mutex_lock(&mutex);

        // Controlla se c'è spazio nel buffer di trasmissione (Finestra di Invio)
        if (next_sequence_number < sender_base + WINDOW_SIZE && next_sequence_number <= last_packet) {

            // assegno la corretta dimensione al pacchetto corrente
            if (msg_size == 0) {
                data_size = 0;  // caso in cui si vuole inviare un pacchetto vuoto

            } else if (next_sequence_number == last_packet && residue_packet_size != 0) {
                data_size = residue_packet_size;    // ultimo pacchetto con campo data_size opportuno

            } else {
                data_size = DATA_SIZE;              // taglia standard

            }


            // crea il pacchetto
            packet.sequence_number = next_sequence_number;                                        // imposta il seq_num del pacchetto
            packet.data_size = data_size;                                                         // imposta la taglia effettiva dei byte utili di data
            memcpy(packet.data, buffer_ptr, data_size);                                           // assegna il campo data
            packet.last_pck_flag = (next_sequence_number == last_packet) ? true:false;   // indica se questo è lultimo pacchetto della trasmissione
            packet.checksum = calculate_checksum(packet);                                         // Calcola la checksum


            // Aggiungi il pacchetto al buffer di ritrasmissione
            sender_buffer[next_sequence_number % WINDOW_SIZE] = packet;


            // Invia il pacchetto
#ifdef SERVER
            //mutex_lock(&mutex_snd);
            sendto(socket, &packet, sizeof(packet), 0, addr, sizeof(*addr));
            //mutex_unlock(&mutex_snd);

#else
            sendto(socket, &packet, sizeof(packet), 0, addr, sizeof(*addr));
#endif
            printf("inviato pacchetto: %d\n", packet.sequence_number);


            // Avvia il timer per il pacchetto appena inviato
            timers[packet.sequence_number % WINDOW_SIZE].sequence_number = packet.sequence_number;
            timers[packet.sequence_number % WINDOW_SIZE].start_time = time(NULL);
            timers[packet.sequence_number % WINDOW_SIZE].num_timeout_fail = 0;


            // Dopo l'invio del primo pacchetto avviamo il thread per la ricezione degli ack e per i timeout
            if (packet.sequence_number == 0) {
                pthread_create(&receive_thread, NULL, receive_acks, &args);
                pthread_create(&timeout_thread, NULL, timeout_acks, &args);
            }


            // Avanza il numero di sequenza e il puntatore al buffer
            next_sequence_number++;
            buffer_ptr += packet.data_size;      // tutti i calcoli sono riferiti all'unità intesa in byte
        }


        // Se avvengono troppi timeout per uno stesso pacchetto
        if (max_timeout_flag == true) {
            mutex_unlock(&mutex);
    
            // Aspetta la terminazione dei thread figli
            pthread_join(timeout_thread, NULL);
            pthread_join(receive_thread, NULL);

            // Libera risorse
            pthread_mutex_destroy(&mutex);

            return EXIT_ERROR;
        }


        // Se tutti i pacchetti vengono inviati e riscontrati correttamente
        if (last_packet_acked == last_packet) {
            mutex_unlock(&mutex);

            // Aspetta la terminazione dei thread figli
            pthread_join(receive_thread, NULL);
            pthread_join(timeout_thread, NULL);

            // Libera risorse
            pthread_mutex_destroy(&mutex);

            return EXIT_SUCCESS;
        }

        mutex_unlock(&mutex);

    }
}

// Thread per ricevere gli ack dei pacchetti inviati
void *receive_acks(void *arg) {
    Thread_data *args = (Thread_data*)arg;
    pthread_mutex_t *mutex = args->mutex;

    int socket = args->socket;
    struct sockaddr addr_recived;
    socklen_t addr_len;

    int last_packet = args->last_packet;

    Ack received_ack;

    bool boolean_variable;
    double random_value;
    double p = PROBABILITY;                    // probabilità perdità (tra 0 e 1)


    // Inizializza il generatore di numeri casuali con il tempo attuale come seme
    srand(time(NULL));


    while (true) {
 
        // Ricezione Ack
#ifdef SERVER
        mutex_lock(&mutex_rcv);
        recvfrom(socket, &received_ack, sizeof(received_ack), MSG_PEEK, (struct sockaddr *)&addr_recived, &addr_len); // non sto consumando i dati
        
        // Verifico se l'indirizzo IP e la porta del mittente non corrispondono a quelli previsti
        if (memcmp(args->addr, &addr_recived, addr_len) != 0) {
            mutex_unlock(&mutex_rcv); // in questo modo il vero destinatario ha la possibilità di consumare i dati
            mutex_unlock(mutex);

            continue; // in caso non corrispondano ignoro il messaggio e ritento fino allo scadere del timer
        }

        recvfrom(socket, &received_ack, sizeof(received_ack), 0, (struct sockaddr *)&addr_recived, &addr_len); // ho consumato i dati
        mutex_unlock(&mutex_rcv);
#else
        recvfrom(socket, &received_ack, sizeof(received_ack), 0, (struct sockaddr *)&addr_recived, &addr_len);
#endif
        // Fine ricezione Ack


        mutex_lock(mutex);


        // Genera un numero casuale tra 0 e 1
        random_value = (double)rand() / RAND_MAX;
        boolean_variable = (random_value > p);


        // Verifica la checksum dell'ACK ricevuto
        if (verify_ack_checksum(received_ack) && boolean_variable) {
            printf("ricevuto ack: %d\n", received_ack.sequence_number);

            // ACK ricevuto correttamente, aggiornare il base sender e azzerare il timer
            if (received_ack.sequence_number > *(args->last_packet_acked)) {

                // Azzera il timer per il pacchetto riscontrato e quelli con seq_num inferiori (Ack Cumulativi)
                for (int i = 0; i < WINDOW_SIZE; i++) {
                    if (args->timers[i].sequence_number <= received_ack.sequence_number) {
                        args->timers[i].sequence_number = -1; // Segna il timer come non attivo
                        args->timers[i].num_timeout_fail = 0;

                    }
                }


                *(args->last_packet_acked) = received_ack.sequence_number;

                // Aggiorna il base sender sempre se la finestra non è arrivata alla fine
                if (*(args->last_packet_acked) <= last_packet-WINDOW_SIZE) {
                    *(args->sender_base) = received_ack.sequence_number + 1;

                }
                
            }

        } else {
            // La checksum dell'ACK non corrisponde, ignora l'ACK oppure simulazione perdita ack

            if (p > 0) {
                printf("%sack perso: %d%s\n", RED, received_ack.sequence_number, RESET);
            }

        }


        // Se tutti i pacchetti vengono inviati e riscontrati correttamente
        if (*(args->last_packet_acked) == last_packet) {
            mutex_unlock(mutex);
            
            return NULL;
        }

        // Se avvengono troppi timeout per uno stesso pacchetto
        if (*(args->max_timeout_flag) == true) {
            mutex_unlock(mutex);
            
            return NULL;
        }

        mutex_unlock(mutex);

    }
}

// Thread per la gestione dei timeot degli ack da ricevere
void *timeout_acks(void *arg) {

    Thread_data *args = (Thread_data*)arg;
    pthread_mutex_t *mutex = args->mutex;

    int socket = args->socket;
    Packet packet;
    struct sockaddr *addr = args->addr;

    int last_packet = args->last_packet;

    time_t current_time;
    int sequence_number_to_retransmit;


    while (true) {
        mutex_lock(mutex);

        // Controlla se il timer è scaduto per un pacchetto
        for (int i = 0; i < WINDOW_SIZE; i++) {

            if (args->timers[i].sequence_number != -1) {  // solo se il timer è attivo

                current_time = time(NULL);

                // Timer scaduto per il pacchetto, ritransmettere il pacchetto
                if (current_time - args->timers[i].start_time >= TIMEOUT_ACKS) {
                    
                    sequence_number_to_retransmit = args->timers[i].sequence_number;
                    packet = args->sender_buffer[sequence_number_to_retransmit % WINDOW_SIZE];

                    // Effettua la ritrasmissione del pacchetto con sequence_number_to_retransmit
#ifdef SERVER
                    //mutex_lock(&mutex_snd);
                    sendto(socket, &packet, sizeof(packet), 0, addr, sizeof(*addr));
                    //mutex_unlock(&mutex_snd);
#else
                    sendto(socket, &packet, sizeof(packet), 0, addr, sizeof(*addr));
#endif
                    printf("rinviato pacchetto: %d\n", packet.sequence_number);

                    // Avvia il timer per il pacchetto appena rinviato
                    args->timers[packet.sequence_number % WINDOW_SIZE].sequence_number = packet.sequence_number;
                    args->timers[packet.sequence_number % WINDOW_SIZE].start_time = time(NULL);
                    args->timers[packet.sequence_number % WINDOW_SIZE].num_timeout_fail += 1;

                    if (args->timers[packet.sequence_number % WINDOW_SIZE].num_timeout_fail >= MAX_TIMEOUT_FAIL) {
                        *(args->max_timeout_flag) = true;
                        mutex_unlock(mutex);

                        return NULL;
                    }
                }
            }
        }


        // Se tutti i pacchetti vengono inviati e riscontrati correttamente
        if (*(args->last_packet_acked) == last_packet) {
            mutex_unlock(mutex);

            return NULL;
        }


        mutex_unlock(mutex);
    }
}


/*
    Questa funzione si occuppa di ricevere i pacchetti e assemblarli correttamente in base al loro numero di sequenza e inviare gli ack ai pacchetti

    NOTA: il massimo numero di byte che può essere gestito dipende dal massimo numero di sequence_number rappresentabile;
    in particolare abbiamo 1 byte ovvero 8 bit quindi 256 possibili sequence_number distinti, ogni pacchetto dei 256 
    possibili può portare DATA_SIZE byte, ergo max_byte = 256*DATA_SIZE, in caso di dimesioni superiori sarà compito
    del livello applicativo gestire la segmetazione dei messaggi per il mittente e il riassemblaggio per il destinatario
*/
ssize_t rcv_msg(int socket, void *buffer, struct sockaddr *addr) {
    Packet packet;
    Ack ack;

    Packet receiver_buffer[256];
    bool received_packet_flag[256];
    
    struct sockaddr addr_recived;
    socklen_t addr_len;
    ssize_t bytes_received = 0;

    int current;
    int last_packet_acked = -1;
    int last_packet = -1;

    time_t start_time = time(NULL);     // tempo di inizio del timer
    time_t current_time;
    time_t elapsed_time;

    bool boolean_variable;
    double random_value;
    double p = PROBABILITY;             // probabilità perdità (tra 0 e 1)


    memset(receiver_buffer, 0, sizeof(receiver_buffer));
    memset(received_packet_flag, 0, sizeof(received_packet_flag));


    // Inizializza il generatore di numeri casuali con il tempo attuale come seme
    srand(time(NULL));


    while (true) {

        // Calcolo il tempo trascorso 
        current_time = time(NULL);
        elapsed_time = current_time - start_time;

        if (elapsed_time >= TIMEOUT_RCV) {

            //return EXIT_ERROR;
        }
        

        // Ricezione Pacchetto
#ifdef SERVER
        mutex_lock(&mutex_rcv);
        recvfrom(socket, &packet, sizeof(packet), MSG_PEEK, (struct sockaddr *)&addr_recived, &addr_len); // non sto consumando i dati

        // Verifico se l'indirizzo IP e la porta del mittente non corrispondono a quelli previsti
        if (memcmp(addr, &addr_recived, addr_len) != 0) {
            mutex_unlock(&mutex_rcv); // in questo modo il vero destinatario ha la possibilità di consumare i dati


            continue; // in caso non corrispondano ignoro il messaggio e ritento fino allo scadere del timer
        }
        recvfrom(socket, &packet, sizeof(packet), 0, (struct sockaddr *)&addr_recived, &addr_len); // ho consumato i dati
        mutex_unlock(&mutex_rcv);
#else
        recvfrom(socket, &packet, sizeof(packet), 0, (struct sockaddr *)&addr_recived, &addr_len); // ho consumato i dati
#endif
        // Fine ricezione pacchetto


        // Genera un numero casuale tra 0 e 1
        random_value = (double)rand() / RAND_MAX;
        boolean_variable = (random_value > p);


        // Verifica la checksum del pacchetto ricevuto
        if (verify_checksum(packet) && boolean_variable) {
            printf("ricevuto pacchetto: %d\n", packet.sequence_number);


            // Reset timer
            start_time = time(NULL); 


            // Pacchetto ricevuto correttamente, aggiornare il receiver_buffer 
            receiver_buffer[packet.sequence_number] = packet;
            last_packet = (packet.last_pck_flag == true) ? packet.sequence_number:-1;


            // Da qui si implemeta la funzionalità che permette al mittente di sfruttare gli ack cumulativi
            received_packet_flag[packet.sequence_number] = true;     // marco la rispettiva entry come ricevuta ed archiviata
            current = get_last(received_packet_flag);


            if (current > last_packet_acked) {
                last_packet_acked = current;

                
                // crea il pacchetto di ack
                ack.sequence_number = current;
                ack.ack_code = ACK;
                ack.checksum = calculate_ack_checksum(ack);

                // Invia Ack
#ifdef SERVER
                //mutex_lock(&mutex_snd);
                sendto(socket, &ack, sizeof(ack), 0, addr, sizeof(*addr));
                //mutex_unlock(&mutex_snd);
#else
                sendto(socket, &ack, sizeof(ack), 0, addr, sizeof(*addr));
#endif
                printf("inviato ack: %d\n", ack.sequence_number);

            }

        } else {
            // La checksum del pacchetto non corrisponde, ignora il pacchetto (il mittente lo ritrasmetterà allo scadere del timer)

            if (p > 0) {
                printf("%spacchetto perso: %d%s\n", RED, packet.sequence_number, RESET);
            }

        }


        // Verifica che tutti i pacchetti siano stati ricevuti
        if (check_end_transmission(received_packet_flag, last_packet)) {

            // Assembla il messaggio
            bytes_received = assembly_msg(receiver_buffer, last_packet, buffer);
            return bytes_received;
        }
    }
}




