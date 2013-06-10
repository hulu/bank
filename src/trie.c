#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "trie.h"
#include "logger.h"

Node *search_trie(Node *root, const char *val) {
	/*
	 * Search the Trie for the char array.
	 * Create new nodes if part of the array does not exist yet.
	 * Return the node corresponding to the end of the char array.
	 */
	if (root == NULL ) {
		log_warning("NULL root found in Trie search_trie.");
		return NULL ;
	}
	if (val == NULL ) {
		log_warning("Empty target string found in search_trie.");
		return NULL ;
	}
	if (strlen(val) + (root->level) > MAX_TRIE_LEVEL) {
		log_critical("Reached MAX_TRIE_LEVEL. Root level %d, Node %s",
				root->level, val);
		return NULL ;
	}

	Node *cur = root;
	Node *child = NULL;
	int i;
	int found;
	int cur_level = root->level;
	int len = strlen(val);
	char c;
	for (i = 0; i < len; i++, cur_level++) {
		child = cur->children;
		c = val[i];
		found = 0;
		while (child != NULL ) {
			if (child->val != c) {
				child = child->sibling;
			} else {
				found = 1;
				break;
			}
		}
		if (found == 0) {
			Node *n = get_new_node(c, cur_level + 1);
			if (n == NULL ) {
				return NULL ;
			}
			if (cur->children == NULL ) {
				cur->children = n;
			} else {
				child = cur->children;
				while (child->sibling != NULL ) {
					child = child->sibling;
				}
				child->sibling = n;
			}
			cur = n;
		} else {
			cur = child;
		}
	}
	return cur;
}

PointerContainer *harvest_all_hooked(Node *root, int (*func)(void *arg)) {
	/*
	 * Get all the pointers from the hooks
	 */
	if (root == NULL ) {
		log_warning("NULL root found in GetAllHooked.");
		return NULL ;
	}
	Node *node = NULL;
	char buffer[MAX_TRIE_LEVEL + 1];
	PointerContainer *head = NULL, *tail = NULL, *temp = NULL;
	PointerContainer *shead = NULL, *stemp = NULL; // Use a stack for traverse
	shead = get_new_container(root, NULL );
	shead->next = NULL;
	while (shead != NULL ) {
		//pop
		node = (Node*) (shead->contained);
		stemp = shead;
		shead = shead->next;
		delete_container(&stemp);
		if (node->level >= 0) {
			buffer[node->level] = node->val;
		}

		//push
		if (node->sibling != NULL ) {
			stemp = shead;
			shead = get_new_container(node->sibling, NULL );
			shead->next = stemp;
		}
		if (node->children != NULL ) {
			stemp = shead;
			shead = get_new_container(node->children, NULL );
			shead->next = stemp;
		}

		//harvest
		if (node->hook != NULL && ((func == NULL )|| ((*func)(node->hook))==1)) {
			buffer[(node->level)+1] = '\0';
			temp = get_new_container(node->hook, buffer);
			strcpy(temp->label, buffer);
			temp->next = NULL;
			if(head == NULL){
				head = temp;
				tail = temp;
			}else{
				tail->next = temp;
				tail = tail->next;
			}
		}
	}
	return head;
}

Node *get_new_node(char c, int level) {
	Node *n = malloc(sizeof(Node));
	if (n == NULL ) {
		log_critical("%s", "Fail to allocate memory for Node.");
		return NULL ;
	}
	n->val = c;
	n->level = level;
	n->hook = NULL;
	n->children = NULL;
	n->sibling = NULL;
	return n;
}

PointerContainer *get_new_container(void *contained, char *label) {
	PointerContainer *p = malloc(sizeof(PointerContainer));
	p->contained = contained;
    int len = 0;
	if(label != NULL){
		len = strlen(label);
		p->label = malloc(sizeof(char)*(len+1));
		strcpy(p->label, label);
		p->label[len] = '\0';
	}else{
	    p->label = NULL;
	}
	p->next = NULL;
	return p;
}

void delete_container(PointerContainer **p) {
	if (p == NULL ) {
		return;
	}
	if ((*p) == NULL ) {
		return;
	}
	if ((*p)->label != NULL ) {
		free((*p)->label);
		(*p)->label = NULL;
	}
	free(*p);
	*p = NULL;
}

void print_trie(Node *root) {
	if (root == NULL ) {
		return;
	}
	printf("%c", root->val);
	Node *child = root->children;
	int branched = 0;
	if (child != NULL ) {
		if (child->sibling != NULL ) {
			branched = 1;
		}
		while (child != NULL ) {
			if (branched == 1) {
				printf("{");
			}
			print_trie(child);
			child = child->sibling;
			if (branched == 1) {
				printf("}");
			}
		}
	}
}
