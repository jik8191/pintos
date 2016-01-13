#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "mysh.h"
#include "y.tab.h"

#define MAX_BUFSIZE 1024
// TODO: reallocate memory to support arbitrary length buffer/filepath

extern int yyparse(command *cmnd_struct);
int loop();

int main() {
    int exit;

    exit = 0;

    while(exit == 0) {
        exit = loop();
    }
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
    int exit;
    char *dir_home = (char *) malloc(sizeof(char) * MAX_BUFSIZE);
    command *cmnd_struct = (command *) malloc(sizeof(command));
    token *root = (token *) malloc(sizeof(token));
    char **tokens;
    // TODO: null check
    
    root = NULL;
    cmnd_struct->first_token = root;

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

    exit = yyparse(cmnd_struct);
    printf("Command type: %c\n", cmnd_struct->type);
    printf("Command String: ");
    token *temp;
    for (temp = cmnd_struct->first_token; temp != NULL; temp = temp->next) {
        printf("%s ", temp->value);
    }
    printf("\n");
    tokens = tokenize(cmnd_struct);
    
    // TODO: recognize chdir
    if ((strcmp("cd", cmnd_struct->first_token->value) == 0)) {
        if (cmnd_struct->first_token->next == NULL ||
            !strcmp("~", cmnd_struct->first_token->next->value)){
            chdir(dir_home); 
        }
        else {
            chdir(cmnd_struct->first_token->next->value);
        }
    }
    else {
        pid_t pid;
        pid = fork();
        // TODO: check if pid = -1 (failed to fork)
        if (pid == 0) {
            // first argument is file to 
            execvp(tokens[0], tokens);
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
