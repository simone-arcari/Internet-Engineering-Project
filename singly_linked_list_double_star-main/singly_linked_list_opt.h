#ifndef SINGLY_LINKED_LIST_OPT_H_
#define SINGLY_LINKED_LIST_OPT_H_

typedef struct node {
	int value;
	struct node *next;
} node_t;


/* function declarations */
bool is_empty(node_t **head);
bool is_not_finish(node_t *pos);
bool is_finish(node_t *pos);
node_t *create_list(void);
node_t *delete_list(node_t **head);
node_t *insert_first(node_t **head, int value);
node_t *insert(node_t **head, node_t *pos, int value);
node_t *remove_node(node_t **head, node_t *pos);
node_t *next(node_t *pos);
node_t *prev(node_t **head, node_t *pos);
node_t *head(node_t **head);
node_t *tail(node_t **head);
int get_value(node_t *pos);
void set_value(node_t *pos, int value);
void popolate_list(node_t **head, int value[], int n);
void print_list(node_t **head);

#endif  /* SINGLY_LINKED_LIST_OPT_H_ */
