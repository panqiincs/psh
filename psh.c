#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>


#define MAX_ARG_NUM          20    // max number of arguments
#define MAX_ARG_LEN          200   // max length of commands
#define MAX_PROMPT_LEN       100   // max length of prompt string

#define CMD_NORMAL           0x00  // normal comand without any symbols
#define CMD_INPUT_REDIRECT   0x01  // contains '<' symbol
#define CMD_OUTPUT_REDIRECT  0x02  // contains '>' symbol
#define CMD_PIPELINE         0x04  // contains '|' symbol
#define CMD_INVALID          0x08  // contains more than one symbol


void print_prompt(char *);
void handle_input(char *, char **);
void run_cmd(char **);


int main(int argc, char **argv)
{
    char *arg_buf;                       // store user input strings
    char *arg_list[MAX_ARG_NUM+1];       // argument list
    char  prompt_str[MAX_PROMPT_LEN+1];  // store prompt string

    // in parent process, ignore SIGINT and SIGQUIT, or process
    // will quit
    signal(SIGINT, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);

    arg_buf = (char *)malloc(sizeof(char)*MAX_ARG_LEN);

    for ( ; ; ) {
        print_prompt(prompt_str);
        handle_input(arg_buf, arg_list);
        run_cmd(arg_list);
    }

    free(arg_buf);
    exit(EXIT_SUCCESS);
}


/**
 * Construct a prompt string, store it in an charactor array which address
 * is stringp and print out. A prompt should be like this:
 *
 *   [psh]krist@linux-szhw:/home/krist/Workspace$
 *     |    |   <--------> <------------------->|
 *     |    |        |               |          |
 *   header username hostname       pwd       prompt
 *
 * @param stringp  a pointer to a char array that store prompt string, the
 *                 function modifies the array content
 * @return void
 */
void print_prompt(char *stringp)
{
    size_t offset = 0;

    // header [psh]
    stringp[offset++] = '[';
    stringp[offset++] = 'p';
    stringp[offset++] = 's';
    stringp[offset++] = 'h';
    stringp[offset++] = ']';

    // username@hostname
    getlogin_r(stringp + offset, MAX_PROMPT_LEN - offset);
    offset += strlen(stringp + offset);
    stringp[offset++] = '@';
    gethostname(stringp + offset, MAX_PROMPT_LEN - offset);
    offset += strlen(stringp + offset);

    stringp[offset++] = ':';

    // current directory
    getcwd(stringp + offset, MAX_PROMPT_LEN - offset);
    offset += strlen(stringp + offset);

    // prompt
    if (geteuid() == 0) {
        stringp[offset++] = '#';  // superuser
    } else {
        stringp[offset++] = '$';  // normal
    }

    stringp[offset++] = ' ';
    stringp[offset] = '\0';

    printf("%s", stringp);
}


/**
 * Check the command type:
 * NORMAL, INPUT REDIRECT, OUTPUT REDIRECT, PIPELINE or INVALID
 *
 * @param arg_vec  an array of pointers to arguments, terminated by NULL
 * @param pos      return the position of the symbol
 *
 * @return         command type
 */
int check_cmd(char **arg_vec, int *pos)
{
    int item_offset = -1;       // used to locate each string
    int symbol_cnt = 0;         // number of symbols
    int cmd_type = CMD_NORMAL;  // command type, normal in default
    *pos = -1;                  // initial position of the symbol, invalid

    char **ptr;
    for (ptr = arg_vec; *ptr != NULL; ptr++) {
        ++item_offset;
        if (strcmp(*ptr, "<") == 0) {
            cmd_type = CMD_INPUT_REDIRECT;
            *pos = item_offset;
            ++symbol_cnt;
        } else if (strcmp(*ptr, ">") == 0) {
            cmd_type = CMD_OUTPUT_REDIRECT;
            *pos = item_offset;
            ++symbol_cnt;
        } else if (strcmp(*ptr, "|") == 0) {
            cmd_type = CMD_PIPELINE;
            *pos = item_offset;
            ++symbol_cnt;
        } else {

        }

        // at most one symbol is allowed
        if (symbol_cnt > 1) {
            *pos = -1;
            return CMD_INVALID;
        }
    }

    // symbol appears at the beginning or end
    if (*pos == 0 || *pos == item_offset) {
        return CMD_INVALID;
    }

    return cmd_type;
}


/**
 * Store user input in input_buf, parse it to arguments, and store the
 * argument list in arg_vec
 *
 * @param input_buf  a pointer to a char array, store one line of user
 *                   input, the caller should guarantee the buffer size is
 *                   big enough
 * @param arg_vec  an array of pointers to arguments, terminated by NULL
 *
 * @return void
 */
