#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdio.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>

#define STREQ(a, b) (strcmp((a), (b)) == 0)
#define FORK_FAILURE -1
#define WAITPID_FAILURE -1
#define NOT_FOUND -1

/*
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
~~~~~~~~~~~~~~~~~~ Signal Handlers ~~~~~~~~~~~~~~~~~~
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
*/

void sigchld_handler(int signum) {
    if (signum == SIGCHLD) {
        int status;
        pid_t pid;
        while ((pid = waitpid(-1, &status, WNOHANG)) > 0) { }
        // Check if waitpid returned due to an error
        if (pid == WAITPID_FAILURE && errno != ECHILD) // ECHILD means no more child processes
            perror("waitpid failure");
    }
}

void sigint_handler(int signum) { }


int prepare(void)
{
    struct sigaction sa_chld, sa_int;

    // SIGCHLD
    sigemptyset(&sa_chld.sa_mask);
    sa_chld.sa_handler = sigchld_handler;
    sa_chld.sa_flags = SA_NOCLDSTOP | SA_RESTART;
    if (sigaction(SIGCHLD, &sa_chld, NULL) < 0) {
        perror("SIGCHLD handle registration failed");
        return 1;
    }

    // SIGINT
    sigemptyset(&sa_int.sa_mask);
    sa_int.sa_handler = sigint_handler;
    sa_int.sa_flags = SA_RESTART;
    if (sigaction(SIGINT, &sa_int, NULL) < 0) {
        perror("SIGINT handle registration failed");
        return 1;
    }

    return 0;
}


int finalize(void)
{
    return 0;
}

/*
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
~~~~~~~~~~~~~~~~~~ Utilities ~~~~~~~~~~~~~~~~~~
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
*/

static int waitpid_w_error_handling(pid_t pid)
{
    int status;
    if (waitpid(pid, &status, 0) == WAITPID_FAILURE && errno != ECHILD) {
        perror("waitpid failure");
        return 0;
    }
    return 1;
}

static void execvp_w_error_handling(char *cmd1, char **arglist)
{
    execvp(cmd1, arglist);
    perror("execvp failure");
    exit(1); /* In cases of child failure I certainly don't want the child to return to shell.c */
}

/*
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
~~~~~~~~~~~~~~~~ Classification ~~~~~~~~~~~~~~~~
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
*/

static int has_ampersand(int count, char **arglist)
{
    return STREQ(arglist[count - 1], "&");
}

static int pipe_index(int count, char **arglist)
{
    int i;
    for (i = 0; i < count; i++)
    {
        if (STREQ(arglist[i], "|"))
            return i;
    }
    return NOT_FOUND;
}

static int has_right_redirection(int count, char **arglist)
{
    return count > 2 ? STREQ(arglist[count - 2], ">") : 0;
}

static int has_left_redirection(int count, char **arglist)
{
    return count > 2 ? STREQ(arglist[count - 2], "<") : 0;
}

/*
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
~~~~~~~~~~~~~~~~~~~ Handling ~~~~~~~~~~~~~~~~~~~
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
*/

static int default_exec(int count, char **arglist)
{
    pid_t pid = fork();
    if (pid == FORK_FAILURE) {
        perror("fork failure");
        return 0;
    }
    if (pid == 0)
    {
        execvp_w_error_handling(arglist[0], arglist);
        perror("execvp failure");
        exit(1); /* In cases of child failure I certainly don't want the child to return to shell.c */
    }
    return waitpid_w_error_handling(pid);
}

static int exec_on_background(int count, char **arglist)
{
    pid_t pid;
    arglist[count - 1] = NULL;
    pid = fork();
    if (pid == FORK_FAILURE) {
        perror("fork failure");
        return 0;
    }
    if (pid == 0)
    {
        if (setpgid(0, 0) == -1) { // set new process group for background process
            perror("Failed to set new process group for background process");
            exit(1);
        }
        execvp_w_error_handling(arglist[0], arglist);
    }
    return 1;
}

