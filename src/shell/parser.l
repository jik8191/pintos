%{
#include <stdio.h>
#include "mysh.h"
#include "y.tab.h"
%}

%option noinput
%option nounput

whitesp     [\t ]
validchars  [^&:;<>`"'#~=%\|\(\)\$\?\[\]\\\t\r\n ]

%%

^exit$          return EXIT;
\|              return PIPE;
\<              return IN;
\>              return OUT;
\>\>            return APP;
\".*\"          yylval.str=strdup(yytext); return QUOTE_ARG;
{validchars}+   yylval.str=strdup(yytext); return ARG;
\\\n     
\n              return END;
{whitesp}+      /* Ignore Whitespace */

%%

void set_input(char *str) {
    yy_scan_string(str);
}

void clear() {
    yy_delete_buffer(YY_CURRENT_BUFFER);
}