void handle_input(char *input_buf, char **arg_vec)
{
    size_t  len;         // max length of arg string buffer
    ssize_t num_read;    // number of characters read from stdin
    size_t  arg_num;     // arguments count
    char   *token;
    char   *stringp;

    len = MAX_ARG_LEN;
    num_read = 0;
    if ((num_read = getline(&input_buf, &len, stdin)) == -1) {
        exit(EXIT_FAILURE);
    }
    if (len > MAX_ARG_LEN) {
        printf("Exceeds max argument length limit, please try again!\n");
        exit(EXIT_FAILURE);
    }

#ifdef DEBUG
    printf("\ninput_buf: address = %p, capacity = %zu\n\n", input_buf, len);
    printf("retrieved line of length %zu:\n", num_read);
    printf("\t%s\n", input_buf);
#endif

    input_buf[num_read - 1] = '\0';  // overwrite the newline character

    stringp = input_buf;
    arg_num = 0;       // number of arguments
    while (((token = strsep(&stringp, " ")) != NULL)
           && (arg_num < MAX_ARG_NUM))
    {
        // Token is terminated by overwriting the delimiter with a null
        // byte('\0'), so continuous space will result in a token with
        // only a null byte, skip it.
        if (strcmp(token, "") != 0) {
            arg_vec[arg_num] = token;
            arg_num++;
        }
    }
    arg_vec[arg_num] = NULL;

#ifdef DEBUG
    printf("\n%zu arguments in total:\n", arg_num);
    char **ptr;
    for (ptr = arg_vec; *ptr != NULL; ptr++) {
        printf("\t%s\n", *ptr);
    }
    printf("\n");
#endif

#ifdef DEBUG
    int pos;
    int cmd_type = check_cmd(arg_vec, &pos);
    printf("cmd_type = %d, symbol pos = %d\n\n", cmd_type, pos);
#endif
}


/**
 * Execute a normal command, no redirect, no pipeline
 *
 * @param arg_vec  an array of pointers to arguments, terminated by NULL
 *
 * @return void
 */
void exec_normal(char **arg_vec)
{
    execvp(arg_vec[0], arg_vec);
    perror("execvp");
    exit(EXIT_FAILURE);
}


/**
 * Execute a input redirect command
 *
 * @param arg_vec  an array of pointers to arguments, terminated by NULL
 * @param pos      the position of the '<' symbol
 *
 * @return void
 */
void exec_input_redirect(char **arg_vec, int pos)
{
    char *filename = arg_vec[pos+1];  // filename locate after symbol '<'
    arg_vec[pos] = NULL;              // comand strings locate before symbol

    // open the file for input and redirect stdin
    int fdin = open(filename, O_RDONLY);
    if (fdin == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }
    if (dup2(fdin, STDIN_FILENO) == -1) {
        perror("dup2");
        exit(EXIT_FAILURE);
    }
    if (close(fdin) == -1) {
        perror("close");
        exit(EXIT_FAILURE);
    }

    execvp(arg_vec[0], arg_vec);
    perror("execvp");
    exit(EXIT_FAILURE);
}


/**
 * Execute a output redirect command
 *
 * @param arg_vec  an array of pointers to arguments, terminated by NULL
 * @param pos      the position of the '>' symbol
 *
 * @return void
 */
