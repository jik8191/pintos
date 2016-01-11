#include <stdlib.h>
#include <stdio.h>
#include "y.tab.h"
#include <unistd.h>
#include <string.h>

extern int yyparse(void);
int loop();

int main() {
    int exit;

    exit = 0;

    while(exit == 0) {
        exit = loop();
    }
}

int loop() {
    char *username;
    char *directory;
    char prompt[500] = "\0";
    int exit;

    exit = 0;

    // Username of the session
    username = getlogin();
    // Current directory
    directory = getcwd(NULL, 100); // Arbitrary buff size. Need to free this?
    // Getting the current prompt
    strcat(strcat(strcat(strcat(prompt, username), ":"), directory), ">");
    // Displaying the prompt
    printf("%s ", prompt);

    exit = yyparse();
    /*prompt[0] = '\0';*/
   
    return exit;
}
