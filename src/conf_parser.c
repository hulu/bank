#define _XOPEN_SOURCE 600

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include "trie.h"
#include "logger.h"
#include "conf_parser.h"

Node *read_conf_file(const char *file_name) {
	FILE *pconf = fopen(file_name, "rt");
	if (pconf == NULL ) {
		log_critical("Fail to open conf file %s", file_name);
		return NULL ;
	}
	Node *conf = get_new_node('$', ROOT_TRIE_LEVEL);
	char line[LINE_MAX];
	Node *section_node = NULL;
	Node *var_node = NULL;
	int len = 0;
	int equal_pos = 0;
	char *equal = NULL;

	while (fgets(line, LINE_MAX, pconf) != NULL ) {
		len = strlen(line);
		if (len < 4) {
			continue;
		}
		if (line[0] == '[' && line[len - 2] == ']' && line[len - 1] == '\n') {
			line[len - 2] = '\0';
			section_node = search_trie(conf, &line[1]);
		} else {
			if (section_node == NULL ) {
				continue;
			}
			equal = strchr(line, '=');
			if (equal == NULL ) {
				continue;
			}
			equal_pos = (int) strcspn(line, "=");
			if (equal_pos == len - 1) {
				continue;
			}
			line[equal_pos] = '\0';
			if (section_node->hook == NULL ) {
				section_node->hook = get_new_node('$', ROOT_TRIE_LEVEL);
			}
			var_node = search_trie(section_node->hook, line);
			if (var_node->hook != NULL ) {
				free(var_node->hook);
			}
			if (line[len - 1] == '\n') {
				line[len - 1] = '\0';
			}
			var_node->hook = malloc(
					sizeof(char) * ((int) strlen(&line[equal_pos + 1]) + 1));
			strcpy(var_node->hook, &line[equal_pos + 1]);
		}
	}
	fclose(pconf);

	return conf;
}

int apply_config_i(int *target, Node *conf, const char *section,
		const char *var, const char *default_var) {
	char *raw = get_config(conf, section, var, default_var);
	char *ep = NULL;
	errno = 0;
	*target = strtol(raw, &ep, 10);
	if (errno != 0) {
		log_critical("%s %s is not valid %s", section, var, raw);
		return 0;
	}
	log_debug("Config %s %s %d", section, var, *target);
	return 1;
}

int apply_config_d(double *target, Node *conf, const char *section,
		const char *var, const char *default_var) {
	char *raw = get_config(conf, section, var, default_var);
	char *ep = NULL;
	errno = 0;
	*target = strtod(raw, &ep);
	if (errno != 0) {
		log_critical("%s %s is not valid %s", section, var, raw);
		return 0;
	}
	log_debug("Config %s %s %f", section, var, *target);
	return 1;
}

int apply_config_s(char **target, Node *conf, const char *section,
		const char *var, const char *default_var) {
	*target = get_config(conf, section, var, default_var);
	if (*target == NULL ) {
		log_critical("%s %s is NULL", section, var);
		return 0;
	}
	log_debug("Config %s %s %s", section, var, *target);
	return 1;
}

int apply_config_log(LogLevel *target, Node *conf, const char *section,
		const char *var, const char *default_var) {
	char *raw = get_config(conf, section, var, default_var);
	int i = 0;
	for (i = 0; i < (int) CRITICAL; i++) {
		if (strcmp(LogLevelNames[i], raw) == 0) {
			*target = (LogLevel) i;
			log_debug("Config %s %s %s", section, var, LogLevelNames[i]);
			return 1;
		}
	}
	log_critical("%s %s %s invalid", section, var, raw);
	return 0;
}

int apply_config_ss(char ***target, int *count, char *sep, Node *conf,
		const char *section, const char *var, const char *default_var) {
	char *raw = get_config(conf, section, var, default_var);
	int i = 0;
	int j = 0;
	int delta = 0;
	int len = strlen(raw);
	if (len == 0) {
		return 0;
	}
	for (i = 0, (*count) = 1; i < len; i++) {
		if (raw[i] == ',') {
			(*count)++;
		}
	}
	*target = malloc(sizeof(char*) * (*count));
	for (i = 0, j = 0; i < len; i = i + delta + 1) {
		delta = strcspn(&raw[i], sep);
		(*target)[j] = malloc(sizeof(char) * (delta + 1));
		strncpy((*target)[j], &raw[i], delta);
		(*target)[j][delta] = '\0';
		log_debug("Config %s %s %d %s", section, var, j + 1, (*target)[j]);
		j++;
	}
	return 1;
}

char *get_config(Node *config, const char *section, const char *var,
		const char *default_var) {
	if (config != NULL || section != NULL || var != NULL ) {
		Node *section_node = search_trie(config, section);
		if (section_node->hook != NULL ) {
			Node *var_node = search_trie(section_node->hook, var);
			if (var_node->hook != NULL ) {
				return (char*) var_node->hook;
			}
		}
	}
	int len = strlen(default_var);
	char *buf = malloc(sizeof(char) * (len + 1));
	strcpy(buf, default_var);
	buf[len] = '\0';
	return buf;
}
