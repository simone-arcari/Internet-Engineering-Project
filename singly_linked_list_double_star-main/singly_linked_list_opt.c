/*
NOTA:   in this implementation of lists we use the assert.h library for
	carry out all the checks on the parameters passed to the functions.
    	Furthermore, the list is formed by a sentinel node of type *node_t which
    	contains the pointer to the first node in the list. 
*/


#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <stdbool.h>
#include "singly_linked_list_opt.h"


bool is_empty(node_t **head) {
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
	if (!head) exit(EXIT_FAILURE);
	
	head = NULL;
	return head;
}

node_t *delete_list(node_t **head) {
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
	assert(head!=NULL);
	
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

node_t *insert_first(node_t **head, int value) {
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
	assert(head!=NULL);
	
	node_t *new = malloc(sizeof(node_t));
	if (!new) exit(EXIT_FAILURE);
	
	
	new->next = *head;
	*head = new;
	new->value = value;
	
	return new;
}

node_t *insert(node_t **head, node_t *pos, int value) {
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
	assert(head!=NULL);
	
	if(!pos) return insert_first(head, value);
	
	
	while (head && *head != pos->next)
		head = &(*head)->next; // increment the position
	
	
	if(head) {
		node_t *new = malloc(sizeof(node_t));
		if (!new) exit(EXIT_FAILURE);
		
		new->value = value;
		new->next = *head;
		*head = new;
		return new;
	}
	return NULL;
}

node_t *remove_node(node_t **head, node_t *pos) {
/*******************************************************************************
 *
 * Description:
 *      removes the node pointed to by pos deallocating its memory area in the 
 *      heap and returns the position to the previous node.
 * 
 * Parameters:
 *      [in] head: pointer to the sentinel node
 *      [in] pos: pointer to node
 *      [out] return temp: pointer to the node preceding the one removed
 *
 ******************************************************************************/
	assert(head!=NULL);
	assert(pos!=NULL);

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

node_t *prev(node_t **head, node_t *pos) {
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
	assert(head!=NULL);
	assert(pos!=NULL);
	
	if(*head == pos) return NULL;
	
	while (*head && (*head)->next != pos) 
		head = &(*head)->next; // increment the position

	return *head;
}

node_t *head(node_t **head) {
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
	assert(head!=NULL);
	return *head;
}

node_t *tail(node_t **head) {
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
	assert(head!=NULL);

	while (*head && (*head)->next != NULL) 
		head = &(*head)->next; // increment the position

	return *head;
}

int get_value(node_t *pos) {
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
	return pos->value;
}

void set_value(node_t *pos, int value) {
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
	pos->value = value;
}

void popolate_list(node_t **head, int value[], int n) {
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
	assert(head!=NULL);
	assert(n>=0);

	node_t *pos = NULL; // 
	for(int i=0; i<n; i++) {
		pos = insert(head, pos, value[i]); // pos is incremented each time
	}
}

void print_list(node_t **head) {
/*******************************************************************************
 *
 * Description:
 *      print list
 * 
 * Parameters:
 *      [in] head: pointer to the sentinel node
 *
 ******************************************************************************/
	assert(head!=NULL);

	printf("[ ");
	while(*head) {
		printf("%d, ", (*head)->value);
		head = &(*head)->next;
	}
	printf("\b\b ]\n");
}
