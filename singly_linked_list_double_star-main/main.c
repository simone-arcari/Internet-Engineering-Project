#include <stdio.h>
#include <stdbool.h>
#include "singly_linked_list_opt.h"

/******************************** MAIN PROGRAM ********************************/
int main(void) {
	/* example code */
	node_t *list;// = create_list(); // the list is a pointer to the first node
	
	/* populate my list */
	int value[9] = {2,5,7,1,6,4,8,9,0};	// array of auxiliaries
	popolate_list(&list, value, 9);
		
	/* print list */
	printf("lista: ");
	print_list(&list);

	/* get the value of the first node */
	printf("il primo nodo contiene: %d\n", get_value(head(&list)));

	/* remove the last node */
	node_t *pos = tail(&list);
	printf("\nrimuovo l'ultimo nodo:\n");
	pos = remove_node(&list, pos);	// now pos points to the new last node

	/* print list */
	printf("lista: ");
	print_list(&list);

	/* insert node */
	pos = prev(&list, pos);	// now pos points to the penultimate node
	printf("\ninserisco 3 prima dell'ultimo nodo:\n");
	insert(&list, pos, 3);

	/* print list */
	printf("lista: ");
	print_list(&list);

	/* loads all zeros in the value field of the nodes starting from the head */
	pos = list; // now pos points to the first node
	int count = 1;
	printf("\ncarico tutti 0 nei nodi partendo dalla testa:\n");
	do {
		set_value(pos, 0);
		
		printf("%d°-->", count);
		print_list(&list);
		
		pos = next(pos);	// increment the pointed node
		count++;

	}while(pos);

	/* loads all 1s in the value field of the nodes starting from the queue */
	pos = tail(&list); // now pos points to the last node
	count = 1;
	printf("\ncarico tutti 1 nei nodi partendo dalla coda:\n");
	do {
		set_value(pos, 1);
		
		printf("%d°-->", count);
		print_list(&list);
		
		count++;
		pos = prev(&list, pos);	// decreases the pointed node

	}while(pos);	// until pos reaches the sentinel node

	/* delete list */
	list = delete_list(&list);
	if(list==NULL)
		printf("\nho eliminato(deallocato) la lista\n");
	
	return 0;
}
/****************************** END MAIN PROGRAM ******************************/
