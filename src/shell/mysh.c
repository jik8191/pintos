#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <errno.h>
#include <pwd.h>
#include <fcntl.h>
#include "mysh.h"
#include "y.tab.h"
#include <readline/readline.h>
#include <readline/history.h>

#define MAX_BUFSIZE 1024
// TODO: reallocate memory to support arbitrary length buffer/filepath

extern int yyparse(parsed *line);
extern void set_input(char *str);
extern void clear();
int loop();
int exec_cmd(command *cmd, int *prevfds, int *currfds);

int main() {
    int exitcode;

    exitcode = 0;

    while(exitcode == 0) {
        exitcode = loop();
    }

    return 0;
}

#define MAX_TOK_BUFSIZE 64
char **tokenize(command *cmnd_struct){
    char **tokens = malloc(MAX_TOK_BUFSIZE*sizeof(char*)); // array of tokens
    token *tok_temp = cmnd_struct->first_token;
    int index = 0;

    if (!tokens){
        fprintf(stderr, "error: memory allocation error\n");
        exit(EXIT_FAILURE);
    }

    while (tok_temp != NULL){
        tokens[index] = tok_temp->value;
        index+= 1;
        tok_temp = tok_temp->next;
        // TODO: reallocate if we run out of space
    }

    tokens[index] = NULL;

    return tokens;
}

int loop() {
    char *username;
    char *dir_curr;
    char prompt[500] = "\0";
    int exitcode;

    parsed *line = (parsed *) malloc(sizeof(parsed));
    line->frst = NULL;
    line->curr = NULL;
    line->error = 0;

    exitcode = 0;

    // Username of the session
    /* username = getlogin(); */
    username = getpwuid(getuid())->pw_name;

    // Current directory
    dir_curr = getcwd(NULL, MAX_BUFSIZE); // Need to free this?

    // Getting the current prompt
    strcat(strcat(strcat(strcat(prompt, username), ":"), dir_curr), "> ");

    // Displaying the prompt
    /*printf("%s ", prompt);*/

    // Getting user input via readline
    static char *user_input = (char *) NULL;

    user_input = readline(prompt);

    if(user_input && *user_input) {
        add_history(user_input);
    }

    strcat(user_input, "\n");
    set_input(user_input);
    exitcode = yyparse(line);
    clear();
    free(user_input);
    if (exitcode != 0) {
        return 1; // User asked to exit
    }
    if (line->error != 0) {
        return 0; // Parse error, so skip this loop
    }

    if (line->frst == NULL) {
        return 0;
    }

    int prevfds[2] = {-1};
    int currfds[2] = {-1};

    command *cmd = line->frst;

    for (; cmd->next != NULL; cmd = cmd->next) {
        if (pipe(currfds) == -1) {
            printf("error: failed to open pipe\n");
            return 0;
        }

        exitcode = exec_cmd(cmd, prevfds, currfds);
        // TODO: Maybe check error here.

        *prevfds = *currfds;
    }

    exitcode = exec_cmd(cmd, prevfds, NULL);

    /* Memory freeing */

    for (cmd = line->frst; cmd != NULL; cmd = cmd->next) {
        token *temp;

        for (temp = cmd->first_token; temp != NULL; temp = temp->next) {
            free(temp);
        }

        free(cmd);
    }

    free(line);

    /******************/

    return exitcode;
}


/**
 * @brief Executes a command specified by a command struct.
 *
 * `cd`s or `chdir`s are handled differently.
 */
int exec_cmd(command* cmd, int *prevfds, int *currfds) {

    if (strcmp("cd", cmd->first_token->value) == 0 ||
        strcmp("chdir", cmd->first_token->value) == 0) {

        // CD to the user's home directory by default or with ~
        if (cmd->first_token->next == NULL ||
            !strcmp("~", cmd->first_token->next->value)){

            // Home directory of the user
            char *dir_home = (char *) malloc(sizeof(char) * MAX_BUFSIZE);
            dir_home = getenv("HOME");

            chdir(dir_home);

        } else {
            chdir(cmd->first_token->next->value);
        }

    } else {

        pid_t pid;
        char **tokens = tokenize(cmd);

        pid = fork();

        if (pid == -1) {
            printf("error: failed to fork process\n");
            return 0;
        }

        if (pid == 0) {

            // If we are given the file descriptors of the previous process in
            // the pipe chain, then use that pipe as our STDIN.
            if (prevfds != NULL && prevfds[0] != -1) {
                close(prevfds[1]);
                dup2(prevfds[0], STDIN_FILENO);
                close(prevfds[0]);
            }

            // If we have a pipe set up for the next process in the pipe chain,
            // then use the pipe sa our STDOUT.
            if (currfds != NULL && currfds[0] != -1) {
                close(currfds[0]);
                dup2(currfds[1], STDOUT_FILENO);
                close(currfds[1]);
            }

            // If we have an input redirect, we set the specified file to the
            // STDIN. This will overwrite any piped input, which is consistent
            // with bash's behavior.
            if (cmd->input_redirection != NULL) {
                char *fname = cmd->input_redirection;
                int in_fd = open(fname, O_RDONLY);

                if (in_fd < 0) {
                    printf("error: could not read from file: %s\n", fname);
                    exit(EXIT_FAILURE);
                }

                dup2(in_fd, STDIN_FILENO);
                close(in_fd);
            }

            // If we have an output redirect, we set the specified file to the
            // STDOUT. This means that nothing will be piped to the next
            // function if any, which is consistent with bash's behavior.
            if (cmd->output_redirection != NULL) {
                char *fname = cmd->output_redirection;
                int out_fd = open(fname, O_WRONLY | O_CREAT | O_TRUNC);

                if (out_fd < 0) {
                    printf("error: could not write to file: %s\n", fname);
                    exit(EXIT_FAILURE);
                }

                dup2(out_fd, STDOUT_FILENO);
                close(out_fd);
            }

            execvp(tokens[0], tokens);

            // If the above function returned, then the command doesn't exist.
            printf("error: that command could not be found\n");
            exit(errno); // we exit here because we need the child to quit
        } else {
            if (currfds != NULL) {
                close(currfds[1]);
            }

            wait(NULL);
        }
    }

    return 0;
}


