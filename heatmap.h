#ifndef HEATMAP_H
#define HEATMAP_H

#define BPP 3
                                // best results:
#define B_DEFAULT 10            // 10
#define G_DEFAULT 127           // 127
#define G_DEFAULT_BRIGHT 128    // 128
#define R_DEFAULT 10            // 10
                                //
#define RED_SHIFT 3             // 3
#define BLUE_SHIFT -5           // -4
#define GREEN_SHIFT 2           // 2

typedef struct Color
{
    unsigned char b;
    unsigned char g;
    unsigned char r;
} Color;

typedef struct Matrix
{
    float *matrix;
    int cols;
    int rows;
    float baseVal;
} Matrix;

unsigned char *heatmap_gen(float *, int, int, int, int, float, float);

#endif