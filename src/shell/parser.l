%{
#include <stdio.h>
#include "mysh.h"
#include "y.tab.h"
%}

%%
^exit$                                      yylval.str = "exit"; return EXIT;
>|<                                         yylval.str=strdup(yytext); return REDIRECT;
^[A-Za-z][A-Za-z0-9_\-]*                    yylval.str=strdup(yytext); return COMMAND;
\".*\"                                      yylval.str=strdup(yytext); return ARG;
[^&:;<>`"'#~=%/\|\(\)\$\*\?\[\]\\\n\t\r ]*  yylval.str=strdup(yytext); return ARG;
\n                                          return END;
[ \t]+                                      /* Ignore whitespace */
%%
/* [A-Za-z0-9\.\-_~]*                          yylval.str=strdup(yytext); return ARG; */