static int redirect_output(int count, char **arglist)
{
    char *file_name =  arglist[count - 1];
    pid_t pid;
    arglist[count - 2] = NULL;
    pid = fork();
    if (pid == FORK_FAILURE) {
        perror("fork failure");
        return 0;
    }
    if (pid == 0)
    {
        int file = open(file_name, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (file < 0)
        {
            perror("open file failure");
            exit(1);
        }
        int dup = dup2(file, STDOUT_FILENO);
        if (dup < 0)
        {
            perror("dup2 failure");
            close(file);
            exit(1);
        }
        close(file);
        execvp_w_error_handling(arglist[0], arglist);
    }
   return waitpid_w_error_handling(pid);
}

static int redirect_input(int count, char **arglist)
{
    char *file_name =  arglist[count - 1];
    pid_t pid;
    arglist[count - 2] = NULL;
    pid = fork();

    if (pid == FORK_FAILURE) {
        perror("fork failure");
        return 0;
    }

    if (pid == 0)
    {
        int file = open(file_name, O_RDONLY);
        if (file < 0) {
            perror("open file failure");
            exit(1);
        }
        int dup = dup2(file, STDIN_FILENO);
        if (dup < 0) {
            perror("dup2 failure");
            close(file);
            exit(1);
        }
        close(file);
        execvp_w_error_handling(arglist[0], arglist);
    }
    return waitpid_w_error_handling(pid);
}

static int pipe_commands(int count, char **arglist, int pipe_index)
{
    char* cmd1 = arglist[0];
    char* cmd2 = arglist[pipe_index + 1];
    int pipefd[2];
    arglist[pipe_index] = NULL;

    if (pipe(pipefd) == -1) {
        perror("pipe failure");
        return 0;
    }
    pid_t pid0 = fork();
    if (pid0 == FORK_FAILURE) {
        perror("fork failure");
        return 0;
    }
    if (pid0 == 0)
    {
        /* This is the process that will execute the second command */
        close(pipefd[1]); // write
        int dup = dup2(pipefd[0], STDIN_FILENO);
        if (dup < 0) {
            perror("dup2 failure in second command");
            close(pipefd[0]);
            exit(1);
        }
        close(pipefd[0]); // read
        execvp_w_error_handling(cmd2, &arglist[pipe_index + 1]);
    }

    pid_t pid1 = fork();
    if (pid1 == FORK_FAILURE) {
        perror("fork failure");
        exit(1);
    }
    if (pid1 == 0)
    {
        /* This is the process that will execute the first command */
        close(pipefd[0]); // read
        int dup = dup2(pipefd[1], STDOUT_FILENO);
        if (dup < 0) {
            perror("dup2 failure in first command");
            close(pipefd[1]);
            exit(1);
        }
        close(pipefd[1]); // write
        execvp_w_error_handling(cmd1, arglist);
    }
    close(pipefd[0]);
    close(pipefd[1]);
    int res0 = waitpid_w_error_handling(pid0);
    int res1 = waitpid_w_error_handling(pid1);
    return res0 || res1;
}

int process_arglist(int count, char **arglist)
{
    int ha = has_ampersand(count, arglist);
    int pi = pipe_index(count, arglist);
    int hr = has_right_redirection(count, arglist);
    int hl = has_left_redirection(count, arglist);
    int ret_val;
    if (ha)
    {
        ret_val = exec_on_background(count, arglist);
    }
    else if (pi != NOT_FOUND)
    {
        ret_val = pipe_commands(count, arglist, pi);
    }
    else if (hr)
    {
        ret_val = redirect_output(count, arglist);
    }
    else if (hl)
    {
        ret_val = redirect_input(count, arglist);
    }
    else
    {
        ret_val = default_exec(count, arglist);
    }

    return ret_val;
}
