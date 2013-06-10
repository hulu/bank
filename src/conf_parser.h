#ifndef CONF_PARSER_H_
#define CONF_PARSER_H_

Node *read_conf_file(const char *file_name);

char *get_config(Node *config, const char *section, const char *var,
		const char *default_var);

int apply_config_i(int *target, Node *conf, const char *section,
		const char *var, const char *default_var);

int apply_config_d(double *target, Node *conf, const char *section,
		const char *var, const char *default_var);

int apply_config_s(char **target, Node *conf, const char *section,
		const char *var, const char *default_var);

int apply_config_log(LogLevel *target, Node *conf, const char *section,
		const char *var, const char *default_var);

int apply_config_ss(char ***target, int *count, char *sep, Node *conf,
		const char *section, const char *var, const char *default_var);

#endif /* _CONF_PARSER_H */
