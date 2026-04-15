CC      := cc
PREFIX  := $(HOME)/.local
DATADIR := $(PREFIX)/share/caravel
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

.PHONY: all clean install uninstall

all: $(TARGET)

$(TARGET): $(SRCS) $(HDRS)
	$(CC) $(CFLAGS) -o $@ $(SRCS)

clean:
	rm -f $(TARGET)

install: $(TARGET)
	install -d $(PREFIX)/bin
	install -m 755 $(TARGET) $(PREFIX)/bin/$(TARGET)
	install -d $(DATADIR)/include/caravel
	install -m 644 ../include/caravel.h $(DATADIR)/include/
	install -m 644 ../include/bpf.ld    $(DATADIR)/include/
	install -m 644 ../include/caravel/*.h $(DATADIR)/include/caravel/
	@echo "Installed $(TARGET) to $(PREFIX)/bin"
	@echo "Installed SDK headers to $(DATADIR)/include"

uninstall:
	rm -f $(PREFIX)/bin/$(TARGET)
	rm -rf $(DATADIR)
	@echo "Uninstalled $(TARGET) and SDK headers"
