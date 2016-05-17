#define _GNU_SOURCE

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>

#define MAX_HISTORY 10
#define MAX_ARGS 20

/**
 * A struct representing a history item
 *
 * Stores its own index in the history list,
 * the number of arguments it has and the
 * actual command and its arguments.
 */
typedef struct {
    long int commandIndex;
    int argCount;
    char *args[MAX_ARGS];
    int valid;
} historyItem;

/**
 * A struct representing a process that's
 * running in the background
 */
typedef struct backgroundItem {
    long int pid;
    char *command;
    struct backgroundItem *next;
    struct backgroundItem *prev;
} backgroundItem;

// Create the command history
historyItem commandHistory[MAX_HISTORY];

backgroundItem *backgroundProcess;

/**
 * Get input from the command line and tokenize it
 * Print the prompt
 * Store the command and its arguments in args
 * Find out if the command needs to be in the background or not
 */
int getcmd(char *prompt, char *args[], int *background) {
    int length, i = 0;
    char *token, *loc;
    char *line = NULL;
    size_t linecap = 0;

    printf("%s", prompt);
    length = getline(&line, &linecap, stdin);

    if (length <= 0) {
        exit(-1);
    }

    // Check if background is specified..
    if ((loc = index(line, '&')) != NULL) {
        *background = 1;
        *loc = ' ';
    } else
        *background = 0;

    while ((token = strsep(&line, " \t\n")) != NULL) {
        for (int j = 0; j < strlen(token); j++)
            if (token[j] <= 32)
                token[j] = '\0';
        if (strlen(token) > 0)
            args[i++] = token;
    }

    // This is needed for execvp
    args[i] = '\0';

    // Free the memory allocated by getline
    free(line);

    return i;
}

void insertIntoHistory(historyItem *commandHistory, long int *historyIndex, char *args[], int argCount) {
    int index = (*historyIndex - 1) % MAX_HISTORY;
    commandHistory[index].commandIndex = *historyIndex;
    int i = 0;
    for (i; i < argCount; i++)
        commandHistory[index].args[i] = args[i];
    commandHistory[index].args[i] = '\0';
    commandHistory[index].argCount = argCount;

    *historyIndex += 1;
}

/**
 * Tries to fork and execute a command in the child process
 * Returns 1 (true) if the command was successful or 0 (false) otherwise
 */
int executeCommand(char *args[], int bg) {
    pid_t pid;

    // Try to fork
    if ((pid = fork()) < 0) {
        printf("Could not create child process, exiting...\n");
        exit(1);
    }

    // Fork successful
    if (pid > 0) {
        // parent
        if (bg) {
//            printf("Background enabled\n");
            backgroundItem *bgProcess = malloc(sizeof(backgroundItem));
            bgProcess->pid = pid;
            bgProcess->next = NULL;
            bgProcess->prev = NULL;
            asprintf(&bgProcess->command, "%s", args[0]);

            // First node
            if (backgroundProcess == NULL)
                backgroundProcess = bgProcess;
            else {
                backgroundItem *trav = backgroundProcess;
                // Find the last node in the list
                while (1) {
                    if (trav->next == NULL)
                        break;
                    trav = trav->next;
                }
                trav->next = bgProcess;
                bgProcess->prev = trav;
            }
            // Assume background processes are always correct
            return 1;
        } else {
//            printf("Background not enabled\n");
            int status;
            waitpid(pid, &status, 0);
            // There is no way to differentiate between a command that wasn't found
            // and a command that was run with invalid parameters. Both of them return
            // non-zero exit codes but the latter is valid and should be saved
            // to the history. The following is a compromise:
            if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
                return 1;
            return 0;
        }
    } else if (pid == 0) {
        // child
        if (execvp(args[0], args) < 0) {
            printf("%s: command not found\n", args[0]);
            exit(1);
        }
    }

    return 0;
}

/**
 * Returns 1 (true) if a built-in command was run
 */
int executeBuiltInCommand(char *args[], int bg) {
    if (strcmp(args[0], "exit") == 0)
        exit(0);
    else if (strcmp(args[0], "history") == 0) {
        for (int i = 0; i < MAX_HISTORY; ++i) {
            if (commandHistory[i].commandIndex == -1)
                break;
            printf("  %ld  ", commandHistory[i].commandIndex);
            for (int j = 0; j < commandHistory[i].argCount; j++)
                printf("%s ", commandHistory[i].args[j]);
            printf("\n");
        }
        return 1;
    }
    else if (strcmp(args[0], "cd") == 0) {
        // Command
        chdir(args[1]);
        return 1;
    } else if (strcmp(args[0], "pwd") == 0) {
        // Command
        char cwd[1024];
        getcwd(cwd, sizeof(cwd));
        printf("%s\n", cwd);
        return 1;
    } else if (strcmp(args[0], "fg") == 0) {
        // get pid arg
        if (args[1] == NULL || strtol(args[1], NULL, 10) == 0L)
            return 1;
        long int pid = strtol(args[1], NULL, 10);
        waitpid(pid, NULL, 0);
        return 1;
    } else if (strcmp(args[0], "jobs") == 0) {
        backgroundItem *trav = backgroundProcess;
        if (trav == NULL)
            return 1;
        do {
            printf("  %ld  %s\n", trav->pid, trav->command);
            trav = trav->next;
        } while (trav != NULL);
        return 1;
    }
    return 0;
}

