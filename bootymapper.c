#include "logger.h"

#include <event.h>
#include <arpa/inet.h>

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ulimit.h>
#include <ctype.h>

struct config {
	uint16_t port;
	int connect_timeout;
	int read_timeout;
	int max_concurrent;
	int max_read_size;
	int stdin_closed;
	char *search_string;
	int search;
	int format;
	char *send_str;
	long send_str_size;
	int current_running;
	struct event_base *base;
        struct bufferevent *stdin_bev;

	struct stats_st {
		int init_connected_hosts;
		int connected_hosts;
		int completed_hosts;
		int found;
	} stats;
};


struct state {
	struct config *conf;
	uint32_t ip;
	char *response;
	int response_length;
	enum {CONNECTING, CONNECTED} state;
};

void stdin_read_callback(struct bufferevent *bev, void *arg);

void print_status(evutil_socket_t fd, short events, void *arg) {
	struct event *ev;
	struct config *conf = arg;
	struct event_base *base = conf->base;
	struct timeval status_timeout = {1, 0};
	ev = evtimer_new(base, print_status, conf);
	evtimer_add(ev, &status_timeout);
	(void)fd; (void)events;


	if(conf->search == 1) {
	log_info("bootymapper", "(%d/%d descriptors in use) - %d found containing \"%s\", %d inititiated, %d connected, %d completed",
			conf->current_running, conf->max_concurrent, conf->stats.found, conf->search_string,
			conf->stats.init_connected_hosts, conf->stats.connected_hosts, conf->stats.completed_hosts);
	} else {
	log_info("bootymapper", "(%d/%d descriptors in use) - %d inititiated, %d connected, %d completed",
                        conf->current_running, conf->max_concurrent, conf->stats.init_connected_hosts,
			conf->stats.connected_hosts, conf->stats.completed_hosts);
	}
}

void decrement_running(struct state *st) {
	struct config *conf = st->conf;

	if(st->response != NULL) {
                free(st->response);
        }

	free(st);

	conf->current_running--;

	if (evbuffer_get_length(bufferevent_get_input(conf->stdin_bev)) > 0) {
		stdin_read_callback(conf->stdin_bev, conf);
	}

	if (conf->stdin_closed && conf->current_running == 0) {
		print_status(0, 0, conf);
		log_info("bootymapper", "Scan completed.");
		exit(0);
	}
}

void event_callback(struct bufferevent *bev, short events, void *arg) {
	struct state *st = arg;
	struct config *conf = st->conf;
	struct in_addr addr;
	addr.s_addr = st->ip;
	if (events & BEV_EVENT_CONNECTED) {
		struct timeval tv = {st->conf->read_timeout, 0};

		if (conf->send_str) {
			struct evbuffer *evout = bufferevent_get_output(bev);
			evbuffer_add_printf(evout, conf->send_str, inet_ntoa(addr), inet_ntoa(addr), inet_ntoa(addr), inet_ntoa(addr));
		}

		bufferevent_set_timeouts(bev, &tv, &tv);

		st->state = CONNECTED;
		st->conf->stats.connected_hosts++;
	} else {
		if(st->response != NULL) {

			st->response[st->response_length] = '\0';

			if(st->conf->search == 1 && strstr(st->response, st->conf->search_string) != NULL) {
				if(st->conf->format == 1) {
					printf("%s\n", inet_ntoa(addr));
				} else {
					printf("%s %s\n\n", inet_ntoa(addr), st->response);
				}
				st->conf->stats.found++;
			} else if(st->conf->search == 0) {
				if(st->conf->format == 1) {
                                	printf("%s\n", inet_ntoa(addr));
				} else {
                                	printf("%s %s\n\n", inet_ntoa(addr), st->response);
                	        }
			}
		}
		st->conf->stats.completed_hosts++;
		bufferevent_free(bev);
		decrement_running(st);
	}
}

