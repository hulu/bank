#ifndef TRIE_H_
#define TRIE_H_

#define MAX_TRIE_LEVEL 1024
#define ROOT_TRIE_LEVEL -1

typedef struct Node Node;
/*
 * Element of Trie
 */
struct Node {
	char val;
	int level;
	void* hook;
	Node* children;
	Node* sibling;
};

typedef struct PointerContainer PointerContainer;
/*
 * Structure used for dynamically collect pointers, via linked-list
 */
struct PointerContainer {
	char *label;
	void *contained;
	PointerContainer *next;
};

Node *get_new_node(char c, int level);

PointerContainer *get_new_container(void *contained, char *label);

void delete_container(PointerContainer **p);

Node *search_trie(Node *root, const char *val);

PointerContainer *harvest_all_hooked(Node *root, int (*func)(void *arg));

void print_trie(Node *root);

#endif /* TRIE_H_ */
