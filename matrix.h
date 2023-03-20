#ifndef MATRIX_H
#define MATRIX_H

#include "bmp.h"

float *matrix_init_empty(int, int);
float *matrix_init(int, int, float);
float *matrix_init_parallel(int, int, float, int);

void matrix_out(float *, int, int, char *);

void matrix_step(float *, int, int, float, float);
void matrix_step_parallel(float **, float**, int, int, float, float, int);

#endif
