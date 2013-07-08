#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include "trie.h"
#include "parser.h"
#include "logger.h"

const int STR_SAVING_SIZE = 8192;

const char MSG_SEP = '\n';
const char HEAD_SEP = ':';
const char BODY_SEP = ',';
const char MTD_SEP = '|';
const char SAMPLE_RATE_MARK = '@';

const char G[] = "g";
const char H[] = "h";
const char C[] = "c";
const char S[] = "s";
const char M[] = "m";
const char MS[] = "ms";

const char STORE_SEP = '\n';
const char *DELETE_STR = ":delete|";

const char ACCUM_METHOD_LIST[NUM_METHODS] = { append, replace, add, append,
		append, append, };

const char *TYPE_LIST[NUM_METHODS] = { "h", "g", "c", "m", "s", "ms", };

void deposit(char *messages, Node *root) {
	parse_token_one_by_one(messages, MSG_SEP, &parse_one_message, root);
}

void parse_token_one_by_one(char *str, char delimiter,
		void (*func)(char*, void*), void *arg) {
	if (str == NULL || func == NULL ) {
		return;
	}
	if (delimiter == '\0') {
		log_warning("Illegal delimiter.");
		return;
	}
	char *cur = str;
	char *sep = NULL;
	while (cur != NULL ) {
		sep = strchr(cur, delimiter);
		if (sep != NULL ) {
			*sep = '\0';
		}
		(*func)(cur, arg);
		if (sep != NULL ) {
			cur = sep + 1;
		} else {
			cur = NULL;
		}
	}
}

void parse_one_message(char *message, void *root) {
	/*
	 * a.b:10|ms,20|g,30|c
	 */
	char *head = NULL;
	char *body = NULL;
	char *sep = strchr(message, HEAD_SEP);
	if (sep == NULL ) {
		body = "";
	} else {
		*sep = '\0';
		body = sep + 1;
	}
	head = message;
	if (strcmp(head, "") == 0) {
		return;
	}
	Node *n = search_trie(root, head);
	if (n == NULL ) {
		log_critical("Cannot obtain node from trie.");
		return;
	}
	if (n->hook == NULL ) {
		Account *acct = get_new_account();
		n->hook = (void*) acct;
	}
	parse_body(body, n->hook);
}

void parse_body(char *body, void *acct) {
	parse_token_one_by_one(body, BODY_SEP, &parse_body_part, acct);
}

void parse_body_part(char *part, void *arg) {
	/*
	 * 10|ms|@0.1 or 10|ms
	 */
	if (part == NULL || arg == NULL ) {
		return;
	}
	Account *acct = (Account*) arg;
	int len = strlen(part);
	char *value = NULL;
	char *type = NULL;
	char *sample_rate = NULL;
	char *sep = NULL;

	value = part;
	sep = strchr(part, MTD_SEP);
	if (sep == NULL ) {
		type = "m";
	} else {
		*sep = '\0';
		len = len - strlen(value) - 1;
		if (len < 1) {
			*sep = MTD_SEP;
			log_warning("Malformed value %s", value);
			return;
		}
		type = sep + 1;
		sep = strchr(type, MTD_SEP);
		if (sep != NULL ) {
			*sep = '\0';
			len = len - strlen(type) - 1;
			if (len < 2) {
				*sep = MTD_SEP;
				log_warning("Malformed type %s", type);
				return;
			}
			if (*(sep + 1) == SAMPLE_RATE_MARK) {
				sample_rate = sep + 2;
			}
		}
	}
	save(value, type, sample_rate, acct);
}

Account *get_new_account() {
	Account *acct = malloc(sizeof(Account));
	int i;
	for (i = 0; i < NUM_METHODS; i++) {
		acct->savings[i] = get_new_saving();
		acct->updated[i] = 0;
	}
	int lock_init_status = pthread_mutex_init(&(acct->lock), NULL );
	if (lock_init_status != 0) {
		log_critical(
				"Account lock initialization failed %d. Operation is not thread safe.",
				lock_init_status);
	}
	return acct;
}

Saving *get_new_saving() {
	Saving *s = malloc(sizeof(Saving));
	s->cur_pos = 0;
	s->delete_old = 0;
	s->value = get_new_savingvalue();
	return s;
}

SavingValue get_new_savingvalue() {
	SavingValue sv;
	sv.iValue = 0;
	sv.sValue = NULL;
	return sv;
}

