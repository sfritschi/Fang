CC=gcc
CFLAGS=-Wall -Wextra -Wpedantic -g -std=gnu11

.PHONY: all, clean
TARGET=fang
all=$(TARGET)

LINK_GL=-lGL -lglut -lGLEW -lm
LINK_FT=-lfreetype -lpng -lbz2 -lz

SRCDIR=src
OBJDIR=bin

SRC=$(wildcard $(SRCDIR)/*.c)
OBJ=$(patsubst $(SRCDIR)/%.c, $(OBJDIR)/%.o, $(SRC))

$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $^ -o $@ -Iinclude/ -I/usr/local/include/freetype2 $(LINK_GL) $(LINK_FT)
	
$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LINK_GL) $(LINK_FT)

$(OBJDIR):
	mkdir -p $@

clean:
	$(RM) $(TARGET)
	$(RM) -r $(OBJDIR)
