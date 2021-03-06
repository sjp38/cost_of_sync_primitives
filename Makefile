.PHONY: clean report

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
		$(HOME)/lazybox/gnuplot/plot.py --xtitle "Number of cores" \
		--ytitle "Operations per second"
	@echo "The report ('plot.pdf') is ready now"

clean:
	rm -f $(APP) $(OBJS)
