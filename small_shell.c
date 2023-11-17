#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h> // For mode constants
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>    // For open
#include <unistd.h>   // For read, write, close, dup2

#define MAX_CHAR_LENGTH 2048            // Maximum character length
#define MAX_ARGUMENTS 512               // Maximum amount of characters
#define MAX_BACKGROUND_PROCESSES 1000   // Maximum amount of background processes set

#define SIG_INT 2                       
#define SIG_TSTP 20

pid_t background_process_pids[MAX_BACKGROUND_PROCESSES]; // Declare array to store PIDs
int num_background_processes = 0; // Initialize to zero

// Global variable to track foreground-only mode
volatile int foreground_only_mode = 0;

// Function prototypes for built-in commands
void handle_cd(const char *args);
void handle_status(int exit_status);
void handle_exit();
void execute_command(char *args[], int *exit_status, int runInBackground);


// SIGINT handler
void sigint_handler(int signo) {
    // Do nothing (ignore SIGINT in the parent process)

}

// SIGTSTP handler
void sigtstp_handler(int signo) {
    // Toggle the foreground-only mode

    char *message;
    if (foreground_only_mode == 0) {
        message = "\nEntering foreground-only mode (& is now ignored)";
        foreground_only_mode = 1;
    } else {
        message = "\nExiting foreground-only mode";
        foreground_only_mode = 0;
    }
    write(STDERR_FILENO, message, strlen(message));
}

void sigchld_handler(int signo) {
    int saved_errno = errno; // Save current errno
    pid_t pid;
    int status;

   while ((pid = waitpid((pid_t)(-1), &status, WNOHANG)) > 0) {
        // Check if the pid is in the background_process_pids array before printing
        int i;
        for (i = 0; i < num_background_processes; i++) {
            if (background_process_pids[i] == pid) {
                // Found a background process that has completed
                printf("Background pid %d is done: ", pid);
                handle_status(status);
                // Remove pid from the array or mark it as completed
                break;
            }
        }
    }

    errno = saved_errno; // Restore errno
}

// Function to handle the "cd" command
// Function to handle the "cd" command
void handle_cd(const char *path) {
    if (path == NULL) {
        // If no path is given, change to the home directory
        const char* home = getenv("HOME");
        if (home != NULL) {
            if (chdir(home) != 0) {
                perror("chdir");
            }
        } else {
            fprintf(stderr, "cd: HOME environment variable not set.\n");
        }
    } else {
        // If a path is given, attempt to change to that directory
        if (chdir(path) != 0) {
            // If changing directory fails, print an error message
            perror("chdir");
        }
    }
}

// Function to handle the "status" command
void handle_status(int exit_status) {
    if (WIFEXITED(exit_status)) {
        // The child exited normally; print the exit status
        printf("Exit value: %d\n", WEXITSTATUS(exit_status));
    } else if (WIFSIGNALED(exit_status)) {
        // The child process terminated due to a signal; print the signal number
        printf("Terminated by signal: %d\n", WTERMSIG(exit_status));
    }
}

// Function to send a termination signal to a process
void terminate_process(pid_t pid) {
    if (kill(pid, SIGTERM) == -1) {
        // Handle the error if sending the signal fails
        perror("kill");
    }
}
// In your handle_exit function:
void handle_exit() {
    // Iterate through the list of background process PIDs and send SIGTERM
    int i;
    for (i = 0; i < num_background_processes; i++) {
        terminate_process(background_process_pids[i]);
    }

    // Cleanup and exit the shell
    exit(0);
}

void setup_redirection(char *args[], char **input_file, char **output_file) {
    int i;
    for (i = 0; args[i] != NULL; i++) {
        if (strcmp(args[i], "<") == 0 && args[i + 1] != NULL) {
            *input_file = args[i + 1];
            // Shift the rest of the arguments
            int j;
            for (j = i; args[j - 1] != NULL; j++) {
                args[j] = args[j + 2];
            }
            i--; // Adjust index after shifting
        } else if (strcmp(args[i], ">") == 0 && args[i + 1] != NULL) {
            *output_file = args[i + 1];
            // Similar shifting for ">"
            int j;
            for (j = i; args[j - 1] != NULL; j++) {
                args[j] = args[j + 2];
            }
            i--; // Adjust index after shifting
        }
    }
}

// Function to replace all occurrences of $$ with the shell's PID
void expand_pid(char* input) {
    char* pid_placeholder = "$$";
    char pid_str[10];
    snprintf(pid_str, 10, "%d", getpid()); // Convert PID to string

    // Temporary buffer to hold the new string with PIDs expanded
    char new_input[MAX_CHAR_LENGTH] = {0};

    char* start = input;
    char* end;
    while ((end = strstr(start, pid_placeholder)) != NULL) {
        // Copy the part before the PID placeholder
        strncat(new_input, start, end - start);
        // Append the PID
        strcat(new_input, pid_str);
        // Move past the placeholder in the input string
        start = end + strlen(pid_placeholder);
    }
    // Copy any remaining part of the input string
    strcat(new_input, start);
    // Copy the new input back into the original input variable
    strcpy(input, new_input);
}

