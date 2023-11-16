#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

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
void execute_command(char *args[], int *exit_status);


// SIGINT handler
void sigint_handler(int signo) {
    // Do nothing (ignore SIGINT in the parent process)

}

// SIGTSTP handler
void sigtstp_handler(int signo) {
    // Toggle the foreground-only mode
    foreground_only_mode = !foreground_only_mode;

    char *message;
    if (foreground_only_mode) {
        message = "\nEntering foreground-only mode (& is now ignored)";
    } else {
        message = "\nExiting foreground-only mode";
    }
    write(STDERR_FILENO, message, strlen(message));

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
        printf("Exit status: %d\n", WEXITSTATUS(exit_status));
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
    for (int i = 0; i < num_background_processes; i++) {
        terminate_process(background_process_pids[i]);
    }

    // Cleanup and exit the shell
    exit(0);
}

void execute_command(char *args[], int *exit_status) {
    pid_t pid = fork();

    if (pid == -1) {
        // Forking error
        perror("fork");
    } else if (pid == 0) {
        // Child process
        if (execvp(args[0], args) == -1) {
            perror("execvp");
            exit(EXIT_FAILURE); // If execvp fails, exit the child process
        }
    } else {
        // Parent process
        int status;
        do {
            waitpid(pid, &status, WUNTRACED); // Wait for the child process to finish
        } while (!WIFEXITED(status) && !WIFSIGNALED(status)); // Loop until the child process has not exited

        // Update the exit_status with either the exit status or the signal number
        if (WIFEXITED(status)) {
            *exit_status = WEXITSTATUS(status);
        } else if (WIFSIGNALED(status)) {
            *exit_status = WTERMSIG(status);
        }
    }
}



void setup_signal_handlers() {
    struct sigaction SIGINT_action = {0}, SIGTSTP_action = {0};

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

        // Remove newline character from the input
        input[strcspn(input, "\n")] = '\0';

        // Tokenize the input into an argument list
        char *args[MAX_ARGUMENTS];
        int arg_count = 0;
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
    execute_command(args, &exit_status);
    }
  }
// Cleanup and exit
return 0;

}
