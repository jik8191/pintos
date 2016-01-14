#ifndef MYSH_H
#define MYSH_H

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include <pwd.h>
#include <fcntl.h>
#include <readline/readline.h>
#include <readline/history.h>

// This is a linked list node for arguments to a command.
typedef struct token {
    char *value;
    struct token *next;
} token;

// This is a single command (separated by pipes)
typedef struct command {
    token *first_token;     // First token
    token *last_token;      // Last token
    struct command *next;   // Next command in sequence
    char *inredir;          // The file to get input from
    char *outredir;         // The file to send output to
    int outappend;          // Whether or not to append output
    int fdout;              // If we have a ' n> ' redirect, this is n
    char *fdredir;          // The file to redirect output specified by fdout
} command;

// This is an object containing the parsed entirety of a user's input
typedef struct parsed {
    int error;
    command *first;
    command *curr;
} parsed;

// Definitions for external (parser and lexer) functions
extern void set_input(char *str);
extern void clear();
extern int yyparse(parsed *line);

// Main shell loop
int loop();

// Executes a single command with given pipes
int exec_cmd(command *cmd, int *prevfds, int *currfds);

// Frees the memory for a single user's shell input
void free_line(parsed *line);

// Converts linked list of arguments into an array
char **tokenize(command *cmd);

#endif // MYSH_H
