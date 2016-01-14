typedef struct token {
    char *value;
    struct token *next;
} token;

typedef struct command {
    int len;
    token *first_token;         // First token
    token *last_token;          // Last token
    struct command *next;       // Next command in sequence
    char *input_redirection;
    char *output_redirection;
} command;

typedef struct parsed {
    int error;
    command *frst;
    command *curr;
} parsed;

int read_input(char* buffer, unsigned long *numBytesRead, int maxBytesToRead);
