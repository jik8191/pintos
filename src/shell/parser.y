%{
#include <stdio.h>
#include <string.h>
#include "mysh.h"

// Function declarations, defined at the end.
int yylex();
void yyerror();
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
        token *token_value = (token *) malloc(sizeof(token));
        token_value->value = $1;
        token_value->next = NULL;

        command *cmd = init_command();

        if (line->curr != NULL) {
            line->curr->next = cmd;
        }

        line->curr = cmd;

        if (line->first == NULL) {
            line->first = cmd;
        }

        line->curr->first_token = token_value;
        line->curr->last_token = token_value;
    };

arglist:
    |
    arglist ARG
    {
        token *token_value = (token *) malloc(sizeof(token));
        token_value->value = $2;
        token_value->next = NULL;

        line->curr->last_token->next = token_value;
        line->curr->last_token = token_value;
    }
    |
    arglist QUOTE_ARG
    {
        token *token_value = (token *) malloc(sizeof(token));
        token_value->value = $2;
        token_value->value++;
        token_value->value[strlen($2) - 2] = 0;
        token_value->next = NULL;

        line->curr->last_token->next = token_value;
        line->curr->last_token = token_value;
    }
    ;

%%

void yyerror(const char *str) {
    fprintf(stderr, "error while parsing\n");
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
