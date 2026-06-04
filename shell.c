#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>

#define MAX_LINE_LEN 1024
#define MAX_ARGS 64
#define MAX_COMMANDS 16

/*
 * Helper function for preprocessing user input.
 * It inserts spaces around special characters: '|', '<', '>',
 * to simplify subsequent tokenization with strtok.
 */
void preprocess_line(const char *in, char *out, size_t max_len) {
    size_t j = 0;
    for (size_t i = 0; in[i] != '\0' && j < max_len - 3; i++) {
        if (in[i] == '|' || in[i] == '<' || in[i] == '>') {
            // Add a space before the special character if not already present
            if (j > 0 && out[j-1] != ' ') {
                out[j++] = ' ';
            }
            out[j++] = in[i];
            // Add a space after the special character if it is not the end of the line
            if (in[i+1] != ' ' && in[i+1] != '\0') {
                out[j++] = ' ';
            }
        } else {
            out[j++] = in[i];
        }
    }
    out[j] = '\0';
}

/*
 * Structure representing a single command in a pipeline.
 * For example, for "cat < input.txt | grep foo > output.txt"
 * there will be two commands in the array.
 */
typedef struct {
    char *argv[MAX_ARGS];  // Array of arguments for execvp (terminated by NULL)
    int argc;              // Argument count
    char *input_file;      // File name for input redirection (NULL if none)
    char *output_file;     // File name for output redirection (NULL if none)
} Command;

/*
 * Parses a single command (without '|' characters) into arguments and redirection files.
 * Example: "cat < input.txt" -> argv = {"cat", NULL}, input_file = "input.txt"
 */
void parse_single_command(char *cmd_str, Command *cmd) {
    cmd->argc = 0;
    cmd->input_file = NULL;
    cmd->output_file = NULL;

    char *token = strtok(cmd_str, " \t\r\n");
    while (token != NULL && cmd->argc < MAX_ARGS - 1) {
        if (strcmp(token, "<") == 0) {
            token = strtok(NULL, " \t\r\n");
            if (token != NULL) {
                cmd->input_file = token;
            }
        } else if (strcmp(token, ">") == 0) {
            token = strtok(NULL, " \t\r\n");
            if (token != NULL) {
                cmd->output_file = token;
            }
        } else {
            cmd->argv[cmd->argc++] = token;
        }
        token = strtok(NULL, " \t\r\n");
    }
    cmd->argv[cmd->argc] = NULL;
}

/*
 * Executes a pipeline of commands.
 * num_cmds - number of commands in the pipeline (cmd1 | cmd2 | ... | cmdN)
 */
void execute_pipeline(Command cmds[], int num_cmds) {
    int pipefds[2 * (MAX_COMMANDS - 1)];
    pid_t pids[MAX_COMMANDS];

    // Create all necessary pipes
    // For N commands we need N-1 pipes, each having 2 file descriptors (read/write)
    for (int i = 0; i < num_cmds - 1; i++) {
        if (pipe(pipefds + 2 * i) < 0) {
            perror("pipe error");
            return;
        }
    }

    for (int i = 0; i < num_cmds; i++) {
        pids[i] = fork();
        if (pids[i] < 0) {
            perror("fork error");
            return;
        }

        if (pids[i] == 0) { // Code executed in the child process
            // 1. Setup pipe redirections for the pipeline
            // If it is not the first command, redirect standard input from the previous pipe
            if (i > 0) {
                if (dup2(pipefds[2 * (i - 1)], STDIN_FILENO) < 0) {
                    perror("dup2 read pipe error");
                    exit(1);
                }
            }
            // If it is not the last command, redirect standard output to the next pipe
            if (i < num_cmds - 1) {
                if (dup2(pipefds[2 * i + 1], STDOUT_FILENO) < 0) {
                    perror("dup2 write pipe error");
                    exit(1);
                }
            }

            // Close all pipe file descriptor copies in the child process.
            // This is critical! If any write descriptor remains open in the process group,
            // the reading process will never receive EOF and will hang.
            for (int j = 0; j < 2 * (num_cmds - 1); j++) {
                close(pipefds[j]);
            }

            // 2. Input redirection from file (cmd < file)
            if (cmds[i].input_file != NULL) {
                int fd_in = open(cmds[i].input_file, O_RDONLY);
                if (fd_in < 0) {
                    perror("open input file error");
                    exit(1);
                }
                if (dup2(fd_in, STDIN_FILENO) < 0) {
                    perror("dup2 input file error");
                    exit(1);
                }
                close(fd_in);
            }

            // 3. Output redirection to file (cmd > file)
            if (cmds[i].output_file != NULL) {
                int fd_out = open(cmds[i].output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd_out < 0) {
                    perror("open output file error");
                    exit(1);
                }
                if (dup2(fd_out, STDOUT_FILENO) < 0) {
                    perror("dup2 output file error");
                    exit(1);
                }
                close(fd_out);
            }

            // 4. Execute standard utility
            if (cmds[i].argv[0] != NULL) {
                execvp(cmds[i].argv[0], cmds[i].argv);
                // If execvp returns, an error has occurred
                perror("exec error");
            }
            exit(1);
        }
    }

    // Code executed in the parent process: close all pipe file descriptors,
    // as the parent process does not read or write from them directly.
    for (int i = 0; i < 2 * (num_cmds - 1); i++) {
        close(pipefds[i]);
    }

    // Wait for all child processes to finish to avoid zombie processes
    for (int i = 0; i < num_cmds; i++) {
        int status;
        waitpid(pids[i], &status, 0);
    }
}

