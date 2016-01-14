CFLAGS = -Wall -g

SRC = mysh.c y.tab.c lex.yy.c

LEXNAME = parser.l

YACCNAME = parser.y

all: clean mysh

mysh: $(SRC)
	$(CC) $(CFLAGS) -o $@ $(SRC) $(LDFLAGS) -lreadline 

y.tab.c: $(YACCNAME)
	yacc -d $(YACCNAME)

lex.yy.c: $(LEXNAME)
	lex $(LEXNAME)

clean:
	rm -f *.o *~ mysh
	rm -f lex.yy.c
	rm -f y.tab.c
	rm -f y.tab.h

.PHONY: all clean

