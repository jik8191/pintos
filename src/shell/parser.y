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
%token<str> COMMAND 
%token<str> ARG
%token END
%token<str> EXIT
%token<str> REDIRECT
%parse-param { struct command *cmnd_struct }
%%

commands: /* empty */
        | commands command
        ;

command: 
        command_value
        |
        special
        ;

command_value:
        COMMAND
        {
            printf("COMMAND:%s\n", $1);
            cmnd_struct->type = 'N'; // A normal command
            cmnd_struct->len = 1;  
            // Putting the token in the command struct
            token *token_value = (token *) malloc(sizeof(token));
            token_value->value = $1;
            token_value->next = NULL;
            cmnd_struct->first_token = token_value;
            cmnd_struct->last_token = token_value;
        }
        |
        ARG
        {
            printf("ARG:%s\n", $1);
            cmnd_struct->len++;
            // Putting the token in the command struct
            token *token_value = (token *) malloc(sizeof(token));
            token_value->value = $1;
            token_value->next = NULL;
            cmnd_struct->last_token->next = token_value;
            cmnd_struct->last_token = token_value;
        }
        ;

special:
        REDIRECT ARG
        {
            if (strcmp($1, ">") == 0) {
                cmnd_struct->input_redirection = $2;
                printf("INPUT_REDIRECT to %s\n", $2);
            }
            else {
                cmnd_struct->output_redirection = $2;
                printf("OUTPUT_REDIRECT to %s\n",$2);
            }
        }
        |
        END 
        {
            YYACCEPT;
        }
        |
        EXIT
        {
            token *token_value = (token *) malloc(sizeof(token));
            token_value->value = $1;
            token_value->next = NULL;
            cmnd_struct->first_token = token_value;
            YYABORT;
        }
        ;


%%

int yyerror(const char *str)
{
    fprintf(stderr,"error: %s\n",str);
    return 0;
}
 
int yywrap()
{
    return 1;
} 

/*
int main()
{
    yyparse();
    return 0;
} 
*/
