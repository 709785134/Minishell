#include "env.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>

#define MAX_VARS 100
#define MAX_LINE 256

typedef struct {
    char name[32];
    char value[64];
} Var;

static Var vars[MAX_VARS];
static int var_count = 0;
static int exit_status = 0;
static int process_id = 1234;
static int arg_count = 0;
static char arg_list[256] = "";


const char* get_var(const char* name) {
    // Handle special variables
    if (strcmp(name, "?") == 0) {
        static char status_str[16];
        sprintf(status_str, "%d", exit_status);
        return status_str;
    }
    else if (strcmp(name, "$") == 0) {
        static char pid_str[16];
        sprintf(pid_str, "%d", process_id);
        return pid_str;
    }
    else if (strcmp(name, "#") == 0) {
        static char count_str[16];
        sprintf(count_str, "%d", arg_count);
        return count_str;
    }
    else if (strcmp(name, "*") == 0) {
        return arg_list;
    }
    else if (strcmp(name, "@") == 0) {
        return arg_list;
    }

    // Regular variable lookup
    for (int i = 0; i < var_count; i++) {
        if (strcmp(vars[i].name, name) == 0)
            return vars[i].value;
    }
    return "";
}

void set_var(const char* name, const char* value) {
    // Update exit status if setting $?
    if (strcmp(name, "?") == 0) {
        exit_status = atoi(value);
        return;
    }
    else if (strcmp(name, "#") == 0) {
        arg_count = atoi(value);
        return;
    }
    else if (strcmp(name, "*") == 0 || strcmp(name, "@") == 0) {
        strncpy(arg_list, value, sizeof(arg_list) - 1);
        arg_list[sizeof(arg_list) - 1] = '\0';
        return;
    }

    for (int i = 0; i < var_count; i++) {
        if (strcmp(vars[i].name, name) == 0) {
            strncpy(vars[i].value, value, sizeof(vars[i].value) - 1);
            vars[i].value[sizeof(vars[i].value) - 1] = '\0';
            return;
        }
    }

    if (var_count < MAX_VARS) {
        strncpy(vars[var_count].name, name, sizeof(vars[var_count].name) - 1);
        vars[var_count].name[sizeof(vars[var_count].name) - 1] = '\0';
        strncpy(vars[var_count].value, value, sizeof(vars[var_count].value) - 1);
        vars[var_count].value[sizeof(vars[var_count].value) - 1] = '\0';
        var_count++;
    }
}

void init_special_vars() {
    exit_status = 0;
    process_id = 1234;
    arg_count = 0;
    strcpy(arg_list, "");
}

int get_exit_status() {
    return exit_status;
}

void update_exit_status(int status) {
    exit_status = status;
}

void replace_vars(char* line) {
    char result[MAX_LINE] = "";
    char* src = line;
    char* dest = result;

    while (*src) {
        if (*src == '$') {
            src++; // skip $
            if (*src == '{') {
                // Handle ${var} syntax
                src++; // skip {
                char var_name[32];
                int i = 0;
                while (*src && *src != '}') {
                    if (i < 31) var_name[i++] = *src;
                    src++;
                }
                var_name[i] = '\0';
                if (*src) src++; // skip }

                const char* value = get_var(var_name);
                strcat(result, value);
                dest = result + strlen(result);
            }
            else {
                // Handle $var syntax
                char var_name[32];
                int i = 0;

                // Check for special variables
                if (*src == '?' || *src == '$' || *src == '#' || *src == '*' || *src == '@') {
                    var_name[0] = *src;
                    var_name[1] = '\0';
                    src++;
                }
                else {
                    // Handle regular variable names
                    while (*src && ((unsigned char)*src == '_' ||
                        ((unsigned char)*src >= 'a' && (unsigned char)*src <= 'z') ||
                        ((unsigned char)*src >= 'A' && (unsigned char)*src <= 'Z') ||
                        ((unsigned char)*src >= '0' && (unsigned char)*src <= '9'))) {
                        if (i < 31) var_name[i++] = *src;
                        src++;
                    }
                    var_name[i] = '\0';
                }

                if (strlen(var_name) > 0) {
                    const char* value = get_var(var_name);
                    strcat(result, value);
                    dest = result + strlen(result);
                }
                else {
                    *dest++ = '$';
                }
            }
        }
        else {
            *dest++ = *src++;
        }
    }
    *dest = '\0';
    strcpy(line, result);
}

void replace_arithmetic_vars(const char* input, char* output) {
    char work_expr[MAX_LINE];
    strcpy(work_expr, input);

    char result[MAX_LINE] = "";
    char* src = work_expr;
    char* dest = result;

    while (*src) {
        // Skip spaces
        while (*src == ' ') {
            *dest++ = *src++;
        }

        if (!*src) break;

        unsigned char uc = (unsigned char)*src;

        // Check if this is a variable name
        if ((uc == '_' ||
            (uc >= 'a' && uc <= 'z') ||
            (uc >= 'A' && uc <= 'Z')) &&
            (src == work_expr || !((unsigned char)*(src - 1) >= '0' && (unsigned char)*(src - 1) <= '9')) &&
            (src == work_expr || *(src - 1) != '$')) {

            char var_name[32];
            int i = 0;

            // Extract the full variable name
            while (*src && ((unsigned char)*src == '_' ||
                ((unsigned char)*src >= 'a' && (unsigned char)*src <= 'z') ||
                ((unsigned char)*src >= 'A' && (unsigned char)*src <= 'Z') ||
                ((unsigned char)*src >= '0' && (unsigned char)*src <= '9'))) {
                if (i < 31) var_name[i++] = *src;
                src++;
            }
            var_name[i] = '\0';

            // Check if this is a known variable
            const char* value = get_var(var_name);
            if (value && strlen(value) > 0) {
                strcat(result, value);
                dest = result + strlen(result);
            }
            else {
                strcat(result, "0");
                dest = result + strlen(result);
            }
        }
        else {
            *dest++ = *src++;
        }
    }
    *dest = '\0';
    strcpy(output, result);
}