#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h> 
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>    

#define MAX_CHAR_LENGTH 2048            // Maximum length of array
#define MAX_ARGUMENTS 512               // Maximum amount of arguments
#define MAX_BACKGROUND_PROCESSES 1000   // Maximum amount of background processes, set it to something large for now

#define SIG_INT 2   // Signal number for SIGINT
#define SIG_TSTP 20 // Signal number for SIGSTP

pid_t background_process_pids[MAX_BACKGROUND_PROCESSES]; // Declared array to store PIDs
int num_background_processes = 0; // Initialize these processes to zero

// Global variable to track foreground-only mode
int foreground_only_mode = 0;

// Function declarations for built-in commands
void handle_cd(const char *args);
void handle_status(int exit_status);
void handle_exit();
void execute_command(char *args[], int *exit_status, int runInBackground);

/*
    SIGNINT handler meant to keep the default behavior from happening.
    This is why we do nothing, ignoring the SIGINT instead.
*/
void sigint_handler(int signo) {
    // Do nothing (ignore SIGINT instead of default)

}

/*
    SIGSTP handler meant to toggle the foreground-only mode either ON or OFF,
    this will affect the ability for background processes to occur.
*/
void sigtstp_handler(int signo) {
    // Toggle the foreground-only mode
    char *message;
    // Switch to ON
    if (foreground_only_mode == 0) {
        message = "\nEntering foreground-only mode (& is now ignored)";
        foreground_only_mode = 1;
    } 
    // Switch to OFF
    else {
        message = "\nExiting foreground-only mode";
        foreground_only_mode = 0;
    }
    write(STDERR_FILENO, message, strlen(message));
}

/*
    SIGCHLD handler meant to manage the background processes, the 
    function is triggered when a child processes' state is received.
*/
void sigchld_handler(int signo) {
    int saved_errno = errno; // Save current errno
    pid_t pid;
    int status;

    // Check for background process that have finished executing
   while ((pid = waitpid((pid_t)(-1), &status, WNOHANG)) > 0) {
        // Check if the pid is in the background_process_pids array before printing
        int i;
        for (i = 0; i < num_background_processes; i++) {
            if (background_process_pids[i] == pid) {
                // Found a background process that has completed
                printf("Background pid %d is done: ", pid);
                handle_status(status);
                // Remove pid from the array
                break;
            }
        }
    }

    errno = saved_errno; // Restore errno
}

/*
    The handle_cd function is meant to use the path given to change directories,
    and if NULL, then the default location will be the home directory.
*/
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
            // If this fails, print out an error
            perror("chdir");
        }
    }
}

/*
    The handle_status will take in the exit status parameter and output the status
    if there was a normal exit, otherwise output the signal number if terminated
*/
void handle_status(int exit_status) {
    if (WIFEXITED(exit_status)) {
        // Normal exit, print the exit status
        printf("Exit value: %d\n", WEXITSTATUS(exit_status));
        fflush(stdout);
    } else if (WIFSIGNALED(exit_status)) {
        // Terminated due to a signal; print the signal number
        printf("Terminated by signal: %d\n", WTERMSIG(exit_status));
        fflush(stdout);
    }
}

/*
    The terminate_process function will use a pid and send a SIGTERM to it,
    useful for when exiting the shell process and processes need to be terminated
    first
*/
void terminate_process(pid_t pid) {
    if (kill(pid, SIGTERM) == -1) {
        // Handle the error if sending the signal fails
        perror("kill");
    }
}

/*
    The handle exit function will iterate through the processes array and send SIGTERM
    to each one using the terminate_process function, and then exit the shell.
*/
void handle_exit() {
    // Iterate through the list of background process PIDs and send SIGTERM
    int i;
    for (i = 0; i < num_background_processes; i++) {
        terminate_process(background_process_pids[i]);
    }

    // Cleanup and exit the shell
    exit(0);
}