#ifndef TEST_BUILD
int main() {
    char input_line[MAX_LINE_LEN];
    char preprocessed_line[MAX_LINE_LEN * 2];

    while (1) {
        // Print shell prompt
        printf("shad_shell> ");
        fflush(stdout);

        // Read command line from standard input
        if (fgets(input_line, sizeof(input_line), stdin) == NULL) {
            // EOF (Ctrl+D)
            printf("\n");
            break;
        }

        // Remove newline character
        input_line[strcspn(input_line, "\n")] = '\0';

        // Skip empty inputs
        if (strlen(input_line) == 0) {
            continue;
        }

        // Preprocess input (insert space around special characters)
        preprocess_line(input_line, preprocessed_line, sizeof(preprocessed_line));

        // Split input into commands based on the pipe '|' delimiter
        char *cmd_strings[MAX_COMMANDS];
        int num_cmds = 0;

        char *cmd_token = strtok(preprocessed_line, "|");
        while (cmd_token != NULL && num_cmds < MAX_COMMANDS) {
            cmd_strings[num_cmds++] = cmd_token;
            cmd_token = strtok(NULL, "|");
        }

        // Initialize Command structs for each pipeline stage
        Command cmds[MAX_COMMANDS];
        for (int i = 0; i < num_cmds; i++) {
            parse_single_command(cmd_strings[i], &cmds[i]);
        }

        // Skip if pipeline starts with an empty command
        if (num_cmds == 0 || cmds[0].argv[0] == NULL) {
            continue;
        }

        // Handle shell built-ins (executed in the parent shell process)
        if (strcmp(cmds[0].argv[0], "exit") == 0) {
            break;
        } else if (strcmp(cmds[0].argv[0], "cd") == 0) {
            // Changing directories must be done in the parent process
            if (cmds[0].argv[1] == NULL) {
                // If no arguments, fallback to the HOME directory
                char *home = getenv("HOME");
                if (home != NULL) {
                    chdir(home);
                }
            } else {
                if (chdir(cmds[0].argv[1]) < 0) {
                    perror("cd error");
                }
            }
            continue;
        } else if (strcmp(cmds[0].argv[0], "export") == 0) {
            if (cmds[0].argv[1] == NULL) {
                // Print all environment variables
                extern char **environ;
                for (char **env = environ; *env != NULL; env++) {
                    printf("%s\n", *env);
                }
            } else {
                for (int j = 1; cmds[0].argv[j] != NULL; j++) {
                    char *arg = cmds[0].argv[j];
                    char *equals = strchr(arg, '=');
                    if (equals != NULL) {
                        *equals = '\0';
                        char *name = arg;
                        char *value = equals + 1;
                        if (setenv(name, value, 1) < 0) {
                            perror("export error");
                        }
                    } else {
                        fprintf(stderr, "export: '%s': not a valid identifier\n", arg);
                    }
                }
            }
            continue;
        } else if (strcmp(cmds[0].argv[0], "unset") == 0) {
            if (cmds[0].argv[1] == NULL) {
                fprintf(stderr, "unset: not enough arguments\n");
            } else {
                for (int j = 1; cmds[0].argv[j] != NULL; j++) {
                    if (unsetenv(cmds[0].argv[j]) < 0) {
                        perror("unset error");
                    }
                }
            }
            continue;
        }

        // Execute commands pipeline
        execute_pipeline(cmds, num_cmds);
    }

    return 0;
}
#endif