void save(char *value, char *type, char *sample_rate, Account *acct) {
	if (value == NULL || type == NULL || acct == NULL ) {
		log_warning("Bad input value=%s, type=%s, account %d", value, type, acct == NULL );
		return;
	}
	log_info("Saving value %s of type %s", value, type);
	int i;
	int len = 0;
	int vlen = 0;
	int slen = 0;
	int is_delete = 0;
	long int l_value = 1;
	double d_sample_rate = 1;
	char *error;

	errno = 0;
	if (strcmp(type, M) != 0) {
		l_value = strtol(value, &error, 10);
		if (errno != 0) {
			if (strcmp(value, "delete") == 0) {
				is_delete = 1;
			} else {
				log_warning("Error %s converting %s to long", error, value);
				return;
			}
		}
		if (sample_rate != NULL){
			errno = 0;
			d_sample_rate = strtod(sample_rate, &error);
			if (errno != 0) {
				log_warning("Error %s converting %s to double", error, sample_rate);
				return;
			}
			l_value = l_value*(int)(1/d_sample_rate);
		}
	}
	for (i = 0; i < NUM_METHODS; i++) {
		if (strcmp(type, TYPE_LIST[i]) == 0) {
			break;
		}
	}

	if (i >= NUM_METHODS) {
		log_warning("Unrecognized type %s", type);
		return;
	}
	pthread_mutex_lock(&(acct->lock));
	Saving *saving = acct->savings[i];
	ACCUM_METHOD accum_method = ACCUM_METHOD_LIST[i];
	if (is_delete == 1) {
		saving->delete_old = 1;
		saving->cur_pos = 0;
		saving->value.iValue = 0;
		return;
	}
	vlen = strlen(value);
	len = vlen;
	if (sample_rate != NULL){
		slen = strlen(sample_rate);
		len += ((slen)+1);
	}
	switch (accum_method) {
	case append:
		if (saving->value.sValue == NULL ) {
			saving->value.sValue = malloc(sizeof(char) * STR_SAVING_SIZE);
			saving->cur_pos = 0;
		}
		if (STR_SAVING_SIZE - (saving->cur_pos) > len + 1) {
			strcpy(&(saving->value.sValue[saving->cur_pos]), value);
			saving->cur_pos += vlen;
			if (sample_rate != NULL){
				saving->value.sValue[saving->cur_pos] = SAMPLE_RATE_MARK;
				saving->cur_pos ++;
				strcpy(&(saving->value.sValue[saving->cur_pos]), sample_rate);
				saving->cur_pos += slen;
			}
			saving->value.sValue[saving->cur_pos] = STORE_SEP;
			saving->cur_pos++;
			saving->value.sValue[saving->cur_pos] = '\0';
		} else {
			log_critical("STR_SAVING_SIZE exceeded");
		}
		break;
	case add:
		saving->value.iValue += l_value;
		break;
	case replace:
		saving->value.iValue = l_value;
		break;
	default:
		log_error("Accumulation method not supported %d", ACCUM_METHOD_LIST[i]);
		break;
	}
	acct->updated[i] = 1;
	pthread_mutex_unlock(&(acct->lock));
}

