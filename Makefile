.PHONY: clean help report

APP	:= cost_of_sync_primitives
OBJS	:= cost_of_sync_primitives.o

CFLAGS	:= -g -O3 -Wall -Werror -std=gnu99
LIBS	:= -lpthread

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(APP): $(OBJS)
	$(CC) -o $@ $^ $(LIBS)

report: $(APP)
	./$(APP) | ./_to_gnuplot_dat.py | \
		$(HOME)/lazybox/gnuplot/plot_stdin.sh scatter \
		"Number of cores" "Operations per second"
	evince plot.pdf

clean:
	rm -f $(APP) $(OBJS)
