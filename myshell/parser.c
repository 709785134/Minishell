#include "parser.h"
#include "env.h"
#include "executor.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#define MAX_LINE 256
#define MAX_BLOCK 100

// Enhanced condition evaluation to handle string comparisons and file tests
static int eval_condition(const char* condition) {
    if (!condition) return 0;
    
    // Parse condition in format like: [ "$var" = "value" ] or [ $var -eq 5 ]
    char temp[MAX_LINE];
    strcpy(temp, condition);

    // Remove brackets if present
    char* start = temp;
    char* end = temp + strlen(temp) - 1;
    while (*start == ' ' || *start == '[') start++;
    while (end > start && (*end == ' ' || *end == ']')) end--;
    *(end + 1) = '\0';

    // Tokenize the condition
    char* token1 = strtok(start, " ");
    if (!token1) return 0;
    
    char* token2 = strtok(NULL, " ");
    if (!token2) return 0;
    
    char* token3 = strtok(NULL, " ");
    if (!token3) return 0;

    char val1[64], val2[64];
    if (token1[0] == '$') {
        // Process the token to handle variable substitution
        char temp_token1[MAX_LINE];
        strcpy(temp_token1, token1);
        replace_vars(temp_token1);
        strcpy(val1, temp_token1);
    }
    else {
        // Remove quotes if present
        if ((token1[0] == '"' || token1[0] == '\'') &&
            token1[strlen(token1) - 1] == token1[0]) {
            strcpy(val1, token1 + 1);
            val1[strlen(val1) - 1] = '\0';
        }
        else {
            strcpy(val1, token1);
        }
    }

    if (token3[0] == '$') {
        // Process the token to handle variable substitution
        char temp_token3[MAX_LINE];
        strcpy(temp_token3, token3);
        replace_vars(temp_token3);
        strcpy(val2, temp_token3);
    }
    else {
        // Remove quotes if present
        if ((token3[0] == '"' || token3[0] == '\'') &&
            token3[strlen(token3) - 1] == token3[0]) {
            strcpy(val2, token3 + 1);
            val2[strlen(val2) - 1] = '\0';
        }
        else {
            strcpy(val2, token3);
        }
    }

    // Compare based on operator
    if (strcmp(token2, "=") == 0 || strcmp(token2, "==") == 0) {
        return strcmp(val1, val2) == 0;
    }
    else if (strcmp(token2, "!=") == 0) {
        return strcmp(val1, val2) != 0;
    }
    else if (strcmp(token2, "-eq") == 0) {
        return atoi(val1) == atoi(val2);
    }
    else if (strcmp(token2, "-ne") == 0) {
        return atoi(val1) != atoi(val2);
    }
    else if (strcmp(token2, "-gt") == 0) {
        return atoi(val1) > atoi(val2);
    }
    else if (strcmp(token2, "-lt") == 0) {
        return atoi(val1) < atoi(val2);
    }
    else if (strcmp(token2, "-ge") == 0) {
        return atoi(val1) >= atoi(val2);
    }
    else if (strcmp(token2, "-le") == 0) {
        return atoi(val1) <= atoi(val2);
    }

    return 0;
}

// Simple arithmetic expression evaluator
static int evaluate_simple_arithmetic(const char* expr) {
    if (!expr) return 0;
    char expr_copy[MAX_LINE];
    strcpy(expr_copy, expr);

    int result = 0;
    char* token = strtok(expr_copy, " ");

    if (token) {
        result = atoi(token);
        token = strtok(NULL, " ");

        while (token) {
            if (strcmp(token, "+") == 0) {
                token = strtok(NULL, " ");
                if (token) result += atoi(token);
            }
            else if (strcmp(token, "-") == 0) {
                token = strtok(NULL, " ");
                if (token) result -= atoi(token);
            }
            else if (strcmp(token, "*") == 0) {
                token = strtok(NULL, " ");
                if (token) result *= atoi(token);
            }
            else if (strcmp(token, "/") == 0) {
                token = strtok(NULL, " ");
                if (token) {
                    int divisor = atoi(token);
                    if (divisor != 0) result /= divisor;
                }
            }
            token = strtok(NULL, " ");
        }
    }

    return result;
}

// Check if a line is an arithmetic assignment
static int is_arithmetic_assignment(const char* line) {
    if (!line) return 0;
    // Check for pattern: variable=$((expression))
    char* eq = strchr(line, '=');
    if (!eq) return 0;

    char* arith_start = strstr(eq, "$((");
    if (!arith_start || arith_start != eq + 1) return 0;

    return 1;
}

