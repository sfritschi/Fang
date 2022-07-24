#pragma once
#ifndef SPLITMIX64_H
#define SPLITMIX64_H

#include <stdint.h>

#define SM64_RAND_MAX 0xFFFFFFFFFFFFFFFFLU

void set_seed(uint64_t);

uint64_t next();

#endif /* SPLITMIX64_H */