void read_callback(struct bufferevent *bev, void *arg) {
	struct evbuffer *in = bufferevent_get_input(bev);
	struct state *st = arg;
	size_t len = evbuffer_get_length(in);
	struct in_addr addr;
        addr.s_addr = st->ip;

	char *buf = malloc(len+1);
	evbuffer_remove(in, buf, len);

	if(st->response_length >= st->conf->max_read_size) {

		st->response[st->response_length] = '\0';

		if(st->conf->search == 1 && strstr(st->response, st->conf->search_string) != NULL) {
			if(st->conf->format == 1) {
				printf("%s\n", inet_ntoa(addr));
			} else {
				printf("%s %s\n\n", inet_ntoa(addr), st->response);
			}
				st->conf->stats.found++;
		} else if(st->conf->search == 0) {
			if(st->conf->format == 1) {
				printf("%s\n", inet_ntoa(addr));
			} else {
				printf("%s %s\n\n", inet_ntoa(addr), st->response);
			}
		}
		fflush(stdout);
		free(buf);
		bufferevent_free(bev);
		decrement_running(st);
		return;
	}

	if (len > 0) {

		if (!buf) {
			log_fatal("bootymapper", "Cannot allocate buffer of length %d for received data", len+1);
			return;
		}

		char *ptr = NULL;
		ptr = realloc(st->response, st->response_length+len+1);
		if(ptr == NULL) {
			log_fatal("bootymapper", "Failed to reallocate buffer. You probably need more memory");
		}
		st->response = ptr;
		memcpy(st->response + st->response_length, buf, len);
		st->response_length += len;
		fflush(stdout);
	}

	free(buf);
}

