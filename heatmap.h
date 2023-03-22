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

typedef struct Matrix Matrix;

typedef struct Matrix
{
    void *matrix;
    int cols;
    int rows;
    long double baseVal; // base value of matrix, the "room temperature"
    long double range;   // the deviance value that will result in a 0.0 or 1.0 lerp
    long double (*get_relative_val)(Matrix *, const int); // gets relative value between two vars, func ptr

} Matrix;

typedef struct Map
{
    unsigned char *map;
    int imgW;
    int imgH;
    int bpp;
    Color color_low;
    Color color_norm;
    Color color_high;
} Map;

unsigned char *generate_map_float(float *, int, int, int, int, float, float, unsigned char *);
unsigned char *generate_map_int(int *, int, int, int, int, int, int, unsigned char *);
unsigned char *generate_map_double(double *, int, int, int, int, double, double, unsigned char *);
unsigned char *generate_map_long(long *, int, int, int, int, long, long, unsigned char *);

#endif