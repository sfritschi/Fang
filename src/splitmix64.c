#include "splitmix64.h"

// State (arbitrary) -> reproducibility
static uint64_t x = 327;

void set_seed(uint64_t seed) {
    x = seed;
}

uint64_t next() {
    uint64_t z = (x += 0x9e3779b97f4a7c15);
	z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9;
	z = (z ^ (z >> 27)) * 0x94d049bb133111eb;
	return z ^ (z >> 31);
}
