CC      := cc
CFLAGS  := -O2 -Wall -Wextra -std=c11
TARGET  := caravel

SRCS := main.c      \
        config.c    \
        scaffold.c  \
        cmd_init.c  \
        cmd_build.c \
        cmd_test.c  \
        cmd_deploy.c\
        cmd_clean.c \
        cmd_idl.c

HDRS := cli.h       \
        config.h    \
        scaffold.h  \
        templates/entrypoint.h \
        templates/makefile.h   \
        templates/test.h

.PHONY: all clean install

all: $(TARGET)

$(TARGET): $(SRCS) $(HDRS)
	$(CC) $(CFLAGS) -o $@ $(SRCS)

clean:
	rm -f $(TARGET)

install: $(TARGET)
	cp $(TARGET) /usr/local/bin/$(TARGET)
	@echo "Installed to /usr/local/bin/$(TARGET)"
