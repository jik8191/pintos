#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include "mysh.h"
#include "y.tab.h"

#define MAX_BUFSIZE 1024
// TODO: reallocate memory to support arbitrary length buffer/filepath

extern int yyparse(parsed *line);
int loop();

int main() {
    int exit;

    exit = 0;

    while(exit == 0) {
        exit = loop();
    }

    return 0;
}

int loop() {
    char *username;
    char *dir_curr;
    char prompt[500] = "\0";
    int exit;
    char *dir_home = (char *) malloc(sizeof(char) * MAX_BUFSIZE);

    parsed *line = (parsed *) malloc(sizeof(parsed));
    line->new = 1;
    line->frst = NULL;
    line->curr = NULL;
    /* command *cmd = (command *) malloc(sizeof(command)); */
    /* cmd->curr = cmd; */
    /* token *root = (token *) malloc(sizeof(token)); */
    // TODO: null check

    /* root = NULL; */
    /* cmd->first_token = root; */

    exit = 0;

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

    exit = yyparse(line);
    if (exit == 1) {
        return exit;
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

    // cd command
    if ((strcmp("cd", cmd->first_token->value) == 0)) {
        if (cmd->first_token->next == NULL ||
                !strcmp("~", cmd->first_token->next->value)){
            chdir(dir_home);
        }
        else {
            chdir(cmd->first_token->next->value);
        }
    }
    else if ((strcmp("ls", cmd->first_token->value) == 0)) {
        pid_t pid;
        pid = fork();
        if (pid == 0) {
            execlp("ls", "ls", NULL);
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

    return exit;
}

/*
   char *get_args(command *cmnd_struct) {

   }
   */
