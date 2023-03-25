CC=gcc
CFLAGS=-g -Wall -lpthread -pthread
EXE=coordinator palin

all:$(EXE)

%:%.o
	$(CC) $(CFLAGS) -o $@ $<

%.o:%.c
	$(CC) $(FLAGS) -c $<

clean:
	rm -f $(EXE)
