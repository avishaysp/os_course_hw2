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


void mySignalHandler(int signum) { }
static pid_t* sons;
static int num_of_sons;

int prepare(void)
{
    struct sigaction sa = {.sa_handler = mySignalHandler, .sa_flags = SA_RESTART};
    if (sigaction(SIGINT, &sa, NULL) < 0) {
        perror("Signal handle registration failed");
        return 1;
    }
    sons = (pid_t*)malloc(sizeof(pid_t));
    if (sons == NULL) {
        perror("prepare - malloc failed");
        return 1;
    }
    num_of_sons = 0;
    sons = (pid_t*)malloc(sizeof(pid_t));
    if (sons == NULL) {
        perror("prepare - malloc failed");
        return 1;
    }
    num_of_sons = 0;
    return 0;
}

int finalize(void)
{
    int i;
    int status;
    for (i = 0; i < num_of_sons; i++)
    {
        if (sons[i] > 0) {
            waitpid(sons[i], &status, 0);
        }
    }
    free(sons);
    return 0;
}

/*
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
~~~~~~~~~~~~~~~~~~ Utilities ~~~~~~~~~~~~~~~~~~
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
*/



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
    int status;
    pid_t pid = fork();
    if (pid == FORK_FAILURE) {
        perror("fork failure");
        return 0;
    }
    if (pid == 0)
    {
        execvp(arglist[0], arglist);
        perror("execvp failure");
        exit(1); /* In cases of child failure I certainly don't want the child to return to shell.c */
    }
    if (waitpid(pid, &status, 0) == WAITPID_FAILURE) {
        perror("waitpid failure");
        return 0;
    }
    return 1;
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
        execvp(arglist[0], arglist);
        /* If execvp returns, it must have failed */
    }
    num_of_sons++;
    sons = (pid_t*)realloc(sons, sizeof(pid_t) * num_of_sons);
    if (sons == NULL) {
        perror("sons realloc failed");
        return 0;
    }
    sons[num_of_sons - 1] = pid;
    return 1;
}

static int redirect_output(int count, char **arglist)
{
    char *file_name =  arglist[count - 1];
    pid_t pid;
    int status;
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
            return 0;
        }
        int dup = dup2(file, STDOUT_FILENO);
        if (dup < 0)
        {
            perror("dup2 failure");
            close(file);
            return 0;
        }
        close(file);
        execvp(arglist[0], arglist);
        perror("execvp failure");
        exit(1); /* In cases of child failure I certainly don't want the child to return to shell.c */
    }
    if (waitpid(pid, &status, 0) == WAITPID_FAILURE) {
        perror("waitpid failure");
        return 0;
    }
    return 1;
}

static int redirect_input(int count, char **arglist)
{
    char *file_name =  arglist[count - 1];
    pid_t pid;
    int status;
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
            return 0;
        }
        int dup = dup2(file, STDIN_FILENO);
        if (dup < 0) {
            perror("dup2 failure");
            close(file);
            return 0;
        }
        close(file);
        execvp(arglist[0], arglist);
        perror("execvp failure");
        exit(1); /* In cases of child failure I certainly don't want the child to return to shell.c */
    }
    if (waitpid(pid, &status, 0) == WAITPID_FAILURE) {
        perror("waitpid failure");
        return 0;
    }
    return 1;
}
static int pipe_commands(int count, char **arglist, int pipe_index)
{
    char* cmd1 = arglist[0];
    char* cmd2 = arglist[pipe_index + 1];
    int pipefd[2];
    int status;
    arglist[pipe_index] = NULL;
    pid_t pid = fork();
    if (pid == FORK_FAILURE) {
        perror("fork failure");
        return 0;
    }
    if (pid == 0)
    {
        if (pipe(pipefd) == -1) {
            perror("pipe failure");
            exit(EXIT_FAILURE);
        }
        pid = fork();
        if (pid == FORK_FAILURE) {
            perror("fork failure");
            exit(1);
        }
        if (pid == 0)
        {
            /* This is the process that will be execute the first command */
            close(pipefd[0]); // read
            int dup = dup2(pipefd[1], STDOUT_FILENO);
            if (dup < 0) {
                perror("dup2 failure in first command");
                close(pipefd[1]);
                exit(1);
            }
            close(pipefd[1]); // write
            execvp(cmd1, arglist);
            perror("execvp failure");
            exit(1); /* In cases of child failure I certainly don't want the child to return to shell.c */
        }

        /* This is the process that will be execute the second command */
        close(pipefd[1]); // write
        int dup = dup2(pipefd[0], STDIN_FILENO);
        if (dup < 0) {
            perror("dup2 failure in second command");
            close(pipefd[0]);
            exit(1);
        }
        close(pipefd[0]); // read
        execvp(cmd2, &arglist[pipe_index + 1]);
        perror("execvp failure");
        exit(1); /* In cases of child failure I certainly don't want the child to return to shell.c */
    }
    if (waitpid(pid, &status, 0) == WAITPID_FAILURE) {
        perror("waitpid failure in shell");
        return 0;
    }
    return 1;
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
