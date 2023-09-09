/**
 * @file double_linked_list_opt.c
 * @brief Breve descrizione del file.
 *
 * 
 *
 * @authors Simone Arcari
 * @date 2023-09-09 (nel formato YYYY-MM-DD)
 */


/*
NOTA:   in this implementation of lists we use the assert.h library for
		carry out all the checks on the parameters passed to the functions.
    	Furthermore, the list is formed by a sentinel node of type *node_t which
    	contains the pointer to the first node in the list. 
*/


#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <netinet/in.h>	// includo la libreria per sockaddr_in
#include <arpa/inet.h>	// includo la libreria per inet_ntoa() e ntohs()
#include "double_linked_list_opt.h"


bool is_empty(list_t list) {
/*******************************************************************************
 *
 * Description:
 *      true if the list is empty, false otherwise.
 * 
 * Parameters:
 *      [in] head: pointer to the sentinel node
 *      [out] return bool
 *
 ******************************************************************************/
	node_t **head = list;
	
	assert(head!=NULL);
	return *head==NULL;
}

bool is_not_finish(node_t *pos) {
/*******************************************************************************
 *
 * Description:
 *      true if it is not the last node, false otherwise.
 * 
 * Parameters:
 *      [in] pos: pointer to node
 *      [out] return bool
 *
 ******************************************************************************/
	assert(pos!=NULL);
	return pos->next!=NULL; 
}

bool is_finish(node_t *pos) {
/*******************************************************************************
 *
 * Description:
 *      true if it is the last node, false otherwise.
 * 
 * Parameters:
 *      [in] pos: pointer to node
 *      [out] return bool
 *
 ******************************************************************************/
	assert(pos!=NULL);
	return pos->next==NULL;
}

node_t *create_list(void) {
/*******************************************************************************
 *
 * Description:
 *      creates a list by allocating dynamic memory in the heap and returns
 *      the pointer to the allocated memory area.
 * 
 * Parameters:
 *      [out] return head: pointer to the sentinel node
 *
 ******************************************************************************/
	node_t *head = malloc(sizeof(node_t*));
	return head;
}

node_t *delete_list(list_t list) {
/*******************************************************************************
 *
 * Description:
 *      it deletes a list by deallocating its memory from the heap but without 
 *      resetting the values of this memory area.
 * 
 * Parameters:
 *      [in] head: pointer to the sentinel node
 *      [out] return: NULL pointer 
 *
 ******************************************************************************/
	assert(list!=NULL);
	
	node_t **head = list;
	node_t *temp;
	
	/* deallocates all nodes in the list */
	while(*head ) {
		temp = *head;
		*head = (*head)->next; // increment the position
		
		free(temp);
	}
	
	free(*head);	// finally deallocates the sentinel node
	return NULL;
}

node_t *insert_first(list_t list, struct sockaddr_in client_address) {
/*******************************************************************************
 *
 * Description:
 *      inserts a node in first position with its relative 
 *      value. The inserted node is allocated on the heap.
 * 
 * Parameters:
 *	[in] head: pointer to the sentinel node
 *      [in] value: value to be included in the list
 *      [out] return new: pointer to the inseted node 
 *
 ******************************************************************************/
	assert(list!=NULL);
	
	node_t **head = list;
	node_t *new = malloc(sizeof(node_t));
	if (!new) exit(EXIT_FAILURE);
	
	
	new->next = *head;
	*head = new;
	new->client_address = client_address;
	
	return new;
}

node_t *insert(list_t list, node_t *pos, struct sockaddr_in client_address) {
/*******************************************************************************
 *
 * Description:
 *      inserts a node in succession at the given position with its relative 
 *      value. The inserted node is allocated on the heap.
 * 
 * Parameters:
 *	[in] head: pointer to the sentinel node
 *      [in] pos: pointer to node
 *      [in] value: value to be included in the list
 *      [out] return new: pointer to the inseted node 
 *
 ******************************************************************************/
	assert(list!=NULL);
	
	node_t **head = list;

	if(!pos) return insert_first(head, client_address);
	
	
	while (head && *head != pos->next)
		head = &(*head)->next; // increment the position
	
	
	if(head) {
		node_t *new = malloc(sizeof(node_t));
		if (!new) exit(EXIT_FAILURE);
		
		new->client_address = client_address;
		new->next = *head;
		*head = new;
		return new;
	}
	return NULL;
}

