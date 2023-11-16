#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

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
void handle_status(int *exit_status);
void handle_exit();

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
void handle_status(int *exit_status) {
    // Print the exit status or terminating signal of the last foreground process
    printf("Exit status: %d\n", *exit_status);
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

void handle_pwd() {
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("%s\n", cwd);
    } else {
        perror("getcwd() error");
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

        // Tokenize the input and check for specific commands
    char *token = strtok(input, " ");
    
    // Check for blank lines or comments
    if (token == NULL || token[0] == '#') {
        // Ignore blank lines and comments
        continue;
    }

    // Check for "pwd" command
    if (strcmp(token, "pwd") == 0) {
        handle_pwd();
    }
    // Check for "cd" command
    else if (strcmp(token, "cd") == 0) {
        // Pass the rest of the input (directory path) to the function
        token = strtok(NULL, " ");
        handle_cd(token);
    }
    // Check for "status" command
    else if (strcmp(token, "status") == 0) {
        handle_status(&exit_status);
    }
    // Check for "exit" command
    else if (strcmp(token, "exit") == 0) {
        handle_exit();
    }
    // Handle other commands or implement error handling
    else {
        // Implement logic for executing non-built-in commands
        // ...
    }
    }
    // Cleanup and exit
    return 0;
}
