#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <omp.h>
#include "matrix.h"
#include <math.h>

#define WRITE_BUFF_MULT 8
#define CONV_BUFF_SIZE 64

float matrix_sum_neighbors(float *, int, int, int, int, float);

// Takes row/col sizes, allocates an EMPTY matrix accordingly.
// Returns matrix ptr
float *matrix_init_empty(int cols, int rows)
{
    // 1d array which will be indexed like a 2d array
    float *matrix_ptr = (float *)malloc((cols * rows) * sizeof(float));

    return matrix_ptr;
}

// Takes row/col sizes and the base temp, and allocates a matrix accordingly.
// Returns matrix ptr
float *matrix_init(int cols, int rows, float base)
{
    // each column is a pointer to an array (pointer)
    float *matrix_ptr = (float *)malloc((cols * rows) * sizeof(float));

    for (int i = 0; i < rows; i++)
    {
        for (int j = 0; j < cols; j++)
        {
            matrix_ptr[j + (i * cols)] = base; // base is default temperature
        }
    }

    return matrix_ptr;
}

// out of date, slow, not needed
// Takes row/col sizes and the base temp, and allocates a matrix accordingly.
// This version runs in parallel with given number of threads.
// Returns matrix ptr
/*float *matrix_init_parallel(int cols, int rows, float base, int numThreads)
{
    // each column is a pointer to an array (pointer)
    float *matrix_ptr = (float *)malloc((cols * rows) * sizeof(float));

    #pragma omp parallel for num_threads(numThreads) schedule(dynamic) collapse(2)
    for (int i = 0; i < rows; i++)
    {
        for (int j = 0; j < cols; j++)
        {
            matrix_ptr[j + (i * cols)] = base; // base is default temperature
        }
    }

    return matrix_ptr;
}*/

// Takes 2d array matrix, its dimensions, and an output file name.
// Prints a buffer containing every cell of the matrix to the given file.
// Done this way to avoid literally 25 million fprintf's, because thats slow.
void matrix_out(float *matrix, int cols, int rows, char *outFileName)
{
    int writeBuffSize = (cols * rows) * (sizeof(char) * WRITE_BUFF_MULT);
    char *write_buffer = (char *)malloc(writeBuffSize);
    write_buffer[0] = '\0';
    char convert_buffer[CONV_BUFF_SIZE];
    int strSize = 0;

    FILE *outFile;
    outFile = fopen(outFileName, "w"); // w for write only

    if (!outFile) // !outFile = file could not be opened, is NULL
    {
        printf("ERROR: Output file could not be opened.\n");
        return;
    }

    for (int i = 0; i < rows; i++)
    {
        for (int j = 0; j < cols; j++)
        {
            // Converts float to string and stores it in a 64 byte buffer
            snprintf(convert_buffer, sizeof(convert_buffer), "%.1f,", matrix[j + (i * cols)]);
            // Concatonates to main buffer by null terminator
            int numLen = strlen(convert_buffer);
            for (int k = 0; k < numLen; k++)
            {
                write_buffer[k + strSize] = convert_buffer[k];
            }
            write_buffer[numLen + strSize] = '\0';
            strSize += numLen;
        }

        // Checks if buffer is 80% full, and writes/empties if so
        if (strSize > (writeBuffSize * 0.8))
        {
            fprintf(outFile, "%s", write_buffer);
            write_buffer[0] = '\0';
            strSize = 0;
        }
        write_buffer[strSize] = '\n';
        strSize++;
    }
    // ONE write to file, because we are going for speed here
    fprintf(outFile, "%s", write_buffer);

    free(write_buffer);
}

// out of date, useless
// Serial code for matrix time step.
// No return value, calculates matrix one timestep ahead
// and reuses the same matrix pointer.
/*void matrix_step(float *matrix, int cols, int rows, float k, float base)
{
    float *tmpMatrix = matrix_init_empty(cols, rows);

    for (int i = 0; i < rows; i++)
    {
        for (int j = 0; j < cols; j++)
        {
            float sum = matrix_sum_neighbors(matrix, j, i, cols, rows, base);
            float newTemp = (matrix[j + (i * cols)] + (k * sum) / 8.0) / 2.0;
            tmpMatrix[j + (i * cols)] = newTemp;
        }
    }

    for (int i = 0; i < rows; i++)
    {
        for (int j = 0; j < cols; j++)
        {
            matrix[j + (i * cols)] = tmpMatrix[j + (i * cols)];
        }
    }

    free(tmpMatrix);
}*/

// Takes ADDRESS of matrix (this is necessary for efficient swapping and avoiding memory leaks)
// as well as dimensions of matrix, transfer rate, temperature, and thread count.
// Performs one time step on the array using given temp/rate/dimensions.
void matrix_step_parallel(float **matrix, float **tmpMatrix, int cols, int rows, float k, float base, int numThreads)
{
    float *newMatrix = *tmpMatrix;
    float *curMatrix = *matrix; // derefence address of matrix to usable form

    // Each thread is given one "cell" (x/y coordinate) at a time
    // and calculates the new temperature based on neighbors.
    // This new value is stored in a temporary matrix, and doing this
    // requires no writes to the original matrix. This means
    // there are no race conditions here, as no thread will write
    // where any other threat wants to write.
    #pragma omp parallel for num_threads(numThreads) schedule(static) collapse(2)
    for (int i = 0; i < rows; i++)
    {
        for (int j = 0; j < cols; j++)
        {
            newMatrix[j + (i * cols)] = 
                (curMatrix[j + (i * cols)] + (k * matrix_sum_neighbors(curMatrix, j, i, cols, rows, base)) / 8.0) / 2.0;
        }
    }

    float *tmp = *matrix;
    *matrix = *tmpMatrix; // put tmpMatrix at the address of main matrix
    *tmpMatrix = tmp;
}

// Takes a matrix (float *), coordinates, dimensions, and a default temperature.
// Does not need parallelized as the smallest reasonable chunks each thread can do
// are to calculate each index's neighbor sum.
// Returns the sum of the coordinate's neighbors, defaulting out-of-bounds indices
// to the default temperature.
float matrix_sum_neighbors(float *matrix, int x, int y, int cols, int rows, float base)
{
    float sum = 0;

    // sort of ugly, but makes the common case quite fast.
    // very little comparisons/math overhead, just raw summing.
    if (x > 0 && x < cols - 1 && y > 0 && y < rows - 1)
    {
        sum += matrix[x - 1 + ((y - 1) * cols)];
        sum += matrix[x     + ((y - 1) * cols)];
        sum += matrix[x - 1 + ( y      * cols)];

        sum += matrix[x + 1 + ((y + 1) * cols)];
        sum += matrix[x     + ((y + 1) * cols)];
        sum += matrix[x + 1 + ( y      * cols)];

        sum += matrix[x - 1 + ((y + 1) * cols)];
        sum += matrix[x + 1 + ((y - 1) * cols)];

        return sum;
    }

    // if above comparison failed, we are on some sort of edge.
    // this loop will only be encountered a number of times equal to the
    // perimeter of the matrix.
    // basically just goes over the neighbors and picks out the bad ones.
    for (int i = -1; i <= 1; i++)
    {
        for (int j = -1; j <= 1; j++)
        {
            if (i == 0 && j == 0)
                continue;

            int cur_x = x + j;
            int cur_y = y + i;

            if (cur_x < 0 || cur_x >= cols || cur_y < 0 || cur_y >= rows)
            {
                sum += base;
                continue;
            }

            sum += matrix[cur_x + (cur_y * cols)];
        }
    }

    return sum;
}