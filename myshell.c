#include <string.h>
#define STREQ(a, b) (strcmp((a), (b)) == 0)



int prepare(void)
{
    return 0;
}

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
    return -1;
}

static int has_right_redirection(int count, char **arglist)
{
    return STREQ(arglist[count - 2], ">");
}

static int has_left_redirection(int count, char **arglist)
{
    return STREQ(arglist[count - 2], "<");
}


int process_arglist(int count, char **arglist)
{
    int ha = has_ampersand(count, arglist);
    int pi = pipe_index(count, arglist);
    int hr = has_right_redirection(count, arglist);
    int hl = has_left_redirection(count, arglist);

    return 0;
}