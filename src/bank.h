#ifndef BANK_H_
#define BANK_H_

typedef struct BankConfig {
	int bank_port;
	int bank_max_msg_length;
	int bank_queue_limit;
	double bank_send_interval;
	char *bank_pid_file;
	char *bank_health_check;

	char **destiny_hosts;
	int destiny_host_count;
	char *destiny_health_check;
	int _destiny_health_check;
	char *destiny_health_check_msg;
	double destiny_health_check_interval;
	char *destiny_consistent_hashing;
	struct sockaddr_in **downstream_sockaddr;
	socklen_t sockaddr_len;

	int _downstream_consistent_hashing; // Use int to save string comparison
	int *downstream_error_count;
	int *downstream_hash_ring;
	int downstream_consistent_hash_replica;
	int downstream_hash_ring_length;

	char *logging_error_log;
	LogLevel logging_log_level;
} BankConfig;

int initialize_sockaddr(int addr_count, char **host_list,
		struct sockaddr_in **sockaddr_list);

void send_to_downstream(Node *store, int sock_fd, BankConfig config);

void run_receiver(Node *store, BankConfig config);

void *run_sender(void *arg);

void run_bank(BankConfig bank_config);

BankConfig get_bank_config(Node *conf);

int gen_pid_file(const char *pfname);

void *run_downstream_healthcheck(void *arg);

#endif /* BANK_H_ */
