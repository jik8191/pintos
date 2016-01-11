/* CS124 Winter 2016 Project 1
 * Implementation of a simple command shell
 */
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>

#define MAX_BUFSIZE 1024

/* Command struct to hold a specific command
 * details needed: input/output/error redirection, 
 * number of tokens, array of command line arguments 
 */

char* mysh_read_line(){
  char* buf=malloc(sizeof(char) * MAX_BUFSIZE);

  if (buf == NULL){  // null check
    fprintf(stderr, "mysh: memory allocation error\n");
    exit(EXIT_FAILURE);
  }
  
  buf=fgets(buf, MAX_BUFSIZE, stdin);
  return buf;
}

/* Tokenize user-specified command string
 */

void mysh_loop(){
  char* line;
  int status = 1;
  do {
    printf("> ");
    line=mysh_read_line();
    system(line);
    free(line);
  } while (status);
    
}

int main() {

  // Run command loop
  mysh_loop();
  
  return EXIT_SUCCESS;  // successful program execution
}

