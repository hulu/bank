#ifndef _DESTINY_H
#define _DESTINY_H

uint32_t get_hash(char * str, int len, int range);

void generate_hash_ring(int **ring, int num_category, int num_replica);

int get_category(int key, const int *ring, const int *downstream_error_count,
		int threshold, int num_category, int num_replica);

#endif
