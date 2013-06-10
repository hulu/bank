#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

uint32_t get_hash(char * str, int len, int range) {
	if (str == NULL || len < 1 || range < 1) {
		return 0;
	}
	// Based on
	// http://en.wikipedia.org/wiki/MurmurHash
	// But I am not sure if your key space will be evenly mapped to int32 space
	// Things like md5 will be too slow..
	uint32_t c1 = 0xcc9e2d51;
	uint32_t c2 = 0x1b873593;
	uint32_t r1 = 15;
	uint32_t r2 = 13;
	uint32_t m = 5;
	uint32_t n = 0xe6546b64;
	uint32_t h = 1;
	uint32_t k = 0;
	int i = 0;
	int j = 0;
	for (i = 0; i < len; i = i + 4) {
		k = *((uint32_t*) (str + i));
		k = k * c1;
		k = (k << r1) | (k >> (32 - r1));
		k = k * c2;
		h = h ^ k;
		h = (h << 2) | (h >> (32 - r2));
		h = h * m + n;
		len = len - 4;
	}
	uint32_t res = 0;
	char *res_p = (char *) (&res);
	for (i = i - 4, j = 1; i < len; i++, j++) {
		*(res_p + (4 - j)) = str[i];
	}
	res = res * c1;
	res = (res << r1) | (res >> (32 - r1));
	res = res * c2;
	h = h ^ res;
	h = h ^ len;
	h = h ^ (h >> 16);
	h = h * 0x85ebca6b;
	h = h ^ (h >> 13);
	h = h * 0xc2b2ae35;
	h = h ^ (h >> 16);

	return h % range;
}

void generate_hash_ring(int **ring, int num_category, int num_replica) {
	int ring_length = num_category * num_replica;
	int i = 0, j = 0;
	int tmp = 0;
	srand(0); // so that restart will keep the consistency
	*ring = malloc(sizeof(int) * (ring_length));
	for (i = 0; i < ring_length; i++) {
		(*ring)[i] = i + 1;
	}
	for (i = ring_length - 1; i > 0; i--) {
		j = rand() % i;
		tmp = (*ring)[j];
		(*ring)[j] = (*ring)[i];
		(*ring)[i] = tmp % num_category;
	}
}

int get_category(int key, const int *ring, const int *downstream_error_count,
		int threshold, int num_category, int num_replica) {
	int i = 0, j = 0;
	int ring_length = num_category * num_replica;
	if (key >= ring_length) {
		return -1;
	}
	for (i = key, j = ring_length;
			downstream_error_count[ring[i]] > threshold && j > 0; j--) {
		i++;
		if (i == ring_length) {
			i = 0;
		}
	}
	if (j == 0) {
		return -1;
	}
	return ring[i];
}

