#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "mysh.h"
#include "y.tab.h"

extern int yyparse(command *cmnd_struct);
int loop();

int main() {
    int exit;

    exit = 0;

    while(exit == 0) {
        exit = loop();
    }
}

int loop() {
    char *username;
    char *directory;
    char prompt[500] = "\0";
    int exit;
    command *cmnd_struct = (command *) malloc(sizeof(command));
    token *root = (token *) malloc(sizeof(token));

    root = NULL;
    cmnd_struct->first_token = root;

    exit = 0;

    // Username of the session
    username = getlogin();
    // Current directory
    directory = getcwd(NULL, 100); // Arbitrary buff size. Need to free this?
    // Getting the current prompt
    strcat(strcat(strcat(strcat(prompt, username), ":"), directory), ">");
    // Displaying the prompt
    printf("%s ", prompt);

    exit = yyparse(cmnd_struct);
    printf("Command type: %c\n", cmnd_struct->type);
    printf("Command String: ");
    token *temp;
    for (temp = cmnd_struct->first_token; temp != NULL; temp = temp->next) {
        printf("%s ", temp->value);
    }
    printf("\n");

    // cd command
    if ((strcmp("cd", cmnd_struct->first_token->value) == 0)) {
        chdir(cmnd_struct->first_token->next->value); 
    }
    else if ((strcmp("ls", cmnd_struct->first_token->value) == 0)) {
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
    for (temp = cmnd_struct->first_token; temp != NULL; temp = temp->next) {
        free(temp);
    }
    free(cmnd_struct);
   
    return exit;
}

/*
char *get_args(command *cmnd_struct) {
    
}
*/