// Process arithmetic assignment
static void process_arithmetic_assignment(const char* line) {
    if (!line) return;
    char temp_line[MAX_LINE];
    strcpy(temp_line, line);

    char* eq = strchr(temp_line, '=');
    if (!eq) return;

    *eq = '\0';
    char* var_name = temp_line;
    char* value_expr = eq + 1;

    // Trim spaces from variable name
    while (*var_name == ' ' || *var_name == '\t') var_name++;
    char* end = var_name + strlen(var_name) - 1;
    while (end > var_name && (*end == ' ' || *end == '\t')) end--;
    *(end + 1) = '\0';

    // Check if this is arithmetic expression $((expr))
    if (strncmp(value_expr, "$((", 3) == 0) {
        char* expr_start = value_expr + 3;
        char* expr_end = strstr(expr_start, "))");
        if (expr_end) {
            *expr_end = '\0';

            // Replace variables in arithmetic expression
            char processed_expr[MAX_LINE];
            replace_arithmetic_vars(expr_start, processed_expr);

            // Evaluate arithmetic
            int result = evaluate_simple_arithmetic(processed_expr);

            char result_str[32];
            sprintf(result_str, "%d", result);
            set_var(var_name, result_str);
        }
    }
}

// Function to split a command line by semicolons for sequential execution
static void execute_sequential_commands(char* line) {
    if (!line) return;
    // Make a copy of the line to work with
    char work_line[MAX_LINE];
    strcpy(work_line, line);

    // Find the first semicolon to split commands
    char* cmd = strtok(work_line, ";");
    while (cmd != NULL) {
        // Trim leading/trailing spaces
        while (*cmd == ' ' || *cmd == '\t') cmd++;
        char* end = cmd + strlen(cmd) - 1;
        while (end > cmd && (*end == ' ' || *end == '\t')) end--;
        *(end + 1) = '\0';

        if (strlen(cmd) > 0) {
            char temp_cmd[MAX_LINE];
            strcpy(temp_cmd, cmd);
            replace_vars(temp_cmd);
            exec_cmd(temp_cmd);
        }
        cmd = strtok(NULL, ";");
    }
}

// Function to execute conditional commands with && and ||
static void execute_conditional_commands(char* line) {
    if (!line) return;
    // First check if this is an arithmetic assignment
    if (is_arithmetic_assignment(line)) {
        process_arithmetic_assignment(line);
        return;
    }

    // Check for && first
    char temp_line[MAX_LINE];
    strcpy(temp_line, line);

    char* and_pos = strstr(temp_line, "&&");
    if (and_pos) {
        *and_pos = '\0';
        and_pos += 2;

        // Trim spaces
        char* cmd1 = temp_line;
        while (*cmd1 == ' ' || *cmd1 == '\t') cmd1++;
        char* end = cmd1 + strlen(cmd1) - 1;
        while (end > cmd1 && (*end == ' ' || *end == '\t')) end--;
        *(end + 1) = '\0';

        char* cmd2 = and_pos;
        while (*cmd2 == ' ' || *cmd2 == '\t') cmd2++;

        if (strlen(cmd1) > 0) {
            char temp_cmd1[MAX_LINE];
            strcpy(temp_cmd1, cmd1);
            execute_sequential_commands(temp_cmd1);
        }

        // Execute second command only if first succeeded
        if (get_exit_status() == 0) {
            if (strlen(cmd2) > 0) {
                char temp_cmd2[MAX_LINE];
                strcpy(temp_cmd2, cmd2);
                execute_conditional_commands(temp_cmd2);
            }
        }
        return;
    }

    // Check for ||
    char* or_pos = strstr(temp_line, "||");
    if (or_pos) {
        *or_pos = '\0';
        or_pos += 2;

        // Trim spaces
        char* cmd1 = temp_line;
        while (*cmd1 == ' ' || *cmd1 == '\t') cmd1++;
        char* end = cmd1 + strlen(cmd1) - 1;
        while (end > cmd1 && (*end == ' ' || *end == '\t')) end--;
        *(end + 1) = '\0';

        char* cmd2 = or_pos;
        while (*cmd2 == ' ' || *cmd2 == '\t') cmd2++;

        if (strlen(cmd1) > 0) {
            char temp_cmd1[MAX_LINE];
            strcpy(temp_cmd1, cmd1);
            execute_sequential_commands(temp_cmd1);
        }

        // Execute second command only if first failed
        if (get_exit_status() != 0) {
            if (strlen(cmd2) > 0) {
                char temp_cmd2[MAX_LINE];
                strcpy(temp_cmd2, cmd2);
                execute_conditional_commands(temp_cmd2);
            }
        }
        return;
    }

    // If no && or ||, execute normally
    execute_sequential_commands(line);
}

