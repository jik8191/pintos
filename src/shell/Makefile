CFLAGS = -Wall -g

SRC = mysh.c y.tab.c lex.yy.c
LEXSRC = parser.l
YACCSRC = parser.y

all: clean mysh

mysh: $(SRC)
	$(CC) $(CFLAGS) -o $@ $(SRC) $(LDFLAGS) -lreadline

y.tab.c: $(YACCSRC)
	yacc -d $(YACCSRC)

lex.yy.c: $(LEXSRC)
	lex $(LEXSRC)

clean:
	rm -f *.o lex.yy.c y.tab.* mysh *~

.PHONY: all clean

