#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdio.h>
#include <signal.h>

#define STREQ(a, b) (strcmp((a), (b)) == 0)
#define FORK_FAILURE -1
#define NOT_FOUND -1


void mySignalHandler(int signum) {}

int prepare(void)
{
    struct sigaction newAction = {.sa_handler = mySignalHandler};
    if (sigaction(SIGINT, &newAction, NULL) < 0) {
        perror("Signal handle registration failed\n");
        return 1;
    }
    return 0;
}

int finalize(void)
{
    printf("\nfinalize\n");
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
    return STREQ(arglist[count - 2], ">");
}

static int has_left_redirection(int count, char **arglist)
{
    return STREQ(arglist[count - 2], "<");
}

/* ~~~~~~~~~~~~~~~~~~~ Handling ~~~~~~~~~~~~~~~~~~~ */

static void handle_default(int count, char **arglist)
{
    char *cmd = arglist[0];
    int status;
    pid_t pid = fork();
    if (pid == FORK_FAILURE) {
        /* Handle fork failure */
    }
    if (pid == 0)
    {
        execvp(cmd, arglist);
        /* If execvp returns, it must have failed */
    }
    else
    {
        waitpid(pid, &status, 0);
        /* Handle child status */
    }
}

// static void handle_ampersand(int count, char **arglist)
// {

// }

int process_arglist(int count, char **arglist)
{
    int ha = has_ampersand(count, arglist);
    int pi = pipe_index(count, arglist);
    int hr = has_right_redirection(count, arglist);
    int hl = has_left_redirection(count, arglist);

    if (ha)
    {
        /* code */
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