CC=gcc
CFLAGS=-Wall -Wextra -Wpedantic -g -std=c11 -fopenmp

.PHONY: all, clean
TARGET=fang
all=${TARGET}

${TARGET}: src/${TARGET}.c
	${CC} ${CFLAGS} -o $@ $^ -Iinclude/

clean:
	${RM} ${TARGET}
