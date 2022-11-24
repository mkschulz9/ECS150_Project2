# Target library
lib := libfs.a
CC := gcc
CFLAGS := -Wall -Wextra -Werror
objs := disk.o fs.o

all: $(lib)

deps := $(patsubst %.o,%.d,$(objs))
-include $(deps)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

libfs.a: $(objs)
	ar -rcs $@ $(objs)

clean:
	rm -f $(lib) $(objs) $(deps)
