CC=gcc
CFLAGS=-Wall -Wextra -Wpedantic -g -std=c11 -fopenmp

.PHONY: all, clean
TARGET=fang
all=$(TARGET)

SRCDIR=src
OBJDIR=bin

SRC=$(wildcard $(SRCDIR)/*.c)
OBJ=$(patsubst $(SRCDIR)/%.c, $(OBJDIR)/%.o, $(SRC))

$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $^ -o $@ -Iinclude
	
$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ -Iinclude/

$(OBJDIR):
	mkdir -p $@

clean:
	$(RM) $(TARGET)
	$(RM) -r $(OBJDIR)
