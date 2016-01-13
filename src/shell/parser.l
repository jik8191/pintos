%{
#include <stdio.h>
#include "mysh.h"
#include "y.tab.h"
%}

%option noinput
%option nounput

whitesp     [\t ]
validchars  [^&:;<>`"'#~=%/\|\(\)\$\*\?\[\]\\\t\r\n ]

%%

^exit$          return EXIT;
\|              return PIPE;
\<              return IN;
\>              return OUT;
\".*\"          yylval.str=strdup(yytext); return ARG;
{validchars}+   yylval.str=strdup(yytext); return ARG;
\n              return END;
{whitesp}+      /* Ignore Whitespace */

%%