void signalHandler(int signal) {
    pid_t pid;
    pid = wait(NULL);
    if (pid > 0) {
        backgroundItem *trav = backgroundProcess;
        if (trav != NULL) {
            do {
                if (trav->pid == pid) {
                    // remove the bg process
                    if (trav->prev != NULL) {
                        trav->prev->next = trav->next;
                    } else {
                        backgroundProcess = trav->next;
                    }
                    if (trav->next != NULL) {
                        trav->next->prev = trav->prev;
                    }
                    free(trav->command);
                    free(trav);
                    break;
                }
                trav = trav->next;
            } while (trav != NULL);
        }
    }
}

/**
 * Initializes default values for all history items
 */
void initializeHistory(historyItem *commandHistory, int size) {
    for (int i = 0; i < size; ++i) {
        commandHistory[i].commandIndex = -1;
        commandHistory[i].argCount = 0;
        commandHistory[i].valid = 1;
    }
}

int main() {

    // the command + arguments
    char *args[MAX_ARGS];

    // background flag
    int bg;

    // Backup stdout a& stderr descriptors
    int stdout_copy = dup(fileno(stdout));
    int stderr_copy = dup(fileno(stderr));

    // Flag indication an output redirection was requested
    int outputRedirection;

    // Output file
    char *output;

    // File descriptors for output redirection
    int fileOut;
    int fileErr;

    // The last index of the history
    long int historyIndex = 1;

    signal(SIGCHLD, signalHandler);

    // Initialize the command history
    initializeHistory(commandHistory, MAX_HISTORY);

    backgroundProcess = NULL;

    // Keep waiting for commands until the user wants to exit
    while (1) {

        // Restore descriptors
        if (outputRedirection) {
            close(fileOut);
            close(fileErr);
            dup2(stdout_copy, fileno(stdout));
            dup2(stderr_copy, fileno(stderr));

            outputRedirection = 0;
        }

        bg = 0;
        int cnt = getcmd(">>  ", args, &bg);

//        // Print arguments
//        for (int i = 0; i < cnt; i++) {
//            printf("Arg[%d] = %s\n", i, args[i]);
//        }

        // Check for redirection
        for (int i = 0; i < cnt; i++) {
            if (strcmp(args[i], ">") == 0 && args[i + 1] != NULL) {
                outputRedirection = 1;
                output = args[i + 1];
                // chop off command
                args[i] = '\0';
                cnt = i;
                break;
            }
        }

        if (outputRedirection) {
            close(fileno(stdout));
            close(fileno(stderr));
            if ((fileOut = open(output, O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR)) == -1) {
                \
                exit(1);
            }
            fileErr = dup(fileOut);
        }

        insertIntoHistory(commandHistory, &historyIndex, args, cnt);
//        printf("command inserted at index %ld : %s\n", historyIndex - 2, commandHistory[historyIndex - 2].args[0]);

        // Built-in commands
        if (executeBuiltInCommand(args, bg))
            continue;

        // History feature
        // Check if the user inputted a history number
        long int index = strtol(args[0], NULL, 10);
        if (index != 0L) {
            // Check if that index exists
            int i = 0;
            for (i; i < MAX_HISTORY; i++)
                if (commandHistory[i].commandIndex == index)
                    break; // command found
            if (i < 10) {
                // command found

//                printf("command found at index %d with history index %ld : %s\n", i, commandHistory[i].commandIndex, commandHistory[i].args[0]);

                if (commandHistory[i].valid) {
                    // History command was a built-in command
                    if (executeBuiltInCommand(commandHistory[i].args, bg)) {
                        insertIntoHistory(commandHistory, &historyIndex, commandHistory[i].args,
                                          commandHistory[i].argCount);
                        continue;
                    }
                    if (executeCommand(commandHistory[i].args, bg)) {
                        // history command was successful, insert into history again
                        insertIntoHistory(commandHistory, &historyIndex, commandHistory[i].args,
                                          commandHistory[i].argCount);
//                        printf("command inserted at index %ld : %s\n", historyIndex - 2, commandHistory[historyIndex - 2].args[0]);
                    }
                } else
                    printf("History command erroneous\n");
                continue;
            } else {
                printf("No command found in history\n");
                continue;
            }
        }

        if (!executeCommand(args, bg))
            // Flag command as erroneous
            commandHistory[historyIndex - 2].valid = 0;

    }
}
