typedef struct token {
    char *value;
    struct token *next;
} token;

typedef struct command {
    char type;     // N (Normal), P (Pipe)
    token *first_token; // First token
    token *last_token; // Last token
    int len;
    struct command *prev;     // Previous command in sequence
    struct command *next;     // Next command in sequence
    char *input_redirection;
    char *output_redirection;
} command;