/*
    The setup_redirection function processes the command line 
    arguments to setup input and output redirection.
*/
void setup_redirection(char *args[], char **input_file, char **output_file) {
    // Iterate through the args array
    int i;
    for (i = 0; args[i] != NULL; i++) {

        // Check if the current argument is the input redirection symbol <
        if (strcmp(args[i], "<") == 0 && args[i + 1] != NULL) {
            *input_file = args[i + 1];  // Set the pointer to the file after the <

            // Shift the arguments to the left to remove the redirection symbol and the file name from args
            int j;
            for (j = i; args[j - 1] != NULL; j++) {
                args[j] = args[j + 2];
            }
            i--; // Decrement index to adjust for the shifted elements in args
        } 
        
        // Check if the current argument is the output redirection symbol '>'
        else if (strcmp(args[i], ">") == 0 && args[i + 1] != NULL) {
            *output_file = args[i + 1]; // Set the pointer to the file specified after '>'

            // Similar shifting process for the '>' symbol and its file name
            int j;
            for (j = i; args[j - 1] != NULL; j++) {
                args[j] = args[j + 2];
            }
            i--; // Decrement index to adjust for the shifted elements in args
        }
    }
}


/*
    The expand_pid function replaces all occurrences of the placeholder "$$" 
    with the shell's pid in an input string.
*/
void expand_pid(char* input) {
    char* pid_placeholder = "$$";
    char pid_str[10];
    snprintf(pid_str, 10, "%d", getpid()); // First let's convert the PID to string

    // Temp buffer to hold the new string with PIDs expanded
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

/*
    The execute command is fairly complex so there will be more comments 
    for each section, but overall it will take in some parameters, and proceed
    to set up redirection to a default location or specific file for a child process,
    if a parent process is detected then it will wait for pid to return before letting 
    a user make the next command in the shell.
*/
void execute_command(char *args[], int *exit_status, int runInBackground) {
    char *input_file = NULL;
    char *output_file = NULL;

    // Call helper function to set up redirection
    setup_redirection(args, &input_file, &output_file);

    pid_t pid = fork();


    if (pid == 0) { // Child process initiated

        // Child process running in the foreground, should revert back to default behavior for SIGINT
        if (!runInBackground) {
            struct sigaction SIGINT_action = {0};
            SIGINT_action.sa_handler = SIG_DFL;
            sigaction(SIGINT, &SIGINT_action, NULL);
        }

        // Child process is running in the background
        if (runInBackground) {
            // Input file parameter is NULL, redirect to default (/dev/null)
            if (input_file == NULL) {
                int devNullIn = open("/dev/null", O_RDONLY);
                if (devNullIn == -1) {
                    perror("open input /dev/null");
                    exit(1);
                }
                dup2(devNullIn, STDIN_FILENO);
                close(devNullIn);
            }

            // Output file parameter is NULL, redirect to default (/dev/null)
            if (output_file == NULL) {
                int devNullOut = open("/dev/null", O_WRONLY);
                if (devNullOut == -1) {
                    perror("open output /dev/null");
                    exit(1);
                }
                dup2(devNullOut, STDOUT_FILENO);
                close(devNullOut);
            }
            } 
            
            else {
                //Input file is NOT null, so attempt to open the file for reading
                if (input_file != NULL) {
                int input_fd = open(input_file, O_RDONLY);
                if (input_fd == -1) {
                    perror("Open input error"); // Text error for input
                    exit(1);
                }
                dup2(input_fd, STDIN_FILENO);
                close(input_fd);
              }
                // Similar to input file, except this time it is with output and will write, create, and/or truncate
                if (output_file != NULL) {
                int output_fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (output_fd == -1) {
                    perror("Open output error"); // Text error for output
                    exit(1);
                }
                dup2(output_fd, STDOUT_FILENO);
                close(output_fd);
              }
            }

        // This outputs the text error for an invalid command entered
        if (execvp(args[0], args) == -1) {
            perror("Invalid command error");
            exit(1);
        }
    } 
    else {  // Parent process initiated
        if (!runInBackground)   // runInBackground not toggled on, foreground process!
        {
            int status;
            waitpid(pid, &status, 0);   // Wait for pid to return first 
            if (WIFEXITED(status)) {
                *exit_status = WEXITSTATUS(status);
            } else if (WIFSIGNALED(status)) {
                *exit_status = WTERMSIG(status);
                printf("Terminated by signal %d\n", *exit_status);
                fflush(stdout);
            }
        }
        // Background mode enabled
         else {
            background_process_pids[num_background_processes++] = pid;  // Create new index of array and assign it this new pid
            printf("Started background process PID: %d\n", pid);
            fflush(stdout);
        }
    }
}

/*
    The setup_signal_handlers is essentially what it's named, it will go ahead
    and setup the signal handlers we need for ensure the appropiate shell behavior occurs.
*/
void setup_signal_handlers() {
    struct sigaction sa_sigchld = {0}, SIGINT_action = {0}, SIGTSTP_action = {0};

    // Handler for SIGINT
    SIGINT_action.sa_handler = SIG_IGN;
    sigfillset(&SIGINT_action.sa_mask);
    SIGINT_action.sa_flags = 0;
    sigaction(SIGINT, &SIGINT_action, NULL);

    // Handler for SIGTSTP
    SIGTSTP_action.sa_handler = sigtstp_handler;
    sigfillset(&SIGTSTP_action.sa_mask);
    SIGTSTP_action.sa_flags = SA_RESTART;
    sigaction(SIGTSTP, &SIGTSTP_action, NULL);

    // Handler for SIGCHLD
    sa_sigchld.sa_handler = sigchld_handler;
    sigemptyset(&sa_sigchld.sa_mask);
    sa_sigchld.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sa_sigchld, NULL);
}

