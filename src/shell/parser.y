%{
#include <stdio.h>
#include <string.h>
#include "mysh.h"

// Function declarations, defined at the end.
int yylex();
int yyerror();
command *init_command();

%}

%union {
    char *str;
}

%token END
%token PIPE
%token IN
%token OUT
%token APP
%token<str> ARG
%token<str> EXIT
%token<str> QUOTE_ARG
%parse-param { struct parsed *line }

%%

line:
    EXIT
    {
        YYABORT;
    }
    |
    END
    {
        YYACCEPT;
    }
    |
    commands END
    {
        YYACCEPT;
    }
    |
    error END
    {
        line->error = 1;
        yyerrok;
        YYACCEPT;
    };

commands:
    command
    |
    command PIPE commands;

command:
    basecommand arglist redirects;

redirects:
    |
    IN ARG redirects
    {
        line->curr->inredir = $2;
    }
    |
    OUT ARG redirects
    {
        line->curr->outredir = $2;
    }
    |
    APP ARG redirects
    {
        line->curr->outredir = $2;
        line->curr->outappend = 1;
    };

basecommand:
    ARG
    {
        token *tok = (token *) malloc(sizeof(token));
        tok->value = $1;
        tok->next = NULL;

        command *cmd = init_command();

        if (line->curr != NULL) {
            line->curr->next = cmd;
        }

        line->curr = cmd;

        if (line->first == NULL) {
            line->first = cmd;
        }

        line->curr->first_token = tok;
        line->curr->last_token = tok;
    };

arglist:
    |
    arglist ARG
    {
        token *tok = (token *) malloc(sizeof(token));
        tok->value = $2;
        tok->next = NULL;

        line->curr->last_token->next = tok;
        line->curr->last_token = tok;
    }
    |
    arglist QUOTE_ARG
    {
        int len = strlen($2) - 2;
        char *quoted = (char *) malloc((len + 1) * sizeof(char));

        // Copy over everything but the two quotes.
        int i;
        for (i = 0; i < len; i++) {
            quoted[i] = $2[i+1];
        }

        quoted[len] = '\0';

        token *tok = (token *) malloc(sizeof(token));
        tok->value = quoted;
        tok->next = NULL;

        line->curr->last_token->next = tok;
        line->curr->last_token = tok;

        free($2);
    }
    ;

%%

int yyerror(const char *str) {
    fprintf(stderr, "error: couldn't parse that command\n");
    return 0;
}

int yywrap() {
    return 1;
}

/**
 * @brief Initializes a command struct with default values.
 *
 * @return The initialized command struct.
 */
command *init_command() {
    command *cmd = (command *) malloc(sizeof(command));
    cmd->next = NULL;
    cmd->inredir = NULL;
    cmd->outredir = NULL;
    cmd->outappend = 0;

    return cmd;
}
