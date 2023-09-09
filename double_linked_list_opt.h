/**
 * @file double_linked_list_opt.h
 * @brief Breve descrizione del file.
 *
 * 
 *
 * @authors Simone Arcari
 * @date 2023-09-09 (nel formato YYYY-MM-DD)
 */


#ifndef SINGLY_LINKED_LIST_OPT_H_
#define SINGLY_LINKED_LIST_OPT_H_

#include <stdbool.h>
#include <netinet/in.h>	// Includo la libreria per sockaddr_in

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

typedef struct node {
	struct sockaddr_in client_address;
	struct node *next;
} node_t;

typedef node_t** list_t;


/* function declarations */
bool is_empty(list_t list);
bool is_not_finish(node_t *pos);
bool is_finish(node_t *pos);
bool is_in_list(list_t list, struct sockaddr_in client_address);
node_t *create_list(void);
node_t *delete_list(list_t list);
node_t *insert_first(list_t list, struct sockaddr_in client_address);
node_t *insert(list_t list, node_t *pos, struct sockaddr_in client_address);
node_t *remove_node(list_t list, node_t *pos);
node_t *next(node_t *pos);
node_t *prev(list_t list, node_t *pos);
node_t *head(list_t list);
node_t *tail(list_t list);
struct sockaddr_in get_value(node_t *pos);
void set_value(node_t *pos, struct sockaddr_in client_address);
void load_list(list_t list, struct sockaddr_in client_address[], int n);
void print_list(list_t list);

#endif  /* SINGLY_LINKED_LIST_OPT_H_ */
