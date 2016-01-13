%{
#include <stdio.h>
#include <string.h>
#include "mysh.h"
int yylex(); /* Included to get rid of a warning */
int yyerror(); /* Included to get rid of a warning */
%}

%union {
    char *str;
}

%token END
%token PIPE
%token IN
%token OUT
%token<str> ARG
%token<str> EXIT
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
    };

commands:
    command
    |
    command PIPE commands;

command:
    arglist redirects
    {
        line->new = 1;
    };

redirects:
    |
    IN ARG
    {
        line->curr->input_redirection = $2;
        printf("INPUT_REDIRECT to %s\n", $2);
    }
    |
    OUT ARG
    {
        line->curr->output_redirection = $2;
        printf("OUTPUT_REDIRECT to %s\n",$2);
    }
    |
    IN ARG OUT ARG
    {
        line->curr->input_redirection = $2;
        line->curr->output_redirection = $2;
        printf("INPUT_REDIRECT to %s\n", $2);
        printf("OUTPUT_REDIRECT to %s\n",$2);
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

        if (line->new) {
            line->new = 0;

            command *cmd = (command *) malloc(sizeof(command));
            cmd->next = NULL;

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
        } else {
            line->curr->len++;
            line->curr->last_token->next = token_value;
            line->curr->last_token = token_value;
        }
    };

%%

int yyerror(const char *str)
{
    fprintf(stderr,"error: %s\n", str);
    return 1;
}

int yywrap()
{
    return 1;
}