int main() {

    // Call to setup signal handlers
    setup_signal_handlers();

    int exit_status = 0; // Initialize exit status
    
    // Main shell loop
    while (1) {
        char input[MAX_CHAR_LENGTH]; // Buffer to store user input
        memset(input, 0, sizeof(input)); // Clear the input buffer
        
        
        // Display the shell prompt (": ")
        printf(": ");
        fflush(stdout);

        // Read a line of input from the user
        if (fgets(input, sizeof(input), stdin) == NULL) {
            break;
        }

        // Expand the PID using input
        expand_pid(input);

        // Remove newline character from the input
        input[strcspn(input, "\n")] = '\0';

        // Tokenize the input into an argument list & set some variables up
        char *args[MAX_ARGUMENTS];
        int arg_count = 0;
        int runInBackground = 0; 
        char *token = strtok(input, " ");

        // Tokenizes the input string into separate arguments.
        while (token != NULL && arg_count < MAX_ARGUMENTS - 1) {
        args[arg_count++] = token;
        token = strtok(NULL, " ");
        }

        args[arg_count] = NULL; // Null-terminate the argument list

        // Check for either a blank line or a comment that was entered and ignore it
        if (arg_count == 0 || args[0][0] == '#') {
            continue;
        }

    // Check for a '&' being located as the last character entered
    if (arg_count > 1 && strcmp(args[arg_count - 1], "&") == 0) {
    if (!foreground_only_mode) {
        runInBackground = 1; // Set to run in background if not in foreground-only mode
    }
    args[arg_count - 1] = NULL; // Now remove the "&" from the argument list
    }


    // Check for built-in commands
    if (strcmp(args[0], "cd") == 0) {
    handle_cd(args[1]); // Command cd was detected, so let's call it's function
    }
    else if (strcmp(args[0], "status") == 0) {
    handle_status(exit_status); // Command status was detected, so let's call it's function
    }
    else if (strcmp(args[0], "exit") == 0) {
    handle_exit();  // Command exit was detected, so let's call it's function
    }

    // If none of those commands are detected, then let's check for a non built in command
    else {
    execute_command(args, &exit_status, runInBackground);
    }
  }
  
// Cleanup and exit
return 0;

}
