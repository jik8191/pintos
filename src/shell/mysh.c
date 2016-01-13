#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <errno.h>
#include "mysh.h"
#include "y.tab.h"

#define MAX_BUFSIZE 1024
// TODO: reallocate memory to support arbitrary length buffer/filepath

extern int yyparse(parsed *line);
int loop();

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
        fprintf(stderr, "memory allocation error\n");
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
    char *dir_home = (char *) malloc(sizeof(char) * MAX_BUFSIZE);

    parsed *line = (parsed *) malloc(sizeof(parsed));
    line->frst = NULL;
    line->curr = NULL;
    // TODO: null check

    /* root = NULL; */
    /* cmd->first_token = root; */

    exitcode = 0;

    // Username of the session
    username = getlogin();
    // Home directory of the user
    dir_home = getenv("HOME");
    // Current directory
    dir_curr = getcwd(NULL, MAX_BUFSIZE); // Need to free this?
    // Getting the current prompt
    strcat(strcat(strcat(strcat(prompt, username), ":"), dir_curr), ">");
    // Displaying the prompt
    printf("%s ", prompt);

    exitcode = yyparse(line);
    if (exitcode == 1) {
        return exitcode;
    }
    /* printf("Command type: %c\n", cmnd_struct->type); */
    /* printf("Command String: "); */
    /* for (temp = cmnd_struct->first_token; temp != NULL; temp = temp->next) { */
    /*     printf("%s ", temp->value); */
    /* } */
    /* printf("\n"); */


    command *cmd = line->frst;
    if (cmd == NULL) {
        return 0;
    }

    char **tokens = tokenize(cmd);

    // cd command. TODO: recognize chdir
    if ((strcmp("cd", cmd->first_token->value) == 0)) {
        if (cmd->first_token->next == NULL ||
                !strcmp("~", cmd->first_token->next->value)){
            chdir(dir_home);
        }
        else {
            chdir(cmd->first_token->next->value);
        }
    } else {
        pid_t pid;
        pid = fork();

        // TODO: check if pid = -1 (failed to fork)
        if (pid == 0) {
            // first argument is file to
            execvp(tokens[0], tokens);

            printf("That command could not be found.\n");
            exit(errno);
        }
        else {
            wait(NULL);
        }
    }

    // Freeing the memory
    for (cmd = line->frst; cmd != NULL; cmd = cmd->next) {
        token *temp;

        for (temp = cmd->first_token; temp != NULL; temp = temp->next) {
            free(temp);
        }

        free(cmd);
    }

    free(line);

    return exitcode;
}

