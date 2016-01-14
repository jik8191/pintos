%{
#include <stdio.h>
#include <string.h>
#include "mysh.h"
int yylex(); /* Included to get rid of a warning */
void yyerror(); /* Included to get rid of a warning */
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
        line->curr->input_redirection = $2;
    }
    |
    OUT ARG redirects
    {
        line->curr->output_redirection = $2;
    }
    |
    APP ARG redirects
    {
        line->curr->output_redirection = $2;
        line->curr->output_append = 1;
    };

basecommand:
    ARG
    {
        token *token_value = (token *) malloc(sizeof(token));
        token_value->value = $1;
        token_value->next = NULL;

        command *cmd = (command *) malloc(sizeof(command));
        cmd->next = NULL;
        cmd->input_redirection = NULL;
        cmd->output_redirection = NULL;
        cmd->output_append = 0;

        if (line->curr != NULL) {
            line->curr->next = cmd;
        }

        line->curr = cmd;

        if (line->frst == NULL) {
            line->frst = cmd;
        }

        line->curr->len = 1;
        line->curr->first_token = token_value;
        line->curr->last_token = token_value;
    };

arglist:
    |
    arglist ARG
    {
        /* printf("ARG: %s\n", $2); */

        // Putting the token in the command struct
        token *token_value = (token *) malloc(sizeof(token));
        token_value->value = $2;
        token_value->next = NULL;

        line->curr->len++;
        line->curr->last_token->next = token_value;
        line->curr->last_token = token_value;
    }
    |
    arglist QUOTE_ARG
    {
        // TODO can't handle newlines in string
        token *token_value = (token *) malloc(sizeof(token));
        token_value->value = $2;
        token_value->value++;
        token_value->value[strlen($2) - 2] = 0; // 2 because of newline?
        token_value->next = NULL;

        line->curr->len++;
        line->curr->last_token->next = token_value;
        line->curr->last_token = token_value;
    }
    ;

%%

void yyerror(const char *str)
{
    fprintf(stderr, "error while parsing\n");
}

int yywrap()
{
    return 1;
}


