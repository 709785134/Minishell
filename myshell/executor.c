#include "executor.h"
#include "env.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#endif

#define MAX_LINE 256

// Function to update exit status after command execution
extern void update_exit_status(int status);

// Check if command is a built-in command
static int is_builtin_cmd(const char* cmd) {
    if (strncmp(cmd, "echo", 4) == 0 && (cmd[4] == ' ' || cmd[4] == '\0')) return 1;
    if (strncmp(cmd, "cd", 2) == 0 && (cmd[2] == ' ' || cmd[2] == '\0')) return 1;
    if (strncmp(cmd, "pwd", 3) == 0 && (cmd[3] == ' ' || cmd[3] == '\0')) return 1;
    if (strncmp(cmd, "exit", 4) == 0 && (cmd[4] == ' ' || cmd[4] == '\0')) return 1;
    if (strncmp(cmd, "set", 3) == 0 && (cmd[3] == ' ' || cmd[3] == '\0')) return 1;
    if (strncmp(cmd, "unset", 5) == 0 && (cmd[5] == ' ' || cmd[5] == '\0')) return 1;
    if (strncmp(cmd, "export", 6) == 0 && (cmd[6] == ' ' || cmd[6] == '\0')) return 1;
    if (strncmp(cmd, "read", 4) == 0 && (cmd[4] == ' ' || cmd[4] == '\0')) return 1;
    if (strncmp(cmd, "[", 1) == 0) return 1;
    return 0;
}

// Test command implementation
static int test_command(const char* cmd) {
    char temp[MAX_LINE];
    strcpy(temp, cmd);

    // Remove brackets
    char* expr = temp;
    if (*expr == '[') expr++;

    // Trim spaces
    while (*expr == ' ') expr++;

    // Remove trailing bracket and spaces
    char* end = expr + strlen(expr) - 1;
    while (end > expr && (*end == ' ' || *end == ']')) end--;
    *(end + 1) = '\0';

    // Parse expression: value1 operator value2
    char* tokens[3];
    int token_count = 0;
    char* token = strtok(expr, " ");
    while (token && token_count < 3) {
        tokens[token_count++] = token;
        token = strtok(NULL, " ");
    }

    if (token_count != 3) return 0;

    // Process variables in values
    char processed_val1[MAX_LINE], processed_val2[MAX_LINE];
    strcpy(processed_val1, tokens[0]);
    strcpy(processed_val2, tokens[2]);
    replace_vars(processed_val1);
    replace_vars(processed_val2);

    // Convert to integers
    int num1 = atoi(processed_val1);
    int num2 = atoi(processed_val2);

    // Compare based on operator
    if (strcmp(tokens[1], "-eq") == 0) {
        return num1 == num2;
    }
    else if (strcmp(tokens[1], "-ne") == 0) {
        return num1 != num2;
    }
    else if (strcmp(tokens[1], "-gt") == 0) {
        return num1 > num2;
    }
    else if (strcmp(tokens[1], "-lt") == 0) {
        return num1 < num2;
    }
    else if (strcmp(tokens[1], "-ge") == 0) {
        return num1 >= num2;
    }
    else if (strcmp(tokens[1], "-le") == 0) {
        return num1 <= num2;
    }

    return 0;
}

// Execute built-in commands
static void exec_builtin_cmd(const char* cmd) {
    if (strncmp(cmd, "echo", 4) == 0) {
        const char* text = cmd + 4;
        while (*text == ' ') text++;

        char processed_text[MAX_LINE];
        strcpy(processed_text, text);
        replace_vars(processed_text);

        int len = strlen(processed_text);
        if (len >= 2 &&
            ((processed_text[0] == '"' && processed_text[len - 1] == '"') ||
                (processed_text[0] == '\'' && processed_text[len - 1] == '\''))) {
            processed_text[len - 1] = '\0';
            printf("%s\n", processed_text + 1);
        }
        else {
            printf("%s\n", processed_text);
        }
        update_exit_status(0);
    }
    else if (strncmp(cmd, "[", 1) == 0) {
        int result = test_command(cmd);
        update_exit_status(result ? 0 : 1);
    }
    else if (strncmp(cmd, "pwd", 3) == 0) {
        char buffer[MAX_LINE];
#ifdef _WIN32
        if (_getcwd(buffer, sizeof(buffer)) != NULL) {
#else
        if (getcwd(buffer, sizeof(buffer)) != NULL) {
#endif
            printf("%s\n", buffer);
            update_exit_status(0);
        }
        else {
            perror("pwd");
            update_exit_status(1);
        }
        }
    else if (strncmp(cmd, "exit", 4) == 0) {
        const char* code = cmd + 4;
        while (*code == ' ') code++;
        int exit_code = 0;
        if (strlen(code) > 0) {
            exit_code = atoi(code);
        }
        exit(exit_code);
    }
    else if (strncmp(cmd, "read", 4) == 0) {
        const char* var = cmd + 4;
        while (*var == ' ') var++;

        if (strlen(var) > 0) {
            char input[256];
            if (fgets(input, sizeof(input), stdin)) {
                input[strcspn(input, "\r\n")] = 0;
                extern void set_var(const char*, const char*);
                set_var(var, input);
                update_exit_status(0);
            }
            else {
                update_exit_status(1);
            }
        }
    }
    else if (strncmp(cmd, "cd", 2) == 0) {
        const char* path = cmd + 2;
        while (*path == ' ') path++;
        if (strlen(path) == 0) path = get_var("HOME");

#ifdef _WIN32
        int result = _chdir(path);
#else
        int result = chdir(path);
#endif
        if (result != 0) {
            perror("cd");
            update_exit_status(1);
        }
        else {
            update_exit_status(0);
        }
    }
    else {
        char buffer[MAX_LINE];
        sprintf(buffer, "sh -c %s", cmd);
        int result = system(buffer);
#ifdef _WIN32
        update_exit_status(result == 0 ? 0 : 1);
#else
        update_exit_status(WEXITSTATUS(result));
#endif
    }
    }

// Function to handle command execution
void exec_cmd(const char* cmd) {
    // Check if it's a built-in command
    if (is_builtin_cmd(cmd)) {
        exec_builtin_cmd(cmd);
        return;
    }

    // Create a copy for processing
    char work_cmd[MAX_LINE];
    strcpy(work_cmd, cmd);

    // Check for redirections and pipes
    char* output_redir = strchr(work_cmd, '>');
    char* input_redir = strchr(work_cmd, '<');
    char* pipe_pos = strchr(work_cmd, '|');

    // Handle redirections/pipes
    if (output_redir || input_redir || pipe_pos) {
        char buffer[MAX_LINE];
#ifdef _WIN32
        sprintf(buffer, "cmd /c %s", cmd);
#else
        sprintf(buffer, "%s", cmd);
#endif
        int result = system(buffer);
#ifdef _WIN32
        update_exit_status(result == 0 ? 0 : 1);
#else
        update_exit_status(WEXITSTATUS(result));
#endif
        return;
    }

    // Regular command execution
    char buffer[MAX_LINE];
#ifdef _WIN32
    sprintf(buffer, "cmd /c %s", cmd);
#else
    sprintf(buffer, "%s", cmd);
#endif
    int result = system(buffer);
#ifdef _WIN32
    update_exit_status(result == 0 ? 0 : 1);
#else
    update_exit_status(WEXITSTATUS(result));
#endif
}