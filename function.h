/**
 * @file function.h
 * @brief Breve descrizione del file.
 *
 * 
 *
 * @authors Simone Arcari, Valeria Villano
 * @date 2023-09-09 (nel formato YYYY-MM-DD)
 */


#ifndef FUNCTION_H_
#define FUNCTION_H_


#include <dirent.h>
#include <arpa/inet.h>
#include <signal.h>
#include "double_linked_list_opt.h"


#define MAX_BUFFER_SIZE 1024
#define MAX_BUFFER_FILE_SIZE 32768              // 32kB per ricezione invio file (il server deve inviare frammenti file massimo da 32kB)
#define PATH_FILE_FOLDER "file_folder_server"   // path per la cartella preposta per contenere i file gestiti dal server


#define SMALL_SIZE  1024        // 1kB  valore oltre il quale si considera un file "small" prima di questo valore lo si considera "micro"
#define MEDIUM_SIZE 10240       // 10kB valore oltre il quale si considera un file "medium"
#define LARGE_SIZE  1048576     // 1MB  valore oltre il quale si considera un file "large"


#define FRAGMENT_MICRO_SIZE     1024    // 1kB     dimensione dei frammenti del file passati al livello di trasporto 
#define FRAGMENT_SMALL_SIZE     8192    // 8kB     dimensione dei frammenti del file passati al livello di trasporto
#define FRAGMENT_MEDIUM_SIZE    16384   // 16kB    dimensione dei frammenti del file passati al livello di trasporto
#define FRAGMENT_LARGE_SIZE     32768   // 32kB    dimensione dei frammenti del file passati al livello di trasporto (il client deve poter ricever frammenti file fino a 32kB)


#define EXIT_ERROR -1           // codice di errore


/* Stringhe per gestire i caratteri colorati nel terminale */
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


/* Struttura per passare dati ai threads */
typedef struct {
    int server_socket;
    struct sockaddr_in client_address;
    node_t *pos_client;
} ClientInfo;


int receive_file(int client_socket,  struct sockaddr_in server_address, char* filename);



/* Dichiarazione delle funzioni */
DIR *check_directory(const char *path_name);
int thread_start(int server_socket, struct sockaddr_in client_address, node_t *pos_client);
int accept_client(int server_socket, struct sockaddr_in client_address);
int close_connection(int server_socket, struct sockaddr_in client_address);
int send_file_list(int server_socket, struct sockaddr_in client_address);
int send_file(int server_socket, struct sockaddr_in client_address, char* filename);
int DA_ELIMINARE(int server_socket, struct sockaddr_in client_address, char* filename);
int mutex_lock(pthread_mutex_t *mutex);
int mutex_unlock(pthread_mutex_t *mutex);
void *handler_client(void *arg);
void thread_kill(pthread_t tid, node_t *pos_client);
void handle_ctrl_c(int signum, siginfo_t *info, void *context);

#endif  /* FUNCTION_H_ */