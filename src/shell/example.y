%{
#include <stdio.h>
#include <string.h>
int yylex(); /* Included to get rid of a warning */
int yyerror(); /* Included to get rid of a warning */
/*#include "parser.h"*/
%}

%union {
    char *str;
}
%token<str> COMMAND 
%token<str> ARG
%token END
%token<str> EXIT
%token<str> REDIRECT
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
            printf("COMMAND:%s\n", $1);
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
