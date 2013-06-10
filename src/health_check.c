/* Based on Beej's Guide */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "logger.h"

const int BUFFER_SIZE = 256;
const char* LOG_TAG = "[health_check]";
const char* OK = "ok";

void *run_healthcheck_server(void *port) {
	fd_set maintained_fds;
	fd_set active_fds;
	FD_ZERO(&maintained_fds);
	FD_ZERO(&active_fds);
	int cur_max_fd;
	int listener_fd;
	int new_fd;
	int rw, i;
	struct sockaddr_in server_addr, client_addr;
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = INADDR_ANY;
	server_addr.sin_port = htons(*((int*)port));
	socklen_t addr_len = sizeof(client_addr);
	char buffer[BUFFER_SIZE];

	listener_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (bind(listener_fd, (struct sockaddr *) &server_addr, sizeof(server_addr))
			< 0) {
		log_critical("%sError health check socket binding\n", LOG_TAG);
		return NULL;
	}
	listen(listener_fd, 5);
	FD_SET(listener_fd, &maintained_fds);
	cur_max_fd = listener_fd;

	log_debug("%sStart server", LOG_TAG);
	while (1) {
		active_fds = maintained_fds;
		if (select(cur_max_fd + 1, &active_fds, NULL, NULL, NULL ) == -1) {
			log_error("%sError select fd.", LOG_TAG);
		}
		for (i = 0; i <= cur_max_fd; i++) {
			if (FD_ISSET(i, &active_fds)) {
				if (i == listener_fd) {
					new_fd = accept(listener_fd,
							(struct sockaddr *) &client_addr, &addr_len);
					if (new_fd == -1) {
						log_warning("%sError: Could not get new fd by accept.", LOG_TAG);
					} else {
						FD_SET(new_fd, &maintained_fds);
						if (new_fd > cur_max_fd) {
							cur_max_fd = new_fd;
						}
						log_debug("%sNew connection from socket %d",
								LOG_TAG,
								new_fd);
					}
				} else {
					if ((rw = recv(i, buffer, BUFFER_SIZE, 0)) <= 0) {
						if (rw == 0) {
							log_debug("%sSocket %d hung up", LOG_TAG, i);
						} else {
							log_warning("%srecv error.", LOG_TAG);
						}
						close(i);
						FD_CLR(i, &maintained_fds);
					} else {
						if (send(i, OK, rw, 0) == -1) {
							log_warning("%ssend error.", LOG_TAG);
						}
					}
				}
			}
		}
	}
	log_critical("%sHealth check server shutting down", LOG_TAG);
	close(listener_fd);
	return NULL;
}