void exec_output_redirect(char **arg_vec, int pos)
{
    char *filename = arg_vec[pos + 1];  // filename locate after symbol '>'
    arg_vec[pos] = NULL;                // comand strings locate before symbol

    // create a file for output and redirect stdout
    int fdout = open(filename, O_WRONLY | O_CREAT | O_TRUNC,
                     S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (fdout == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }
    if (dup2(fdout, STDOUT_FILENO) == -1) {
        perror("dup2");
        exit(EXIT_FAILURE);
    }
    if (close(fdout) == -1) {
        perror("close");
        exit(EXIT_FAILURE);
    }

    execvp(arg_vec[0], arg_vec);
    perror("execvp");
    exit(EXIT_FAILURE);
}


/**
 * Execute a pipeline command
 *
 * @param arg_vec  an array of pointers to arguments, terminated by NULL
 * @param pos      the position of the '>' symbol
 *
 * @return void
 */
void exec_pipeline(char **arg_vec, int pos)
{
    char **arg_vec1 = &arg_vec[0];      // first command
    arg_vec[pos] = NULL;                // the separator of two commands
    char **arg_vec2 = &arg_vec[pos+1];  // second command

#ifdef DEBUG
    char **ptr;
    printf("\ncommand 1:\n");
    for (ptr = arg_vec1; *ptr != NULL; ptr++) {
        printf("\t%s\n", *ptr);
    }
    printf("\n");

    printf("\ncommand 2:\n");
    for (ptr = arg_vec2; *ptr != NULL; ptr++) {
        printf("\t%s\n", *ptr);
    }
    printf("\n");
#endif

    int pfd[2];
    if (pipe(pfd) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    // first child, run the first command
    switch (fork()) {
    case -1:
        perror("fork");
        exit(EXIT_FAILURE);
    case 0:
        if (close(pfd[0]) == -1) {
            perror("close");
            exit(EXIT_FAILURE);
        }
        if (pfd[1] != STDOUT_FILENO) {
            if (dup2(pfd[1], STDOUT_FILENO) == -1) {
                perror("dup2");
                exit(EXIT_FAILURE);
            }
            if (close(pfd[1]) == -1) {
                perror("close");
                exit(EXIT_FAILURE);
            }
        }

        execvp(arg_vec1[0], arg_vec1);
        perror("execvp");
        exit(EXIT_FAILURE);
    default:
        break;
    }

    // second child, run the second command
    switch (fork()) {
    case -1:
        perror("fork");
        exit(EXIT_FAILURE);
    case 0:
        if (close(pfd[1]) == -1) {
            perror("close");
            exit(EXIT_FAILURE);
        }
        if (pfd[0] != STDIN_FILENO) {
            if (dup2(pfd[0], STDIN_FILENO) == -1) {
                perror("dup2");
                exit(EXIT_FAILURE);
            }
            if (close(pfd[0]) == -1) {
                perror("close");
                exit(EXIT_FAILURE);
            }
        }

        execvp(arg_vec2[0], arg_vec2);
        perror("execvp");
        exit(EXIT_FAILURE);
    default:
        break;
    }

    if (close(pfd[0]) == -1) {
        perror("close");
        exit(EXIT_FAILURE);
    }
    if (close(pfd[1]) == -1) {
        perror("close");
        exit(EXIT_FAILURE);
    }
    if (wait(NULL) == -1) {
        perror("wait");
        exit(EXIT_FAILURE);
    }
    if (wait(NULL) == -1) {
        perror("wait");
        exit(EXIT_FAILURE);
    }

    exit(EXIT_SUCCESS);
}


/**
 * Execute a non built-in command, fork a child process for each
 * command to run, the parent process wait for children to exit
 *
 * @param arg_vec  an array of pointers to arguments, terminated by NULL
 * @return void
 */
void exec_cmd(char **arg_vec)
{
    int   status;
    pid_t pid;

    pid = fork();
    switch (pid) {
    case -1:
        perror("fork");
        exit(EXIT_FAILURE);
    case 0:
        // in child process, default actions
        signal(SIGINT, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);

        int symbol_pos;
        int cmd_type = check_cmd(arg_vec, &symbol_pos);
        switch (cmd_type) {
        case CMD_NORMAL:
            exec_normal(arg_vec);
        case CMD_INPUT_REDIRECT:
            exec_input_redirect(arg_vec, symbol_pos);
        case CMD_OUTPUT_REDIRECT:
            exec_output_redirect(arg_vec, symbol_pos);
        case CMD_PIPELINE:
            exec_pipeline(arg_vec, symbol_pos);
        case CMD_INVALID:
            fprintf(stderr, "Invalid command syntax!\n");
            exit(EXIT_FAILURE);
        default:
            break;
        }
    default:
        while (wait(&status) != pid)
            ;
    #ifdef DEBUG
        printf("\nchild exited with status %d, %d\n",
               status>>8, status&0377);
    #endif
    }
}


/**
 * Check if it is a built-in command, if it is, run it in parent process
 * without a fork, if not, return
 *
 * @param arg_vec  an array of pointers to arguments, terminated by NULL
 * @return 0  it is a built-in command, finish running and return
 *        -1  it is not a built-in command and return directly
 */
int builtin_cmd(char **arg_vec)
{
    if (strcmp(arg_vec[0], "cd") == 0) { // cd
        if ((arg_vec[1] != NULL) && (arg_vec[2] == NULL)) {
            if (chdir(arg_vec[1]) == -1) {
                perror("cd");
            }
        } else {
            printf("Usage: cd [directory]\n");
        }
        return 0;
    } else if (strcmp(arg_vec[0], "exit") == 0) { // exit
        exit(EXIT_SUCCESS);
    }

    return -1;
}

/**
 * Run the command, both built-in and normal commands
 *
 * @param arg_vec  an array of pointers to arguments, terminated by NULL
 * @return void
 */
void run_cmd(char **arg_vec)
{
    if (arg_vec[0] == NULL) {
        return;
    }

    if (builtin_cmd(arg_vec) == -1) { // run build-in commands
        exec_cmd(arg_vec); // run normal commands
    }
}