// Check if a line is a comment
static int is_comment(const char* line) {
    if (!line) return 0;
    const char* p = line;
    while (*p == ' ' || *p == '\t') p++;
    return *p == '#';
}

// Enhanced function to handle if-elif-else-fi structures
static void handle_if_statement(FILE* fp) {
    if (!fp) return;
    char line[MAX_LINE];
    char condition[MAX_LINE];

    // Read the condition line
    if (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\r\n")] = 0;

        // Extract condition - it should be on this line
        char* condition_start = line;
        while (*condition_start == ' ') condition_start++;

        // Find 'then' to extract condition (could be 'then' or '; then')
        char* then_pos = strstr(condition_start, "; then");
        if (then_pos) {
            *then_pos = '\0';
            strcpy(condition, condition_start);
        }
        else {
            then_pos = strstr(condition_start, "then");
            if (then_pos) {
                *then_pos = '\0';
                strcpy(condition, condition_start);
            }
            else {
                strcpy(condition, condition_start);
                // Read next line for 'then'
                if (fgets(line, sizeof(line), fp)) {
                    line[strcspn(line, "\r\n")] = 0;
                }
            }
        }
        
        // Remove 'if' from the beginning of the condition if present
        if (strncmp(condition, "if", 2) == 0 && (condition[2] == ' ' || condition[2] == '\t')) {
            char* temp = condition + 2;
            while (*temp == ' ' || *temp == '\t') temp++;
            strcpy(condition, temp);
        }
    }

    char block[MAX_BLOCK][MAX_LINE];
    int block_len = 0;
    int elif_found = 0;
    int else_found = 0;

    // Read commands until elif, else, or fi
    while (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\r\n")] = 0;

        if (strncmp(line, "elif", 4) == 0) {
            elif_found = 1;
            break;
        }
        if (strcmp(line, "else") == 0) {
            else_found = 1;
            break;
        }
        if (strcmp(line, "fi") == 0) {
            if (eval_condition(condition)) {
                for (int i = 0; i < block_len; i++) {
                    execute_conditional_commands(block[i]);
                }
            }
            return;
        }

        strcpy(block[block_len++], line);
    }

    // Execute the if block if condition is true
    if (eval_condition(condition)) {
        for (int i = 0; i < block_len; i++) {
            execute_conditional_commands(block[i]);
        }
        // Skip to 'fi' - consume the rest of the if-elif-else-fi structure
        int nested_level = 1;
        while (nested_level > 0 && fgets(line, sizeof(line), fp)) {
            line[strcspn(line, "\r\n")] = 0;
            if (strcmp(line, "fi") == 0) {
                nested_level--;
            }
            else if (strncmp(line, "if", 2) == 0 && (line[2] == ' ' || line[2] == '\0')) {
                // Handle nested if statements
                nested_level++;
                // Need to recursively handle the nested if statement
                // This is complex, so we'll just continue reading until all nested if's are closed
            }
            else if (nested_level == 1 && (strncmp(line, "elif", 4) == 0 || strcmp(line, "else") == 0)) {
                // These are part of the current if structure, not nested ones
                // Continue reading until we find fi
            }
        }
    }
    else {
        // Condition was false
        if (elif_found) {
            handle_if_statement(fp);
        }
        else if (else_found) {
            block_len = 0;
            while (fgets(line, sizeof(line), fp)) {
                line[strcspn(line, "\r\n")] = 0;
                if (strcmp(line, "fi") == 0) break;
                strcpy(block[block_len++], line);
            }
            for (int i = 0; i < block_len; i++) {
                execute_conditional_commands(block[i]);
            }
        }
        else {
            // Skip to 'fi'
            int nested_level = 1;
            while (nested_level > 0 && fgets(line, sizeof(line), fp)) {
                line[strcspn(line, "\r\n")] = 0;
                if (strcmp(line, "fi") == 0) {
                    nested_level--;
                }
                else if (strncmp(line, "if", 2) == 0 && (line[2] == ' ' || line[2] == '\0')) {
                    nested_level++;
                }
            }
        }
    }
}

// Check if a line is a case pattern (ends with ')')
static int is_case_pattern(const char* line) {
    if (!line) return 0;
    char temp[MAX_LINE];
    strcpy(temp, line);

    // Trim spaces
    char* p = temp;
    while (*p == ' ' || *p == '\t') p++;

    // Check if line contains ')' 
    return strchr(p, ')') != NULL;
}