PointerContainer *withdraw(Node *root, const int size_limit) {
	if (root == NULL || size_limit <= 0) {
		return NULL ;
	}
	PointerContainer *harvested = NULL;
	PointerContainer *temp = NULL;
	PointerContainer *head = NULL, *tail = NULL;
	Account *acct = NULL;
	char *label = NULL;
	char buffer[STR_SAVING_SIZE];
	int label_len = 0;
	int del_str_len = strlen(DELETE_STR);
	int i = 0;
	const char *type = NULL;
	Saving *saving = NULL;
	ACCUM_METHOD accum_method;
	int writer_pos = 0;
	char append_str_search_terms[] = {STORE_SEP, SAMPLE_RATE_MARK, '\0'};

	char *ccur = NULL;
	int icur = 0;
	int bcur = 0;
	int sav_len = 0;
	int delta = 0;

	harvested = harvest_all_hooked(root, &is_useful_node);
	if (harvested == NULL ) {
		return NULL ;
	}

	while (harvested != NULL ) {
		temp = harvested;
		harvested = harvested->next;
		if (temp->contained == NULL ) {
			log_error("Nothing in harvested container.");
			delete_container(&temp);
			continue;
		}
		if (((Node*) (temp->contained))->hook == NULL ) {
			log_critical("No hooked item in harvested container.");
			delete_container(&temp);
			continue;
		}
		if (temp->label == NULL ) {
			log_error("No label for harvested container.");
			delete_container(&temp);
			continue;
		}
		label = temp->label;
		label_len = strlen(label);
		acct = (Account*) (temp->contained);
		pthread_mutex_lock(&(acct->lock));
		for (i = 0; i < NUM_METHODS; i++) {
			if (acct->updated[i] == 0) {
				continue;
			}
			saving = (acct->savings)[i];
			type = TYPE_LIST[i];
			accum_method = ACCUM_METHOD_LIST[i];
			if (saving->delete_old == 1) {
				bcur = 0;
				strcpy(&buffer[bcur], label);
				bcur += label_len;
				strcpy(&buffer[bcur], DELETE_STR);
				bcur += del_str_len;
				strcpy(&buffer[bcur], type);
				bcur += strlen(type);
				buffer[bcur] = '\n';
				bcur++;
				buffer[bcur] = '\0';
				continously_write_to_tail(&head, &tail, buffer, &writer_pos,
						size_limit, label);
			}
			switch (accum_method) {
			case append:
				ccur = saving->value.sValue;
				sav_len = strlen(ccur);
				icur = 0;
				while (icur < sav_len) {
					bcur = 0;
					strcpy(&buffer[bcur], label);
					bcur += label_len;
					delta = strcspn(&ccur[icur], append_str_search_terms);
					if (strcmp(type, M) != 0) {
						buffer[bcur] = ':';
						bcur++;
						strncpy(&buffer[bcur], &ccur[icur], delta);
						bcur += delta;
						buffer[bcur] = MTD_SEP;
						bcur++;
						strcpy(&buffer[bcur], type);
						bcur += strlen(type);
					}
					if (ccur[icur+delta] == SAMPLE_RATE_MARK){
						// This data point has sample rate specified
						buffer[bcur] = MTD_SEP;
						bcur++;
						buffer[bcur] = SAMPLE_RATE_MARK;
						bcur++;
						icur += (delta + 1);
						delta = strcspn(&ccur[icur], &STORE_SEP);
						strncpy(&buffer[bcur], &ccur[icur], delta);
						bcur += delta;
					}
					icur += (delta + 1);
					buffer[bcur] = MSG_SEP;
					bcur++;
					buffer[bcur] = '\0';
					continously_write_to_tail(&head, &tail, buffer, &writer_pos,
							size_limit, label);
				}
				break;
			case replace:
			case add:
				bcur = 0;
				strcpy(buffer, label);
				bcur += label_len;
				buffer[bcur] = ':';
				bcur++;
				sprintf(&buffer[bcur], "%ld", saving->value.iValue);
				bcur = strlen(buffer);
				buffer[bcur] = MTD_SEP;
				bcur++;
				strcpy(&buffer[bcur], type);
				bcur += strlen(type);
				buffer[bcur] = MSG_SEP;
				bcur++;
				buffer[bcur] = '\0';
				continously_write_to_tail(&head, &tail, buffer, &writer_pos,
						size_limit, label);
				break;
			default:
				log_error("Unsupported accum_method %d", accum_method);
				break;
			}

			acct->updated[i] = 0;
			acct->savings[i]->cur_pos = 0;
			acct->savings[i]->value.iValue = 0;
		}
		pthread_mutex_unlock(&(acct->lock));
		delete_container(&temp);
	}

	return head;
}

void continously_write_to_tail(PointerContainer **head, PointerContainer **tail, char *content, int *pos,
		const int size_limit, char *label) {
	/*
	 * Assumption: Not OK to break content.
	 * Assumption: Outside there is a head pointer.
	 */
	if (tail == NULL ) {
		log_warning("Empty tail pointer found in continuously_write_to_tail.");
		return;
	}
	if (content == NULL || *pos < 0) {
		log_warning("Invalid content or pos in continuously_write_to_tail.");
		return;
	}
	int content_len = strlen(content);
	if (content_len > size_limit) {
		log_warning("content size %d is too long.", content_len);
		return;
	}
	if ((*tail) == NULL ) {
		(*tail) = get_new_container(NULL, label );
		if((*head) == NULL) {
			*head = *tail;
		}
	}
	if (content_len + (*pos) >= size_limit || strcmp((*tail)->label, label) != 0) {
		(*tail)->next = get_new_container(NULL, label );
		if((*head) == NULL) {
			*head = *tail;
		}
		(*tail) = (*tail)->next;
	}
	if ((*tail)->contained == NULL ) {
		(*tail)->contained = (void*) malloc(sizeof(char) * size_limit);
		*pos = 0;
	}
	char *start = &(((char*) ((*tail)->contained))[*pos]);
	strcpy(start, content);
	*pos = content_len + (*pos);
}

int is_useful_node(void *arg) {
	if (arg == NULL) {
		return 0;
	}
	Account *acct = arg;
	int i = 0;
	for (i = 0; i < NUM_METHODS; i++) {
		if ((acct->updated)[i] == 1) {
			return 1;
		}
	}
	return 0;
}