node_t *remove_node(list_t list, node_t *pos) {
/*******************************************************************************
 *
 * Description:
 *      removes the node pointed to by pos, deallocating its memory area in the 
 *      heap and returns the position to the previous node.
 * 
 * Parameters:
 *      [in] head: pointer to the sentinel node
 *      [in] pos: pointer to node
 *      [out] return temp: pointer to the node preceding the one removed
 *
 ******************************************************************************/
	assert(list!=NULL);
	assert(pos!=NULL);

	node_t **head = list;

	if(*head == pos) {
		*head= pos->next;
		free(pos);

		return *head;
	}

	while(*head && (*head)->next != pos) 
		head = &(*head)->next; // increment the position
	
	if(*head) {
		(*head)->next = pos->next;
		free(pos);
	}
	
	return *head;
}
	
node_t *next(node_t *pos) {
/*******************************************************************************
 *
 * Description:
 *      returns the next node to the given position.
 * 
 * Parameters:
 *      [in] pos: pointer to node
 *      [out] return next: pointer to next node
 *
 ******************************************************************************/
	assert(pos!=NULL);
	return pos->next;
}

node_t *prev(list_t list, node_t *pos) {
/*******************************************************************************
 *
 * Description:
 *      returns the previous node to the given position.
 * 
 * Parameters:
 *      [in] head: pointer to the sentinel node
 *      [in] pos: pointer to node
 *      [out] return next: pointer to previous node
 *
 ******************************************************************************/
	assert(list!=NULL);
	assert(pos!=NULL);

	node_t **head = list;
	
	if(*head == pos) return NULL;
	
	while (*head && (*head)->next != pos) 
		head = &(*head)->next; // increment the position

	return *head;
}

node_t *head(list_t list) {
/*******************************************************************************
 *
 * Description:
 *      returns the first node to the list.
 * 
 * Parameters:
 *      [in] head: pointer to the sentinel node
 *      [out] return next: pointer to first node
 *
 ******************************************************************************/
	assert(list!=NULL);
	node_t **head = list;
	return *head;
}

node_t *tail(list_t list) {
/*******************************************************************************
 *
 * Description:
 *      returns the last node to the list.
 * 
 * Parameters:
 *      [in] head: pointer to the sentinel node
 *      [out] return pos: pointer to last node
 *
 ******************************************************************************/
	assert(list!=NULL);

	node_t **head = list;

	while (*head && (*head)->next != NULL) 
		head = &(*head)->next; // increment the position

	return *head;
}

struct sockaddr_in get_value(node_t *pos) {
/*******************************************************************************
 *
 * Description:
 *      returns the value contained in the node.
 * 
 * Parameters:
 *      [in] pos: pointer to node
 *      [out] return value: value contained in the node
 *
 ******************************************************************************/
	assert(pos!=NULL);
	return pos->client_address;
}

void set_value(node_t *pos, struct sockaddr_in client_address) {
/*******************************************************************************
 *
 * Description:
 *      sets the value provided in the node.
 * 
 * Parameters:
 *      [in] pos: pointer to node
 *      [in] value: value to be set
 *
 ******************************************************************************/
	assert(pos!=NULL);
	pos->client_address = client_address;
}

void load_list(list_t list, struct sockaddr_in client_address[], int n) {
/*******************************************************************************
 *
 * Description:
 *      populates the empty list by creating the nodes relative to the values
 *      of the array value[]
 * 
 * Parameters:
 *      [in] head: pointer to the sentinel node
 *      [in] value[]: array containing values to insert
 *      [in] n: array dimension
 *
 ******************************************************************************/
	assert(list!=NULL);
	assert(n>=0);

	node_t **head = list;
	node_t *pos = NULL;

	for(int i=0; i<n; i++) {
		pos = insert(head, pos, client_address[i]); // pos is incremented each time
	}
}

void print_list(list_t list) {
/*******************************************************************************
 *
 * Description:
 *      print list
 * 
 * Parameters:
 *      [in] head: pointer to the sentinel node
 *
 ******************************************************************************/
	assert(list!=NULL);

	node_t **head = list;
	printf("\n%s%sLista client connessi:%s\n", BOLDBLACK, BG_MAGENTA, RESET);

	while(*head) {
		printf("[ %s%s%s:", CYAN, inet_ntoa((*head)->client_address.sin_addr), RESET);
        printf("%sUDP%u%s ]\n", MAGENTA, ntohs((*head)->client_address.sin_port), RESET);
		head = &(*head)->next;
	}
}

bool is_in_list(list_t list, struct sockaddr_in client_address) {
	assert(list!=NULL);

	node_t **head = list;

	while(*head) {
		if (memcmp(&client_address, &(*head)->client_address, sizeof(client_address)) == 0) {
			return true;

		}

		head = &(*head)->next;
	}
	
	return false;
}
