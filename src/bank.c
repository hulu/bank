#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <string.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <netdb.h>
#include <stdint.h>
#include <stdint.h>
#include "parser.h"
#include "trie.h"
#include "logger.h"
#include "conf_parser.h"
#include "bank.h"
#include "health_check.h"
#include "destiny.h"

const char *BANK_SECTION = "bank";
const char *DESTINY_SECTION = "destiny";
const char *LOGGING_SECTION = "logging";

const char *DEFAULT_CONF_FILE = "bank.conf";
const char *DEFAULT_BANK_PORT = "8125";
const char *DEFAULT_BANK_MAX_MSG_LENGTH = "800";
const char *DEFAULT_BANK_SEND_INTERVAL = "0.5"; //second
const char *DEFAULT_BANK_QUEUE_LIMIT = "50";
const char *DEFAULT_BANK_PID_FILE = "bank.pid";
const char *DEFAULT_LOGGING_ERROR_LOG = "bank.log";
const char *DEFAULT_LOGGING_LEVEL = "DEBUG";
const char *DEFAULT_DESTINY_HEALTH_CHECK_INTERVAL = "1.0";
const char *DEFAULT_DESTINY_CONSISTENT_HASHING_REPLICA = "100";
const char *DEFAULT_HEALTH_CHECK_MSG = "health";
const char *CONF_FALSE = "False";
const char *CONF_TRUE = "True";

const unsigned int DOWN_STREAM_ERROR_THRESHOLD = 5;
pthread_mutex_t sending_lock;

int main(int argc, char *argv[]) {
	if (pthread_mutex_init(&sending_lock, NULL ) != 0) {
		printf("Fail to initialize sending_lock.\n");
		exit(1);
	}
	const char *conf_file = DEFAULT_CONF_FILE;
	if (argc >= 3 && strcmp(argv[1], "-c") == 0) {
		conf_file = argv[2];
	}
	Node *conf = read_conf_file(conf_file);
	if (conf == NULL ) {
		printf("Fail to read conf file %s\n", conf_file);
		exit(1);
	}
	BankConfig config = get_bank_config(conf);
	if (gen_pid_file(config.bank_pid_file) != 0) {
		printf("Fail to generate pid file.\n");
		exit(1);
	}
	if (log_init(config.logging_error_log, config.logging_log_level) != 1) {
		printf("Fail to initialize logger.\n");
		exit(1);
	}
	run_bank(config);
	return 0;
}

void run_bank(BankConfig config) {
	Node *store = get_new_node('$', ROOT_TRIE_LEVEL);
	pthread_t sender_thread = 0;
	int i = 0;
	int downstream_up[config.destiny_host_count];
	for (i = 0; i < config.destiny_host_count; i++) {
		downstream_up[i] = 1;
	}
	void *arg[3];
	arg[0] = (void*) store;
	arg[1] = (void*) &config;
	arg[3] = (void*) downstream_up;

	pthread_create(&sender_thread, NULL, &run_sender, (void *) arg);
	run_receiver(store, config);
}

void run_receiver(Node *store, BankConfig config) {
	int buf_size = config.bank_max_msg_length * 2;
	char buf[buf_size];

	struct sockaddr_in server, client;
	int sfd = socket(AF_INET, SOCK_DGRAM, 0);
	int sfd_ds = socket(AF_INET, SOCK_DGRAM, 0);
	int len = 0;
	bzero(&server, sizeof(server));
	server.sin_family = AF_INET;
	server.sin_port = htons(config.bank_port);
	inet_aton("0.0.0.0", &server.sin_addr);
	log_debug("bind=%d",
			bind(sfd, (struct sockaddr *) &server, sizeof(server)));
	socklen_t l_client = sizeof(client);
	int count = 0;
	while (1) {
		count++;
		len = recvfrom(sfd, buf, buf_size, 0, (struct sockaddr *) &client,
				&l_client);
		buf[len] = '\0';
		log_info("MESSAGE FROM CLIENT: %s", buf);
		deposit(buf, store);
		if (count >= config.bank_queue_limit) {
			log_debug("SEND FROM RECEIVER.");
			send_to_downstream(store, sfd_ds, config);
			count = 0;
		}
	}
}

void *run_sender(void *arg) {
	log_debug("Start sender.");
	void **args = (void **) arg;
	struct sockaddr_in;
	int sfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	Node *store = (Node*) args[0];
	BankConfig config = *((BankConfig*) args[1]);
	int send_interval = (int) (config.bank_send_interval * 1e6); //microseconds

	pthread_t health_check_thread = 0;
	pthread_t downstream_health_check_thread = 0;
	if (strcmp(config.bank_health_check, CONF_TRUE) == 0) {
		pthread_create(&health_check_thread, NULL, &run_healthcheck_server,
				(void *) &config.bank_port);
	}
	if (strcmp(config.destiny_health_check, CONF_TRUE) == 0) {
		pthread_create(&downstream_health_check_thread, NULL,
				&run_downstream_healthcheck, (void *) &config);
	}
	while (1) {
		usleep(send_interval);
		log_debug("SEND FROM SENDER.");
		send_to_downstream(store, sfd, config);
	}
	return NULL ;
}

