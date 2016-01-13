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
%parse-param { struct command *cmnd_struct }

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
    arglist redirects;

redirects:
    |
    IN ARG
    {
        cmnd_struct->input_redirection = $2;
        printf("INPUT_REDIRECT to %s\n", $2);
    }
    |
    OUT ARG
    {
        cmnd_struct->output_redirection = $2;
        printf("OUTPUT_REDIRECT to %s\n",$2);
    }
    |
    IN ARG OUT ARG
    {
        cmnd_struct->input_redirection = $2;
        cmnd_struct->output_redirection = $2;
        printf("INPUT_REDIRECT to %s\n", $2);
        printf("OUTPUT_REDIRECT to %s\n",$2);
    };

arglist:
    |
    arglist ARG
    {
        printf("ARG: %s\n", $2);

        // Putting the token in the command struct
        token *token_value = (token *) malloc(sizeof(token));
        token_value->value = $2;
        token_value->next = NULL;

        if (cmnd_struct->first_token == NULL) {
            cmnd_struct->len = 1;
            cmnd_struct->first_token = token_value;
            cmnd_struct->last_token = token_value;
        } else {
            cmnd_struct->len++;
            cmnd_struct->last_token->next = token_value;
            cmnd_struct->last_token = token_value;
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

