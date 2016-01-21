#include "mysh.h"
#include "y.tab.h"

#define MAX_PROMPT_SIZE 500
#define MAX_TOK_BUFSIZE 64

/**
 * @brief The main shell process.
 *
 * This just loops until we receive an exitcode.
 */
int main() {
    int exitcode = 0;

    // Add readline history.
    using_history();

    while(exitcode == 0) {
        exitcode = loop();
    }

    return 0;
}

/**
 * @brief A single iteration of the shell loop.
 *
 * Gets user input, parses it, and then runs the input.
 */
int loop() {
    char prompt[MAX_PROMPT_SIZE] = "\0";

    // This is used to check the return values of function calls.
    int exitcode = 0;

    // Initialize the parser parameter
    parsed *line = (parsed *) malloc(sizeof(parsed));
    line->first = NULL;
    line->curr = NULL;
    line->error = 0;

    // Username of the session
    /* username = getlogin(); // This is apparently unsafe. */
    char *username = getpwuid(getuid())->pw_name;

    // Current directory
    char *dir_curr = getcwd(NULL, 0); // Need to free this?

    // Getting the current prompt
    strcat(strcat(strcat(strcat(prompt, username), ":"), dir_curr), "> ");

    free(dir_curr); // no longer needed

    // Getting user input via readline
    static char *user_input = (char *) NULL;

    user_input = readline(prompt);

    if(user_input && *user_input) {
        add_history(user_input);
    }

    // Add a newline to the end of the string b/c the parser expects it.
    int input_len = strlen(user_input);
    user_input = realloc(user_input, input_len + 2 * sizeof(char));
    user_input[input_len + 1] = '\0';
    user_input[input_len] = '\n';

    // Set the input for the lexer & parser
    set_input(user_input);

    // Parse and clear the lexer buffer
    exitcode = yyparse(line);
    clear();

    free(user_input); // no longer needed

    if (exitcode != 0) {
        free_line(line);
        return 1; // User asked to exit
    } else if (line->error != 0) {
        free_line(line);
        return 0; // Parse error, so skip this loop
    } else if (line->first == NULL) {
        free_line(line);
        return 0;
    }

    // These will hold the file descriptors of the pipe before and after a
    // given command. -1 indicates that they have not been set yet.
    int prevfds[2] = {-1};
    int currfds[2] = {-1};

    command *cmd = line->first;

    // This only loops through only commands that pipe into another command.
    for (; cmd->next != NULL; cmd = cmd->next) {
        // Create the pipe
        if (pipe(currfds) == -1) {
            printf("error: failed to open pipe\n");

            free_line(line);
            return 0;
        }

        // Exec the actual command
        exitcode = exec_cmd(cmd, prevfds, currfds);

        *prevfds = *currfds;
    }

    // Exec the last command in the chain of pipes, or the single command if
    // there is only one.
    exitcode = exec_cmd(cmd, prevfds, NULL);

    free_line(line);
    return exitcode;
}

/**
 * @brief Executes a command specified by a command struct.
 *
 * `cd`s or `chdir`s are handled differently.
 */
