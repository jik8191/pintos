CFLAGS = -Wall -g

OBJS = mysh.o

SRC = mysh.c y.tab.c lex.yy.c

LEXNAME = example.l

YACCNAME = example.y

all: mysh

lexyacc: compile_parser

mysh: $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

testing: $(SRC)
	cc -o testing $(SRC)

y.tab.c: $(YACCNAME)
	yacc -d $(YACCNAME)

lex.yy.c: $(LEXNAME)
	lex $(LEXNAME)

compile_parser:
	lex $(LEXNAME)
	yacc -d $(YACCNAME)
	cc lex.yy.c y.tab.c -o example

clean: 
	rm -f *.o *~ mysh mylex example testing
	rm lex.yy.c
	rm y.tab.c
	rm y.tab.h

.PHONY: all clean

