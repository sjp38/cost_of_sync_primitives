.PHONY: clean help

APP	:= cost_of_sync_primitives
OBJS	:= cost_of_sync_primitives.o

CFLAGS	:= -g -O3 -Wall -Werror -std=gnu99
LIBS	:= -lpthread

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(APP): $(OBJS)
	$(CC) -o $@ $^ $(LIBS)

clean:
	rm -f $(APP) $(OBJS)
