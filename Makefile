# Target library
lib := libsimlink.a
objs := simlink.o

# Define compiler to user
CC := g++

# Define the flags to use for the compiler
CFLAGS	:= -Wall 
CFLAGS += -Wextra -Werror
CFLAGS  += -pipe


CFLAGS  += -MMD

# Include dependencies
deps := $(patsubst %.o,%.d,$(objs))

all: lib

lib: $(lib)

$(lib): $(objs)
	ar rcs $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

# Clean generated files
clean:
	rm -rf $(objs) $(deps) $(lib)