void send_to_downstream(Node *store, int sock_fd, BankConfig config) {
	pthread_mutex_lock(&sending_lock);
	PointerContainer *head = NULL, *temp = NULL;
	head = withdraw(store, config.bank_max_msg_length);
	int i = 0;
	int l = 0;
	int hash = 0;
	while (head != NULL ) {
		temp = head;
		head = head->next;
		l = strlen((char*) (temp->contained));
		log_debug("Send %d char! %s", l, (char*) (temp->contained));
		if (config._downstream_consistent_hashing == 0) {
			for (i = 0; i < config.destiny_host_count; i++) {
				if (config.downstream_sockaddr[i] != NULL
						&& (config._destiny_health_check == 0
								|| config.downstream_error_count[i]
										<= DOWN_STREAM_ERROR_THRESHOLD)) {
					sendto(sock_fd, (char*) (temp->contained), l, 0,
							(struct sockaddr *) (config.downstream_sockaddr[i]),
							config.sockaddr_len);
				}
			}
		} else {
			hash = (int) get_hash(temp->label, strlen(temp->label),
					config.downstream_hash_ring_length);
			i = get_category(hash, config.downstream_hash_ring,
					config.downstream_error_count, DOWN_STREAM_ERROR_THRESHOLD,
					config.destiny_host_count,
					config.downstream_consistent_hash_replica);
			log_debug("Consistent hashing shows we send it to %d.", i);
			if (i < 0) {
				log_warning("No downstream end-point is available.");
				continue;
			}
			if (config.downstream_sockaddr[i] != NULL ) {
				sendto(sock_fd, (char*) (temp->contained), l, 0,
						(struct sockaddr *) (config.downstream_sockaddr[i]),
						config.sockaddr_len);
			}
		}
		free((char*) (temp->contained));
		delete_container(&temp);
	}
	pthread_mutex_unlock(&sending_lock);
}

void *run_downstream_healthcheck(void *arg) {
	int i = 0;
	int n = 0;
	int buf_size = 200;
	char buf[buf_size];
	BankConfig config = *((BankConfig *) (arg));
	int *downstream_error_count = config.downstream_error_count;
	int check_interval = (int) config.destiny_health_check_interval * 1e6;
	int *sockets = malloc(sizeof(int) * config.destiny_host_count);
	int l_msg = strlen(config.destiny_health_check_msg);
	for (i = 0; i < config.destiny_host_count; i++) {
		downstream_error_count[i] = 0;
		sockets[i] = -1;
	}
	while (1) {
		for (i = 0; i < config.destiny_host_count; i++) {
			if (downstream_error_count[i] > 0) {
				close(sockets[i]);
				sockets[i] = -1;
			}
			if (sockets[i] < 0) {
				sockets[i] = socket(AF_INET, SOCK_STREAM, 0);
				if (sockets[i] < 0) {
					downstream_error_count[i] = DOWN_STREAM_ERROR_THRESHOLD + 1;
					log_error(
							"Cannot open socket for downstream health check. %d",
							i);
					continue;
				} else {
					if (connect(sockets[i],
							(struct sockaddr*) (config.downstream_sockaddr[i]),
							config.sockaddr_len) < 0) {
						log_warning(
								"Downstream health check connection fail %d.",
								i);
						downstream_error_count[i] = DOWN_STREAM_ERROR_THRESHOLD
								+ 1;
						continue;
					} else {
						log_debug(
								"Downstream health check connection established %d",
								i);
						downstream_error_count[i] = 0;
					}
				}
			}
			n = send(sockets[i], config.destiny_health_check_msg, l_msg, 0);
			if (n <= 0) {
				log_warning("Downstream health check write failed %d\n", i);
				downstream_error_count[i]++;
				continue;
			}
			n = recv(sockets[i], buf, buf_size - 1, 0);
			if (n <= 0) {
				log_warning("Downstream health check read failed %d\n", i);
				downstream_error_count[i]++;
				continue;
			}
			buf[n] = '\0';
			log_debug("Downstream health check succeed %d with %s\n", i, buf);
			downstream_error_count[i] = 0;
		}
		usleep(check_interval);
	}
	return NULL ;
}

int initialize_sockaddr(int addr_count, char **host_list,
		struct sockaddr_in **sockaddr_list) {
	int i = 0;
	int delta = 0;
	int len = 0;
	int port;
	int success_count = 0;
	struct sockaddr_in *sa_in = NULL;
	struct hostent *he = NULL;
	char buffer[100];
	char *host = NULL;
	char *error = NULL;
	for (i = 0; i < addr_count; i++) {
		sockaddr_list[i] = NULL;
		host = host_list[i];
		if (host == NULL ) {
			continue;
		}
		len = strlen(host);
		delta = strcspn(host, ":");
		strncpy(buffer, host, delta);
		buffer[delta] = '\0';
		he = gethostbyname(buffer);
		if (he == NULL || he->h_addr_list == NULL
				|| (he->h_addr_list)[0] == NULL ) {
			log_critical("Cannot resolve IP address %s", host);
			continue;
		}
		if (delta + 1 < len) {
			strcpy(buffer, &host[delta + 1]);
		} else {
			strcpy(buffer, DEFAULT_BANK_PORT);
		}
		errno = 0;
		port = (int) strtol(buffer, &error, 10);
		if (errno != 0) {
			log_critical("Invalid port number %s", host);
			continue;
		}
		sa_in = malloc(sizeof(struct sockaddr_in));
		bzero(sa_in, sizeof(*sa_in));
		sa_in->sin_family = AF_INET;
		sa_in->sin_port = htons(port);
		memcpy(&(sa_in->sin_addr), he->h_addr_list[0], he->h_length);
		sockaddr_list[i] = sa_in;
		success_count++;
	}
	return success_count > 0 ? 0 : 1;
}

