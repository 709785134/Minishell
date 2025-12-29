#ifndef ENV_H
#define ENV_H

void set_var(const char* name, const char* value);
const char* get_var(const char* name);
void replace_vars(char* line);
void replace_arithmetic_vars(const char* input, char* output);
void init_special_vars();
int get_exit_status();

#endif