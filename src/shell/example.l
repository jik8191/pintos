%{
#include <stdio.h>
#include "y.tab.h"
%}

%%
exit                                       return EXIT;
\>[ \t]*[a-zA-Z0-9\.\-_]+                            yylval.str=strdup(yytext); return REDIRECT;
\<[ \t]*[^ \t\n]+                            yylval.str=strdup(yytext); return REDIRECT;
^[A-Za-z][A-Za-z0-9]*                      yylval.str=strdup(yytext); return COMMAND;
[A-Za-z][A-Za-z0-9]*                       yylval.str=strdup(yytext); return ARG;
[A-Za-z][A-Za-z0-9]+\.[A-Za-z][A-Za-z0-9]+ yylval.str=strdup(yytext); return ARG;
\".*\"                                     yylval.str=strdup(yytext); return ARG;
\n                                         return END;
[ \t]+                                     /* Ignore whitespace */
%%
