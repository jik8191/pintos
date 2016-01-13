#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <errno.h>
#include <pwd.h>
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
    line->error = 0;
    // TODO: null check

    /* root = NULL; */
    /* cmd->first_token = root; */

    exitcode = 0;

    // Username of the session
    /* username = getlogin(); */
    username = getpwuid(getuid())->pw_name;
    // Home directory of the user
    dir_home = getenv("HOME");
    // Current directory
    dir_curr = getcwd(NULL, MAX_BUFSIZE); // Need to free this?
    // Getting the current prompt
    strcat(strcat(strcat(strcat(prompt, username), ":"), dir_curr), ">");
    // Displaying the prompt
    printf("%s ", prompt);

    exitcode = yyparse(line);
    if (exitcode != 0) {
        return 1; // User asked to exit
    }
    if (line->error != 0) {
        return 0; // Parse error, so skip this loop
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

        int pipefd[2];
        pid_t pid;
        int has_next;
        int prev_fd;

        for (cmd = line->frst; cmd != NULL; cmd = cmd->next) {
            tokens = tokenize(cmd);

            if (cmd->next != NULL) {
                has_next = 1;
            } else {
                has_next = 0;
            }

            if (has_next && pipe(pipefd) == -1) {
                printf("failed to open pipe in command\n");
                return 0;
            }

            pid = fork();

            if (pid == -1) {
                printf("failed to fork process\n");
                exit(EXIT_FAILURE);
            }

            if (pid == 0) {
                dup2(prev_fd, STDIN_FILENO);
                close(prev_fd);

                // TODO: We can just run a command outside the loop and change
                // the indexing of the loop so we don't need these
                // conditionals.
                if (has_next) {
                    close(pipefd[0]); // Close read end, we are writing to it.
                    dup2(pipefd[1], STDOUT_FILENO);
                    close(pipefd[1]);
                }

                execvp(tokens[0], tokens);

                // If we return here, then exec failed.
                printf("That command could not be found.\n");
                exit(errno);
            } else {
                close(pipefd[1]); // Close write end, we are reading from it.

                prev_fd = pipefd[0];

                wait(NULL);
            }

        }

        /*
        tokens = tokenize(cmd);
        pid = fork();

        if (pid == 0) {
            execvp(tokens[0], tokens);

            // If we return here, then exec failed.
            printf("That command could not be found.\n");
            exit(errno);
        } else {

            close(pipefd[0]);
            close(pipefd[1]);

            wait(NULL);
        }
        */
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

