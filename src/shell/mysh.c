#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
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


void print_prompt(){
    char *prompt = (char *) malloc(sizeof(char) * MAX_BUFSIZE);
    strcat(prompt, "\0");
    char *username;
    char *dir_curr;

    username = getlogin();      // Username of the session
    // Current directory
    dir_curr = getcwd(NULL, MAX_BUFSIZE); // Need to free this?
    // Getting the current prompt
    strcat(strcat(strcat(strcat(prompt, username), ":"), dir_curr), ">");
    printf("%s ", prompt);
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


void exec_cmd(command *cmd){
    int debug = 1;
    char *dir_home = (char *) malloc(sizeof(char) * MAX_BUFSIZE);
    /* // TODO: This might fail */
    dir_home = getenv("HOME");  // Home directory of the user */
    char **tokens = tokenize(cmd);
    token *temp;
    int fd_ip;
    int fd_op;

    
    if (debug){
        printf("Command String: ");
        for (temp = cmd->first_token; temp != NULL; 
             temp = temp->next) { 
            printf("%s ", temp->value); 
        }
        printf("\n");
        if (cmd->input_redirection != NULL){
            printf("Input redirect to: ");
            printf("%s ", cmd->input_redirection); 
        } else if (cmd->output_redirection != NULL){
            printf("Output redirect to: ");
            printf("%s ", cmd->output_redirection); 
        }
	printf("\n");
    }
    
    // TODO: refactor to include all builtin commands
    // cd command. TODO: recognize chdir, make this look nicer
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
            // child process
            // first argument is file to
	    if (cmd->input_redirection != NULL){
		// obtain file descriptor for input file
		fd_ip = open(cmd->input_redirection, O_RDONLY);
		// modify input file descriptor of child process
		dup2(fd_ip, STDIN_FILENO);
		// close file descriptor
		// TODO: check that it actually is closed (-1 on failure)
		close(fd_ip);
	    }
	    if (cmd->output_redirection != NULL){
		// obtain file descriptor for output file, which might fail.
		// TODO: include O_APPEND later
		fd_op = open(cmd->output_redirection, O_WRONLY);
		// modify input file descriptor of child process
		dup2(fd_op, STDOUT_FILENO);
		// close file descriptor
		// TODO: check that it actually is closed (-1 on failure)
		close(fd_op);
	    }
	    
            execvp(tokens[0], tokens);
            printf("That command could not be found.\n");
            exit(errno);
        } else if (pid < 0){
            // unsuccessful fork
            exit(errno); // perhaps print?
        }
        else {
            // parent process
            wait(NULL);
        }
    }
}


void free_all(command *cmd, parsed* line){
    /* for (cmd = line->frst; cmd != NULL; cmd = cmd->next) { */
    /*     token *temp; */
    /*     for (temp = cmd->first_token; temp != NULL; temp = temp->next) { */
    /*         free(temp); */
    /*     } */
    /*     free(cmd); */
    /* } */
    free(line);
}


int loop() {
    int exitcode = 0;

    parsed *line = (parsed *) malloc(sizeof(parsed));
    line->frst = NULL;
    line->curr = NULL;
    line->error = 0;
    // TODO: null checks for malloc

    print_prompt();
    exitcode = yyparse(line);
    // TODO: move logic here to a function which handles all builtins
    if (exitcode != 0) {
        return 1; // User asked to exit
    }
    if (line->error != 0) {
        return 0; // Parse error, so skip this loop
    }

    
    command *cmd = line->frst;
    if (cmd == NULL) {
        return 0;
    }

    
    exec_cmd(cmd);
    free_all(cmd, line);

    return exitcode;
}

