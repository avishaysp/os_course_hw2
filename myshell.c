#include <stdlib.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdio.h>
#include <signal.h>
#include <errno.h>

#define STREQ(a, b) (strcmp((a), (b)) == 0)
#define FORK_FAILURE -1
#define NOT_FOUND -1
#define DEBUG_PRINT(x) (printf("%d\n", x))

void mySignalHandler(int signum) {printf("an't stop me\n");}
static pid_t* sons;
static int num_of_sons;

int prepare(void)
{
    struct sigaction sa = {.sa_handler = mySignalHandler};
    if (sigaction(SIGINT, &sa, NULL) < 0) {
        perror("Signal handle registration failed\n");
        return 1;
    }
    sons = (pid_t*)malloc(sizeof(pid_t));
    if (sons == NULL) {
        printf("prepare malloc failed: %s\n", strerror(errno));
        return 1;
    }
    num_of_sons = 0;
    sons = (pid_t*)malloc(sizeof(pid_t));
    if (sons == NULL) {
        printf("prepare malloc failed: %s\n", strerror(errno));
        return 1;
    }
    num_of_sons = 0;
    return 0;
}

int finalize(void)
{
    int i;
    int status;
    printf("\nFinalize\n");
    for (i = 0; i < num_of_sons; i++)
    {
        if (sons[i] > 0) {
            waitpid(sons[i], &status, 0);
        }
    }
    free(sons);
    return 0;
}

/* ~~~~~~~~~~~~~~~~ Classification ~~~~~~~~~~~~~~~~ */

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
        {
            return i;
        }
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

/* ~~~~~~~~~~~~~~~~~~~ Handling ~~~~~~~~~~~~~~~~~~~ */

static void handle_default(int count, char **arglist)
{
    char *cmd = arglist[0];
    int status;
    pid_t pid = fork();
    if (pid == FORK_FAILURE) {
        perror("fork failure\n");
        /* Handle fork failure */
    }
    if (pid == 0)
    {
        execvp(cmd, arglist);
        perror("execvp failure\n");
        /* If execvp returns, it must have failed */
    }
    else
    {
        waitpid(pid, &status, 0);
        /* Handle child status */
    }
}

static pid_t handle_ampersand(int count, char **arglist)
{
    char *cmd = arglist[0];
    pid_t pid;
    arglist[count - 1] = NULL;
    pid = fork();
    if (pid == FORK_FAILURE) {
        /* Handle fork failure */
    }
    if (pid == 0)
    {
        execvp(cmd, arglist);
        /* If execvp returns, it must have failed */
    }
    return pid;
}

int process_arglist(int count, char **arglist)
{
    int ha = has_ampersand(count, arglist);
    int pi = pipe_index(count, arglist);
    int hr = has_right_redirection(count, arglist);
    int hl = has_left_redirection(count, arglist);
    pid_t son;

    if (ha)
    {
        son = handle_ampersand(count, arglist);
        printf("new son: %d\n", son);
        num_of_sons++;
        printf("num of sons: %d\n", num_of_sons);
        sons = (pid_t*)realloc(sons, sizeof(pid_t) * num_of_sons);
        if (sons == NULL) {
            printf("sons realloc failed: %s\n", strerror(errno));
            return 0;
        }
        sons[num_of_sons - 1] = son;
    }
    else if (pi != NOT_FOUND)
    {
        /* code */
    }
    else if (hr)
    {
        /* code */
    }
    else if (hl)
    {
        /* code */
    }
    else
    {
        handle_default(count, arglist);
    }

    return 1;
}