// Enhanced function to handle case statements
static void handle_case_statement(FILE* fp) {
    if (!fp) return;
    char line[MAX_LINE];
    char case_expr[MAX_LINE] = "";

    // Read the case expression line (e.g., "case $name in")
    if (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\r\n")] = 0;
        strcpy(case_expr, line);
    }

    // Extract variable from "case $name in"
    char* case_start = case_expr;
    while (*case_start == ' ') case_start++;
    if (strncmp(case_start, "case ", 5) == 0) {
        case_start += 5;
    }

    char* in_pos = strstr(case_start, " in");
    if (!in_pos) {
        in_pos = strstr(case_start, "in");
    }
    if (!in_pos) return;

    *in_pos = '\0';
    char case_var[64];
    strcpy(case_var, case_start);

    // Trim trailing spaces
    char* end = case_var + strlen(case_var) - 1;
    while (end > case_var && (*end == ' ' || *end == '\t')) {
        *end = '\0';
        end--;
    }

    // Get the value to match
    char case_value[64] = "";
    if (case_var[0] == '$') {
        const char* value = get_var(case_var + 1);
        if (value && strlen(value) > 0) {
            strcpy(case_value, value);
        }
    }
    else {
        strcpy(case_value, case_var);
    }

    int pattern_matched = 0;
    char block[MAX_BLOCK][MAX_LINE];
    int block_len = 0;
    int in_pattern_block = 0;

    // Process case options
    while (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\r\n")] = 0;

        // Check for end of case
        if (strcmp(line, "esac") == 0) {
            break;
        }

        // Skip empty lines
        if (strlen(line) == 0) continue;

        // Check if this is a pattern line
        if (is_case_pattern(line) && !in_pattern_block) {
            char* pattern_end = strchr(line, ')');
            if (pattern_end) {
                *pattern_end = '\0';
                char* pattern = line;

                while (*pattern == ' ' || *pattern == '\t') pattern++;

                // Remove quotes
                int len = strlen(pattern);
                if (len >= 2 &&
                    ((pattern[0] == '"' && pattern[len - 1] == '"') ||
                        (pattern[0] == '\'' && pattern[len - 1] == '\''))) {
                    pattern[len - 1] = '\0';
                    pattern++;
                }

                // Check if pattern matches
                if (!pattern_matched &&
                    (strcmp(pattern, case_value) == 0 || strcmp(pattern, "*") == 0)) {
                    pattern_matched = 1;
                    in_pattern_block = 1;
                    block_len = 0;

                    // Check for command on same line
                    char* cmd_after = pattern_end + 1;
                    while (*cmd_after == ' ' || *cmd_after == '\t') cmd_after++;

                    if (strlen(cmd_after) > 0 && strcmp(cmd_after, ";;") != 0) {
                        strcpy(block[block_len++], cmd_after);
                    }
                }
            }
        }
        else if (strcmp(line, ";;") == 0) {
            // End of current case block
            if (in_pattern_block && pattern_matched) {
                // Execute the matched block
                for (int i = 0; i < block_len; i++) {
                    execute_conditional_commands(block[i]);
                }
                // Skip to esac (esac line will be consumed by the loop condition)
                while (fgets(line, sizeof(line), fp)) {
                    line[strcspn(line, "\r\n")] = 0;
                    if (strcmp(line, "esac") == 0) break;
                }
                // The function will return naturally after processing all case options
            }
            in_pattern_block = 0;
            block_len = 0;
        }
        else if (in_pattern_block) {
            // Collect commands in matched block
            strcpy(block[block_len++], line);
        }
    }
}

