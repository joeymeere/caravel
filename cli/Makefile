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
	@mkdir -p $(PREFIX)/bin $(DATADIR)/include/caravel
	cp -f $(TARGET) $(PREFIX)/bin/$(TARGET)
	chmod 755 $(PREFIX)/bin/$(TARGET)
	cp -f ../include/caravel.h $(DATADIR)/include/
	cp -f ../include/bpf.ld    $(DATADIR)/include/
	cp -f ../include/rt.s      $(DATADIR)/include/
	cp -f ../include/caravel/*.h $(DATADIR)/include/caravel/
	@echo "Installed $(TARGET) to $(PREFIX)/bin"
	@echo "Installed SDK headers to $(DATADIR)/include"

uninstall:
	rm -f $(PREFIX)/bin/$(TARGET)
	rm -rf $(DATADIR)
	@echo "Uninstalled $(TARGET) and SDK headers"