void grab_banner(struct state *st) {
	struct sockaddr_in addr;
	struct bufferevent *bev;
	struct timeval read_to = {st->conf->connect_timeout, 0};

	addr.sin_family = AF_INET;
	addr.sin_port = htons(st->conf->port);
	addr.sin_addr.s_addr = st->ip;

	bev = bufferevent_socket_new(st->conf->base, -1, BEV_OPT_CLOSE_ON_FREE);

	bufferevent_set_timeouts(bev, &read_to, &read_to);

	bufferevent_setcb(bev, read_callback, NULL, event_callback, st);
	bufferevent_enable(bev, EV_READ);
	st->state = CONNECTING;

	st->conf->stats.init_connected_hosts++;

	if (bufferevent_socket_connect(bev, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		log_warn("bootymapper", "Could not connect on socket %d (%d are already open). Did you try setting ulimit?",
			st->conf->current_running+1, st->conf->current_running);
		perror("connect");

		bufferevent_free(bev);
		decrement_running(st);
	}
}

void stdin_event_callback(struct bufferevent *bev, short events, void *ptr) {
	struct config *conf = ptr;

	if (events & BEV_EVENT_EOF) {
		conf->stdin_closed = 1;
		if (conf->current_running == 0) {
			log_info("bootymapper", "Scan terminated.");
			print_status(0, 0, conf);
			exit(0);
		}
	}
}

void stdin_read_callback(struct bufferevent *bev, void *arg) {
	struct evbuffer *in = bufferevent_get_input(bev);
	struct config *conf = arg;

	while (conf->current_running < conf->max_concurrent && evbuffer_get_length(in) > 0) {
		char *ip_str;
		size_t line_len;
		char *line = evbuffer_readln(in, &line_len, EVBUFFER_EOL_LF);
		struct state *st;
		if (!line) {
			break;
		}

		ip_str = line;

		conf->current_running++;
		st = malloc(sizeof(*st));
		st->conf = conf;
		st->response = NULL;
        	st->response_length = 0;
		st->ip = inet_addr(ip_str);
		free(line);
		grab_banner(st);
	}
}

int main(int argc, char *argv[]) {
	struct event_base *base;
	struct event *status_timer;
	struct timeval status_timeout = {1, 0};
	int c;
	struct option long_options[] = {
		{"concurrent", required_argument, 0, 'c'},
		{"port", required_argument, 0, 'p'},
		{"connect-timeout", required_argument, 0, 't'},
		{"read-timeout", required_argument, 0, 'r'},
		{"verbosity", required_argument, 0, 'v'},
		{"request", required_argument, 0, 'd'},
		{"search-string", required_argument, 0, 's'},
		{"format", required_argument, 0, 'f'},
		{"max-read-size", required_argument, 0, 'm'},
		{0, 0, 0, 0}

	};

	struct config conf;
	int ret;
	FILE *fp;

	log_init(stderr, LOG_INFO);

	ret = ulimit(4, 1000000);

	if (ret < 0) {
		log_fatal("bootymapper", "Could not set ulimit.");
		perror("ulimit");
		exit(1);
	}

	base = event_base_new();
	conf.base = base;

	conf.stdin_bev = bufferevent_socket_new(base, 0, BEV_OPT_DEFER_CALLBACKS);
	bufferevent_setcb(conf.stdin_bev, stdin_read_callback, NULL, stdin_event_callback, &conf);
	bufferevent_enable(conf.stdin_bev, EV_READ);

	status_timer = evtimer_new(base, print_status, &conf);
	evtimer_add(status_timer, &status_timeout);

	conf.max_read_size = 1048576;
	conf.search = 0;
	conf.search_string = NULL;
	conf.max_concurrent = 10000;
	conf.current_running = 0;
	memset(&conf.stats, 0, sizeof(conf.stats));
	conf.connect_timeout = 5;
	conf.read_timeout = 5;
	conf.stdin_closed = 0;
	conf.send_str = NULL;

	while (1) {
		int option_index = 0;
		c = getopt_long(argc, argv, "c:p:t:r:v:d:s:f:m:",
				long_options, &option_index);

		if (c < 0) {
			break;
		}

		switch (c) {
		case 'c':
			conf.max_concurrent = atoi(optarg);
			break;
		case 'p':
			conf.port = atoi(optarg);
			break;
		case 't':
			conf.connect_timeout = atoi(optarg);
			break;
		case 'r':
			conf.read_timeout = atoi(optarg);
			break;
		case 'v':
			if (atoi(optarg) >= 0 && atoi(optarg) <= 5) {
				log_init(stderr, atoi(optarg));
			}
			break;
		case 'd':
			fp = fopen(optarg, "r");
			if (!fp) {
				log_error("bootymapper", "Could not open request data file '%s':", optarg);
				perror("fopen");
				exit(-1);
			}
			fseek(fp, 0L, SEEK_END);
			conf.send_str_size = ftell(fp);
			fseek(fp, 0L, SEEK_SET);
			conf.send_str = malloc(conf.send_str_size+1);
			if (!conf.send_str) {
				log_fatal("bootymapper", "Could not allocate buffer of length %d to fit request data.", conf.send_str_size+1);
			}
			if (fread(conf.send_str, conf.send_str_size, 1, fp) != 1) {
				log_fatal("bootymapper", "Could not read from send data file '%s':", optarg);
			}
			conf.send_str[conf.send_str_size] = '\0';
			fclose(fp);
			break;
		case 's':
			conf.search = 1;
			conf.search_string = malloc(strlen(optarg) + 1);
			strcpy(conf.search_string, optarg);
			break;
		case 'f':
			if(strstr(optarg, "ip_only") != NULL) {
			conf.format = 1;
			}
			break;
		case 'm':
			conf.max_read_size = atoi(optarg);
			break;
		case '?':
			printf("Usage: %s [-c max_concurrent_sockets] [-t connection_timeout] [-r read_timeout] "
				   "[-v verbosity=0-5] [-d send_data] [-s \"search_string\"] [-f ip_only] -m max_read_size -p port\n", argv[0]);
			exit(1);
		default:
			break;
		}
	}

	log_info("bootymapper", "Scanning port %d, %d max descriptors, %d second connection timeout, %d second read timeout",
			conf.port, conf.max_concurrent, conf.connect_timeout, conf.read_timeout);

	event_base_dispatch(base);

	return 0;
}
