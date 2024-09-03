CC = gcc
CFLAGS = -O3 -flto -Wall -DN=$(N) -DTYPE=uint$(BITS)_t -DCYCLES=$(CYCLES) -DHAMILTONIAN=$(HAMILTONIAN) -DN_THREADS=$(N_THREADS)
TARGET = path-counter
SRCS = src/*.c

.PHONY: all default clean check-params

all: default

check-params:
	@if [ -z "$(N)" ]; then echo "N is not defined. Please define N as an integer value between 4 and 30, inclusive"; exit 1; fi
	@if [ -z "$(BITS)" ]; then echo "BITS is not defined. Please define BITS as an integer value 8, 16, 32, or 64"; exit 1; fi
	@if [ -z "$(CYCLES)" ]; then echo "CYCLES is not defined. Please define CYCLES as 0 or 1"; exit 1; fi
	@if [ -z "$(HAMILTONIAN)" ]; then echo "HAMILTONIAN is not defined. Please define HAMILTONIAN as 0 or 1"; exit 1; fi
	@if [ -z "$(N_THREADS)" ]; then echo "N_THREADS is not defined. Please define N_THREADS as an integer value"; exit 1; fi

default: check-params
	$(CC) -O3 -flto -Wall $(CFLAGS) $(SRCS) -o $(TARGET)

clean:
	rm -f $(TARGET)