BankConfig get_bank_config(Node *conf) {
	BankConfig config;
	apply_config_i(&config.bank_port, conf, BANK_SECTION, "port",
			DEFAULT_BANK_PORT);
	apply_config_s(&config.bank_health_check, conf, BANK_SECTION,
			"health-check", CONF_FALSE);
	apply_config_i(&config.bank_max_msg_length, conf, BANK_SECTION,
			"max-message-length", DEFAULT_BANK_MAX_MSG_LENGTH);
	apply_config_i(&config.bank_queue_limit, conf, BANK_SECTION, "queue-limit",
			DEFAULT_BANK_QUEUE_LIMIT);
	apply_config_d(&config.bank_send_interval, conf, BANK_SECTION,
			"send-interval", DEFAULT_BANK_SEND_INTERVAL);
	apply_config_s(&config.bank_pid_file, conf, BANK_SECTION, "pid-file",
			DEFAULT_BANK_PID_FILE);
	apply_config_s(&config.logging_error_log, conf, LOGGING_SECTION,
			"error-log", DEFAULT_LOGGING_ERROR_LOG);
	apply_config_log(&config.logging_log_level, conf, LOGGING_SECTION,
			"log-level", DEFAULT_LOGGING_LEVEL);
	apply_config_ss(&config.destiny_hosts, &config.destiny_host_count, ",",
			conf, DESTINY_SECTION, "hosts", "");
	apply_config_s(&config.destiny_health_check, conf, DESTINY_SECTION,
			"health-check", CONF_FALSE);
	apply_config_s(&config.destiny_health_check_msg, conf, DESTINY_SECTION,
			"health-check-msg", DEFAULT_HEALTH_CHECK_MSG);
	apply_config_d(&config.destiny_health_check_interval, conf, DESTINY_SECTION,
			"health-check-interval", DEFAULT_DESTINY_HEALTH_CHECK_INTERVAL);
	apply_config_s(&config.destiny_consistent_hashing, conf, DESTINY_SECTION,
			"consistent-hashing", CONF_FALSE);
	apply_config_i(&config.downstream_consistent_hash_replica, conf,
			DESTINY_SECTION, "consistent-hashing-replica",
			DEFAULT_DESTINY_CONSISTENT_HASHING_REPLICA);

	config.downstream_sockaddr = malloc(
			sizeof(struct sockaddr_in*) * config.destiny_host_count);
	if (initialize_sockaddr(config.destiny_host_count, config.destiny_hosts,
			config.downstream_sockaddr) != 0) {
		log_critical("No downstream sockaddr successfully initialized.");
		exit(1);
	}
	config.sockaddr_len = sizeof(*(config.downstream_sockaddr[0]));

	if (strcmp(config.destiny_health_check, CONF_TRUE) == 0) {
		config._destiny_health_check = 1;
		config.downstream_error_count = malloc(
				sizeof(int) * config.destiny_host_count);
		memset(config.downstream_error_count, (char )0,
				sizeof(int) * config.destiny_host_count);
		if (strcmp(config.destiny_consistent_hashing, CONF_TRUE) == 0) {
			config._downstream_consistent_hashing = 1;
			generate_hash_ring(&(config.downstream_hash_ring),
					config.destiny_host_count,
					config.downstream_consistent_hash_replica);
			config.downstream_hash_ring_length = config.destiny_host_count
					* config.downstream_consistent_hash_replica;
		} else {
			config._downstream_consistent_hashing = 0;
			config.downstream_hash_ring = NULL;
			config.downstream_hash_ring_length = 0;
		}
	} else {
		config._destiny_health_check = 0;
		config.downstream_error_count = NULL;
		config._downstream_consistent_hashing = 0;
		config.downstream_hash_ring = NULL;
		config.downstream_hash_ring_length = 0;
	}

	return config;
}

int gen_pid_file(const char *pfname) {
	if (pfname == NULL ) {
		log_error("No pid file name specified.");
		return 1;
	}
	pid_t pid = getpid();
	FILE *f = fopen(pfname, "w");
	if (f == NULL ) {
		log_critical("Cannot open pid file for write: %s", pfname);
		return 1;
	}
	fprintf(f, "%d", (int) pid);
	fclose(f);
	log_debug("Saved pid %d to %s", (pid), pfname);
	return 0;
}