void execute_command(char *args[], int *exit_status, int runInBackground) {
    char *input_file = NULL;
    char *output_file = NULL;

    // Call helper function to set up redirection
    setup_redirection(args, &input_file, &output_file);

    pid_t pid = fork();

    if (pid == -1) {
        perror("fork");
    } 
    else if (pid == 0) { // Child process
        if (runInBackground) {
            if (input_file == NULL) {
                // Redirect stdin to /dev/null for background processes
                int devNullIn = open("/dev/null", O_RDONLY);
                if (devNullIn == -1) {
                    perror("open input /dev/null");
                    exit(1);
                }
                dup2(devNullIn, STDIN_FILENO);
                close(devNullIn);
            }

            if (output_file == NULL) {
                // Redirect stdout to /dev/null for background processes
                int devNullOut = open("/dev/null", O_WRONLY);
                if (devNullOut == -1) {
                    perror("open output /dev/null");
                    exit(1);
                }
                dup2(devNullOut, STDOUT_FILENO);
                close(devNullOut);
            }
        } else {
            if (input_file != NULL) {
                int input_fd = open(input_file, O_RDONLY);
                if (input_fd == -1) {
                    perror("open input");
                    exit(1);
                }
                dup2(input_fd, STDIN_FILENO);
                close(input_fd);
            }

            if (output_file != NULL) {
                int output_fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (output_fd == -1) {
                    perror("open output");
                    exit(1);
                }
                dup2(output_fd, STDOUT_FILENO);
                close(output_fd);
            }
        }

        if (execvp(args[0], args) == -1) {
            perror("execvp");
            exit(1);
        }
    } 
    else {
        // Parent process
        if (!runInBackground) {
            int status;
            waitpid(pid, &status, WUNTRACED);
            if (WIFEXITED(status)) {
                *exit_status = WEXITSTATUS(status);
            } else if (WIFSIGNALED(status)) {
                *exit_status = WTERMSIG(status);
            }
        } else {
            background_process_pids[num_background_processes++] = pid;
            printf("Started background process PID: %d\n", pid);
        }
    }
}




void setup_signal_handlers() {
    struct sigaction sa_sigchld = {0}, SIGINT_action = {0}, SIGTSTP_action = {0};

    // Ignore SIGINT
    SIGINT_action.sa_handler = SIG_IGN;
    sigfillset(&SIGINT_action.sa_mask);
    SIGINT_action.sa_flags = 0;
    sigaction(SIGINT, &SIGINT_action, NULL);

    // Handle SIGTSTP
    SIGTSTP_action.sa_handler = sigtstp_handler;
    sigfillset(&SIGTSTP_action.sa_mask);
    SIGTSTP_action.sa_flags = SA_RESTART;
    sigaction(SIGTSTP, &SIGTSTP_action, NULL);

    // Set up SIGCHLD handler
    sa_sigchld.sa_handler = sigchld_handler;
    sigemptyset(&sa_sigchld.sa_mask);
    sa_sigchld.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sa_sigchld, NULL);
}

int main() {
    // Set up signal handlers
    setup_signal_handlers();

    int exit_status = 0; // Initialize exit status
    
    // Shell main loop
    while (1) {
        char input[MAX_CHAR_LENGTH]; // Buffer to store user input
        memset(input, 0, sizeof(input)); // Clear the input buffer
        
        
        // Display the shell prompt (e.g., ": ")
        printf(": ");
        fflush(stdout);

        // Read a line of input from the user
        if (fgets(input, sizeof(input), stdin) == NULL) {
            // Handle errors or EOF (Ctrl+D)
            break;
        }

        // Expand the PID variables
        expand_pid(input);

        // Remove newline character from the input
        input[strcspn(input, "\n")] = '\0';

        // Tokenize the input into an argument list
        char *args[MAX_ARGUMENTS];
        int arg_count = 0;
        int runInBackground = 0; 
        char *token = strtok(input, " ");

        while (token != NULL && arg_count < MAX_ARGUMENTS - 1) {
        args[arg_count++] = token;
        token = strtok(NULL, " ");
        }

        args[arg_count] = NULL; // Null-terminate the argument list

        if (arg_count == 0 || args[0][0] == '#') {
            // Ignore blank lines and comments
            continue;
        }

    if (arg_count > 1 && strcmp(args[arg_count - 1], "&") == 0) {
    if (!foreground_only_mode) {
        runInBackground = 1; // Set to run in background if not in foreground-only mode
    }
    args[arg_count - 1] = NULL; // Remove the "&" from the argument list
    }


    // Check for built-in commands
    if (strcmp(args[0], "cd") == 0) {
    handle_cd(args[1]); // args[1] will be NULL if no argument is provided
    }
    else if (strcmp(args[0], "status") == 0) {
    handle_status(exit_status);
    }
    else if (strcmp(args[0], "exit") == 0) {
    handle_exit();
    }
    // Handle non-built-in commands
    else {
    execute_command(args, &exit_status, runInBackground);
    }
  }
// Cleanup and exit
return 0;

}
