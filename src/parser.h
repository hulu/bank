#ifndef PARSER_H_
#define PARSER_H_

#define NUM_METHODS 6

#include <pthread.h>
#include "trie.h"

typedef struct SavingValue {
	long int iValue;
	char * sValue;
} SavingValue;

typedef struct Saving{
	SavingValue value;
	int delete_old;
	int cur_pos;
} Saving;

typedef enum ACCUM_METHOD {
	append = 0, replace = 1, add = 2
} ACCUM_METHOD;

typedef struct Account{
	Saving *savings[NUM_METHODS];
	int updated[NUM_METHODS];
	pthread_mutex_t lock;
} Account;

void deposit(char *messages, Node *root);

void parse_token_one_by_one(char *str, char delimiter,
		void (*func)(char*, void*), void *arg);

void parse_one_message(char *message, void *root);

void parse_body(char *body, void *acct);

void parse_body_part(char *part, void *arg);

Account *get_new_account();

Saving *get_new_saving();

SavingValue get_new_savingvalue();

void save(char *value, char *type, char *sample_rate, Account *acct);

PointerContainer *withdraw(Node *root, const int size_limit);

void continously_write_to_tail(PointerContainer **head, PointerContainer **tail, char *content, int *pos,
		const int size_limit, char *label);

int is_useful_node(void *arg);

#endif /* PARSER_H_ */
