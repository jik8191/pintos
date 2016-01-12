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
        command_name
        |
        arg_value
        |
        io
        |
        end_line
        |
        exit_shell
        ;

command_name:
        COMMAND
        {
            printf("ARG:%s\n", $1);
            cmnd_struct->type = 'N';
            token *token_value = (token *) malloc(sizeof(token));
            /*token token_value;*/
            token_value->value = $1;
            token_value->next = NULL;
            if (cmnd_struct->first_token == NULL) {
                cmnd_struct->first_token = token_value;
                cmnd_struct->last_token = token_value;
            }
            else {
                cmnd_struct->last_token->next = token_value;
                cmnd_struct->last_token = token_value;
            }
            cmnd_struct->len++;
        }
        ;

arg_value:
        ARG
        {
            printf("ARG:%s\n", $1);
        }
        ;
io:
        REDIRECT
        {
            printf("HERE\n");
            if (strchr($1, '>') != NULL) {
                printf("INPUT_REDIRECT:%s\n", $1);
            }
            else {
                printf("OUTPUT_REDIRECT:%s\n", $1);
            }
        }
        ;
end_line:
        END
        {
            printf("DONE\n");
            YYACCEPT;
        }
        ;
exit_shell:
    EXIT
    {
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
