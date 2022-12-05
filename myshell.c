#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <wait.h>

/* functions for dealing with SIGINT */
void SIG_IGN_handler(void);
void SIG_CHLD_handler(int sig);
void child_handler(int sig);

/* functions for dealing with the different options of the function process_arglist  */
int executing_command(char **command);
int background_command(char **command);
int single_piping(int split, char **command);
int output_redirecting(int count, char **command);

/* helping function for SIG_CHLD_handler.
 * CREDIT: ERAN'S TRICK -
 * https://stackoverflow.com/questions/7171722/how-can-i-handle-sigchld/7171836#7171836 */
void child_handler(int sig) {
    int id;
    int status;

    while ((id = waitpid(-1, &status, WNOHANG)) > 0) {
        ;
    }
    if (id == -1 && errno!=EINTR && errno!=ECHILD){
        perror("Error with child handler.\n");
        exit(1);
    }
}

/* CREDIT: ERAN'S TRICK -
* https://stackoverflow.com/questions/7171722/how-can-i-handle-sigchld/7171836#7171836 */
void SIG_CHLD_handler(int sig){
    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sa.sa_handler = child_handler;
    if (sigaction(SIGCHLD, &sa, NULL)==-1) {
        perror("Error with SIG_CHLD handler.\n");
        exit(1);
    }
}

/* CREDIT: ERAN'S TRICK -
* https://stackoverflow.com/questions/40601337/what-is-the-use-of-ignoring-sigchld-signal-with-sigaction2/40601403#40601403 */
void SIG_IGN_handler() {
    struct sigaction sa;
    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGCHLD, &sa, 0) == -1) {
        perror("Error with SIG_IGN handler.\n");
        exit(1);
    }
}


/* CREDIT: codeValue - Handling signals YouTube video */
int prepare() {
    SIG_CHLD_handler(1);
    return 0;
}

/* Helping function of the function process_arglist:
 * This function executes a single command and waits for it to finish before running a new one */
int executing_command(char **command) {
    int id = fork();
    int status;

    /* child process */
    if (id==0) {
        SIG_IGN_handler();
        if (execvp(command[0], command) != -1) {
            perror("Error with executing a single command.\n");
            exit(1);
        }
    }

    /* parent process */
    else {
        if (id == -1) {
            perror("Error with fork.\n");
            return 0;
        }

        if (waitpid(id, &status, 0)==-1 && errno!=EINTR && errno!=ECHILD) {
            perror("Error with wait at single command.\n");
            return 0;
        }
    }
    return 1;
}

/* Helping function of the function process_arglist:
 * This function executes a single command but doesn't wait for it to finish before running a new one */
int background_command(char **command) {
    int id = fork();

    /* child process */
    if (id==0) {
        if (execvp(command[0], command) == -1) {
            perror("Error with executing background command\n");
            exit(1);
        }
    }

    /* parent process */
    if (id == -1) {
        perror("Error with executing background command\n");
        return 0;
    }
    return 1;
}


/* Helping function of the function process_arglist:
 * This function executes a single piping command.
 * The output of the first function is the input of the second function.
 * CREDIT: codeVault - "Simulating the pipe "|" operator in C" YouTube video */
int single_piping(int split, char **command) {
    command[split] = NULL;

    /* creating the pipe */
    int fd[2];
    if (pipe(fd)==-1) {
        perror("Error with piping\n");
        exit(1);
    }

    int id1 = fork();

    /* child process 1 */
    if (id1 == 0) {
        SIG_IGN_handler();
        if (dup2(fd[1], STDOUT_FILENO) == -1) {
            perror("Error with dup2 at piping\n");
            exit(1);
        }
        close(fd[0]);
        close(fd[1]);
        if (execvp(command[0], command)==-1) {
            perror("Error with piping\n");
            exit(1);
        }
    }
    else {
        if (id1 == -1) {
            perror("Error with fork at piping\n");
            return 0;
        }

        int id2 = fork();

        /* child process 2 */
        if (id2 == 0) {
            SIG_IGN_handler();
            if (dup2(fd[0], STDIN_FILENO) == -1) {
                perror("Error with piping\n");
                exit(1);
            }
            close(fd[0]);
            close(fd[1]);
            if (execvp(command[split + 1], command + split + 1)==-1) {
                perror("Error with piping\n");
                exit(1);
            }

        }
        if (id2 == -1) {
            perror("Error with fork at piping\n");
            return 0;
        }

        close(fd[0]);
        close(fd[1]);

        /* parent process waits for the child processes to finish */
        if (waitpid(id1, NULL, 0)==-1 && errno!=EINTR && errno!=ECHILD) {
            perror("Error with wait at piping\n");
            return 0;
        }
        if (waitpid(id2, NULL, 0)==-1 && errno!=EINTR && errno!=ECHILD) {
            perror("Error with wait at piping\n");
            return 0;
        }
    }

    return 1;
}
/* Helping function of the function process_arglist:
 * This function executes a single command and prints the output into a file with the given filename.
 * We wait until it finishes before starting another command.
 * CREDIT: codeVault - "Redirecting standard output in C" YouTube video */
int output_redirecting(int count, char **command) {
    int id = fork();

    /* child process */
    if (id==0) {
        SIG_IGN_handler();
        int file = open(command[count-1], O_RDWR | O_CREAT | O_TRUNC, 0664);

        /* changing the '>' char to NULL in order to slice out the filename from the command */
        command[count-2] = NULL;

        if (file == -1) {
            perror("Error with opening the file");
            exit(1);
        }
        if (dup2(file, STDOUT_FILENO)==-1) {
            perror("Error with dup2 at output redirecting");
            exit(1);
        }
        if (execvp(command[0], command)==-1) {
            perror("Error with output redirecting");
            exit(1);
        }
        close(file);
    }

    /* parent process */
    else {
        if (id==-1) {
            perror("Error with fork at output redirecting");
            return 0;
        }
        if (waitpid(id, NULL, 0)==-1) {
            perror("Error with wait at output redirecting");
            return 0;
        }
    }
    return 1;
}

int process_arglist(int count, char **arglist) {
    /* special identifier characters */
    char background_command_char = '&';
    char single_piping_char = '|';
    char output_redirecting_char = '>';

    /* Executing commands in the background */
    if (count > 1) {
        char last_char = arglist[count-1][0];
        if (last_char == background_command_char) {
            arglist[count - 1] = NULL;
            return background_command(arglist);
        }
    }

    /* Output redirecting */
    if (count > 2) {
        char one_before_last_char = arglist[count-2][0];
        if (one_before_last_char == output_redirecting_char) {
            return output_redirecting(count, arglist);
        }
    }

    /* Single piping */
    for (int i=0; i<count; i++) {
        char curr_char = arglist[i][0];
        if (curr_char == single_piping_char) {
            return single_piping(i, arglist);
        }
    }

    /* Executing commands */
    return executing_command(arglist);
}

/* no finalizing needed */
int finalize(){
    return 0;
}