void interpret(FILE* fp) {
    if (!fp) return;
    char line[MAX_LINE];
    char block[MAX_BLOCK][MAX_LINE];
    int block_len;

    init_special_vars();

    while (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\r\n")] = 0;
        if (strlen(line) == 0) continue;

        if (is_comment(line)) continue;

        // Variable assignment
        if (strchr(line, '=') &&
            strncmp(line, "if", 2) != 0 &&
            strncmp(line, "for", 3) != 0 &&
            strncmp(line, "while", 5) != 0 &&
            strncmp(line, "export", 6) != 0) {

            if (is_arithmetic_assignment(line)) {
                process_arithmetic_assignment(line);
                continue;
            }

            // Regular assignment
            char* eq = strchr(line, '=');
            if (eq) {
                *eq = '\0';
                char* var_name = line;
                char* value = eq + 1;

                // Trim variable name
                while (*var_name == ' ' || *var_name == '\t') var_name++;
                char* end = var_name + strlen(var_name) - 1;
                while (end > var_name && (*end == ' ' || *end == '\t')) end--;
                *(end + 1) = '\0';

                // Process value - remove surrounding quotes if present
                char processed_value[MAX_LINE];
                strcpy(processed_value, value);

                // Check for quotes and remove them
                int len = strlen(processed_value);
                if (len >= 2 &&
                    ((processed_value[0] == '"' && processed_value[len - 1] == '"') ||
                        (processed_value[0] == '\'' && processed_value[len - 1] == '\''))) {
                    processed_value[len - 1] = '\0';
                    strcpy(processed_value, processed_value + 1);
                }

                // Then do variable substitution
                replace_vars(processed_value);
                set_var(var_name, processed_value);
            }
            continue;
        }

        // Handle if statements
        if (strncmp(line, "if", 2) == 0 && (line[2] == ' ' || line[2] == '\0')) {
            handle_if_statement(fp);
            continue;
        }

        // Handle for loops
        if (strncmp(line, "for", 3) == 0) {
            char var[32];
            char values[10][32];
            int vcount = 0;

            char temp_line[MAX_LINE];
            strcpy(temp_line, line);

            char* do_pos = strstr(temp_line, "do");
            char* semicolon_pos = strchr(temp_line, ';');

            if (semicolon_pos && do_pos) {
                *do_pos = '\0';
            }

            char* token = strtok(temp_line, " \t");
            if (token && strcmp(token, "for") == 0) {
                token = strtok(NULL, " \t");
                if (token) {
                    strcpy(var, token);
                    token = strtok(NULL, " \t");
                    if (token && strcmp(token, "in") == 0) {
                        while ((token = strtok(NULL, " \t")) != NULL) {
                            if (strcmp(token, ";") == 0 || strcmp(token, "do") == 0) break;
                            int len = strlen(token);
                            if (len > 0 && token[len - 1] == ';') {
                                token[len - 1] = '\0';
                            }
                            strcpy(values[vcount++], token);
                        }
                    }
                }
            }

            if (!do_pos) {
                while (fgets(line, sizeof(line), fp)) {
                    line[strcspn(line, "\r\n")] = 0;
                    if (strcmp(line, "do") == 0) break;
                    if (strcmp(line, "done") == 0) return;
                }
            }

            block_len = 0;
            while (fgets(line, sizeof(line), fp)) {
                line[strcspn(line, "\r\n")] = 0;
                if (strcmp(line, "done") == 0) break;
                strcpy(block[block_len++], line);
            }

            for (int i = 0; i < vcount; i++) {
                set_var(var, values[i]);
                for (int j = 0; j < block_len; j++) {
                    char temp[MAX_LINE];
                    strcpy(temp, block[j]);
                    execute_conditional_commands(temp);
                }
            }
            continue;
        }

        // Handle while loops
        if (strncmp(line, "while", 5) == 0) {
            char temp_line[MAX_LINE];
            strcpy(temp_line, line);
            char* condition_start = temp_line + 5;
            while (*condition_start == ' ') condition_start++;

            char condition[MAX_LINE];
            char* do_pos = strstr(condition_start, "do");
            if (do_pos) {
                *do_pos = '\0';
                strcpy(condition, condition_start);
            }
            else {
                strcpy(condition, condition_start);
            }

            if (!do_pos) {
                while (fgets(line, sizeof(line), fp)) {
                    line[strcspn(line, "\r\n")] = 0;
                    if (strcmp(line, "do") == 0) break;
                    if (strcmp(line, "done") == 0) return;
                }
            }

            block_len = 0;
            while (fgets(line, sizeof(line), fp)) {
                line[strcspn(line, "\r\n")] = 0;
                if (strcmp(line, "done") == 0) break;
                strcpy(block[block_len++], line);
            }

            int iteration_count = 0;
            const int MAX_ITERATIONS = 1000;

            while (iteration_count++ < MAX_ITERATIONS) {
                char current_condition[MAX_LINE];
                strcpy(current_condition, condition);
                replace_vars(current_condition);

                if (!eval_condition(current_condition)) break;

                for (int j = 0; j < block_len; j++) {
                    char temp[MAX_LINE];
                    strcpy(temp, block[j]);
                    execute_conditional_commands(temp);
                }
            }
            continue;
        }

        // Handle case statements
        if (strncmp(line, "case", 4) == 0) {
            handle_case_statement(fp);
            continue;
        }

        // Handle command execution
        execute_conditional_commands(line);
    }
}