int exec_cmd(command* cmd, int *prevfds, int *currfds) {

    if (strcmp("cd", cmd->first_token->value) == 0 ||
        strcmp("chdir", cmd->first_token->value) == 0) {

        // CD to the user's home directory by default or with ~
        if (cmd->first_token->next == NULL ||
            !strcmp("~", cmd->first_token->next->value)){

            // Home directory of the user
            char *dir_home = getenv("HOME");

            chdir(dir_home);
        } else {
            chdir(cmd->first_token->next->value);
        }

    } else if (strcmp("history", cmd->first_token->value) == 0){
        HIST_ENTRY **curr_hist = history_list();
        HIST_ENTRY *temp_hist;
        int i;
        for (i = 0; curr_hist[i] != NULL; i++) {
            temp_hist = curr_hist[i];
            printf("%d: %s\n", i, temp_hist->line);
        }
    } else {

        pid_t pid;

        pid = fork();

        if (pid == -1) {
            printf("error: failed to fork process\n");
            return 0;
        }

        if (pid == 0) {
            char **tokens = tokenize(cmd);

            // If we are given the file descriptors of the previous process in
            // the pipe chain, then use that pipe as our STDIN.
            if (prevfds != NULL && prevfds[0] != -1) {
                close(prevfds[1]);
                dup2(prevfds[0], STDIN_FILENO);
                close(prevfds[0]);
            }

            // If we have a pipe set up for the next process in the pipe chain,
            // then use the pipe sa our STDOUT.
            if (currfds != NULL && currfds[0] != -1) {
                close(currfds[0]);
                dup2(currfds[1], STDOUT_FILENO);
                close(currfds[1]);
            }

            // If we have an input redirect, we set the specified file to the
            // STDIN. This will overwrite any piped input, which is consistent
            // with bash's behavior.
            if (cmd->inredir != NULL) {
                char *fname = cmd->inredir;
                int in_fd = open(fname, O_RDONLY);

                if (in_fd < 0) {
                    printf("error: could not read from file: %s\n", fname);
                    free(tokens);
                    exit(EXIT_FAILURE);
                }

                dup2(in_fd, STDIN_FILENO);
                close(in_fd);
            }

            // If we have an output redirect, we set the specified file to the
            // STDOUT. This means that nothing will be piped to the next
            // function if any, which is consistent with bash's behavior.
            if (cmd->outredir != NULL) {
                char *fname = cmd->outredir;

                // Specify the options to the write file. Check if appending.
                int options = O_WRONLY | O_CREAT;
                if (cmd->outappend) {
                    options = options | O_APPEND;
                } else {
                    options = options | O_TRUNC;
                }

                // Set the file permissions.
                int permissions = S_IROTH   // Read for anyone
                    | S_IRUSR | S_IWUSR     // R/W for owner
                    | S_IRGRP | S_IWGRP;    // R/W for group

                int out_fd = open(fname, options, permissions);

                if (out_fd < 0) {
                    printf("error: could not write to file: %s\n", fname);
                    free(tokens);
                    exit(EXIT_FAILURE);
                }

                dup2(out_fd, STDOUT_FILENO);
                close(out_fd);
            }

            // If we have a redirect of the form " n> file", we want to set
            // the file descriptor at n to ouput to the file.
            if (cmd->fdredir != NULL) {
                char *fname = cmd->fdredir;

                // Specify the options to the write file. Check if appending.
                int options = O_WRONLY | O_CREAT;
                if (cmd->outappend) {
                    options = options | O_APPEND;
                } else {
                    options = options | O_TRUNC;
                }

                // Set the file permissions.
                int permissions = S_IROTH   // Read for anyone
                    | S_IRUSR | S_IWUSR     // R/W for owner
                    | S_IRGRP | S_IWGRP;    // R/W for group

                int out_fd = open(fname, options, permissions);

                if (out_fd < 0) {
                    printf("error: could not write to file: %s\n", fname);
                    free(tokens);
                    exit(EXIT_FAILURE);
                }

                dup2(out_fd, cmd->fdout);
                close(out_fd);
            }

            execvp(tokens[0], tokens);

            // If the above function returned, then the command doesn't exist.
            printf("error: that command could not be found\n");
            free(tokens);
            exit(errno); // we exit here because we need the child to quit
        } else {
            // If we are the parent, we can close the write end of the next
            // pipe in the chain, since the next child will only need the read
            // end.
            if (currfds != NULL && currfds[1] != -1) {
                close(currfds[1]);
            }

            wait(NULL);
        }
    }

    return 0;
}

/**
 * @brief Takes a linked list of tokens and returns an array.
 *
 * A token is an argument to a function and they are attached in a linked list
 * format from the parser. We just turn this into an array.
 *
 * @param cmd The command that is being run.
 *
 * @return An array of pointers to the argument strings.
 */
char **tokenize(command *cmd){
    // Size of the string array
    int size = MAX_TOK_BUFSIZE * sizeof(char *);

    char **tokens = malloc(size); // array of tokens
    if (!tokens){
        fprintf(stderr, "error: memory allocation error\n");
        exit(EXIT_FAILURE);
    }

    token *tok_temp = cmd->first_token;

    int index = 0;
    while (tok_temp != NULL){
        tokens[index] = tok_temp->value;
        tok_temp = tok_temp->next;

        index += 1;

        // If we have too many tokens, reallocate the array to a larger one.
        if (index >= size) {
            size = size * 2;
            tokens = realloc(tokens, size);
        }
    }

    tokens[index] = NULL;

    return tokens;
}

/**
 * @brief Frees all the memory associated with a user's parsed input.
 *
 * The parsed line has a series of commands, each of which has a series of
 * tokens (arguments), each of which have a value that needs to be freed.
 *
 * @param line The parsed user input.
 */
void free_line(parsed *line) {
    // We always keep track of the current and next since this is a linked list
    // and we free current before we can find the next.
    command *ccmd, *ncmd;
    token *ctok, *ntok;

    for (ccmd = line->first; ccmd != NULL; ccmd = ncmd) {
        ncmd = ccmd->next;

        for (ctok = ccmd->first_token; ctok != NULL; ctok = ntok) {
            ntok = ctok->next;
            free(ctok->value);
            free(ctok);
        }

        if (ccmd->inredir != NULL) {
            free(ccmd->inredir);
        }

        if (ccmd->outredir != NULL) {
            free(ccmd->outredir);
        }

        if (ccmd->fdredir != NULL) {
            free(ccmd->fdredir);
        }

        free(ccmd);
    }

    free(line);
}

