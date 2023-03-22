#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include "heatmap.h"

unsigned char *heatmap_gen(Matrix *, int, int, unsigned char *);

void generate_cells_per_pixel(Matrix *, Map *);
void generate_pixels_per_cell(Matrix *, Map *);

void fill_pixels(Map *, Matrix *, int, int, int, int, int);
Color avg_cell_chunk(Matrix *, Map *, int, int, int, int);

Matrix *init_matrix(void *, int, int, long double, long double);
Map *init_map(int, int, int, unsigned char *);

int bind(int, int, int);
Color lerp(Color, Color, float);

// for use as function pointers, to support multiple numeric types
long double relative_val_float(Matrix *, const int);
long double relative_val_int(Matrix *, const int);
long double relative_val_double(Matrix *, const int);
long double relative_val_long(Matrix *, const int);

// Places data type matters:
// fill_pixels    : relativeTemp, t, maybe range
//                  store relative temp, t, and range as doubles? and cast to needed type?
//                  should work for ints, chars, floats, and doubles.
//                  then just have functions to do the necessary casted calcs.
//
// avg_cell_chunk : relativeTemp, t, maybe range
//                  same as above, should work hopefully
//
// bind           : data type being bound, might be deprecated though
//                  just dont use bind :)

// TODO: Improve performance, minimize casting, it runs a bit slow at the moment.
//       Maybe multithreading, also just optimizing function ptr usage.

// TODO: Make user defined color schemes more intuitive.
//       having to make a 9 element array kind of sucks, and
//       also sucks to unpack it.
unsigned char *heatmap_gen(Matrix *data, int imgW, int imgH, unsigned char *colors)
{
    Map *dataMap = init_map(imgW, imgH, BPP, colors);

    // Two methods of heatmap generation, one for matrix > img, another for img > matrix
    // One has multiple cells in matrix per pixel ("chunk" of cells per pixel)
    // The other has multiple pixel per cell in matrix (inverse "chunk" of pixels per cell)
    if (data->cols >= imgW && data->rows >= imgH)
    {
        // image will be smaller than matrix, each color being an average of cells
        generate_cells_per_pixel(data, dataMap);
    }
    else
    {
        // image will be larger than matrix, each cell having multiple pixels in the output
        generate_pixels_per_cell(data, dataMap);
    }

    unsigned char *final_map = dataMap->map;
    free(dataMap);

    return final_map;
}

//
/* Type specifying initial function, these are what the user calls in their code. */
/* This tells the functions doing the work what type to expect, via function ptr. */
//
unsigned char *generate_map_float(float *arr, int cols, int rows, int imgW, int imgH, float base, float range, unsigned char *colors)
{
    Matrix *data = init_matrix(arr, cols, rows, base, range);
    data->get_relative_val = relative_val_float;

    unsigned char *final_map = heatmap_gen(data, imgW, imgH, colors);

    free(data);
    return final_map;
}

unsigned char *generate_map_int(int *arr, int cols, int rows, int imgW, int imgH, int base, int range, unsigned char *colors)
{
    Matrix *data = init_matrix(arr, cols, rows, base, range);
    data->get_relative_val = relative_val_int;

    unsigned char *final_map = heatmap_gen(data, imgW, imgH, colors);

    free(data);
    return final_map;
}

unsigned char *generate_map_double(double *arr, int cols, int rows, int imgW, int imgH, double base, double range, unsigned char *colors)
{
    Matrix *data = init_matrix(arr, cols, rows, base, range);
    data->get_relative_val = relative_val_double;

    unsigned char *final_map = heatmap_gen(data, imgW, imgH, colors);

    free(data);
    return final_map;
}

unsigned char *generate_map_long(long *arr, int cols, int rows, int imgW, int imgH, long base, long range, unsigned char *colors)
{
    Matrix *data = init_matrix(arr, cols, rows, base, range);
    data->get_relative_val = relative_val_long;

    unsigned char *final_map = heatmap_gen(data, imgW, imgH, colors);

    free(data);
    return final_map;
}

//
/* Initializers for matrix and map structs, basically constructors. */
//
Matrix *init_matrix(void *matrix, int cols, int rows, long double base, long double range)
{
    Matrix *m = malloc(sizeof(*m));

    m->matrix  = matrix;
    m->cols    = cols;
    m->rows    = rows;
    m->baseVal = base;
    m->range   = range;

    return m;
}

Map *init_map(int width, int height, int bpp, unsigned char *colors)
{
    Map *m = malloc(sizeof(*m));

    m->map = malloc((width * height) * bpp * sizeof(*m->map));
    m->imgW = width;
    m->imgH = height;
    m->bpp = bpp;

    Color temp;
    temp.b = colors[0];
    temp.g = colors[1];
    temp.r = colors[2];
    m->color_low = temp;

    temp.b = colors[3];
    temp.g = colors[4];
    temp.r = colors[5];
    m->color_norm = temp;

    temp.b = colors[6];
    temp.g = colors[7];
    temp.r = colors[8];
    m->color_high = temp;

    return m;
}

//
/* The actual heavy lifting functions, converting the given data type matrix */
/* into color data and storing it in unsigned char form.                     */
//
// cells per pixel mode
// Generates a 1d array containing pixel data for a heatmap
// Takes matrix and dimension data to accomplish this
// This one is for matrices larger than the image being generated
void generate_cells_per_pixel(Matrix *data, Map *dataMap)
{
    const int cells_per_color_x = data->cols / dataMap->imgW;
    const int cells_per_color_y = data->rows / dataMap->imgH;

    const int remainder_x = data->cols - (cells_per_color_x * dataMap->imgW);
    const int remainder_y = data->rows - (cells_per_color_y * dataMap->imgH);

    int ypos, yend, yrem;
    ypos = yend = 0;
    yrem = remainder_y;
    for (int i = 0; i < dataMap->imgH; i++)
    {
        ypos = yend;
        yend = ypos + cells_per_color_y;

        if (yrem > 0)
        {
            yrem--;
            yend++;
        }

        int xpos, xend, xrem;
        xpos = xend = 0;
        xrem = remainder_x;
        for (int j = 0; j < dataMap->imgW; j++)
        {
            xpos = xend;
            xend = xpos + cells_per_color_x;

            if (xrem > 0)
            {
                xrem--;
                xend++;
            }
            
            Color chunk_p = avg_cell_chunk(data, dataMap, xpos, xend, ypos, yend);
            int start_index = (j + (i * dataMap->imgW)) * dataMap->bpp;
            dataMap->map[start_index + 0] = chunk_p.b;
            dataMap->map[start_index + 1] = chunk_p.g;
            dataMap->map[start_index + 2] = chunk_p.r;
        }
    }
}

// pixels per cell mode
// Generates a 1d array containing color data for a heatmap
// Takes matrix and dimension data to accomplish this
// This one is for matrices smaller than the image being generated
void generate_pixels_per_cell(Matrix *data, Map *dataMap)
{
    // x/y chunks per pixel, and remainder chunks after those
    const int colors_per_cell_x = 1. / ((float)data->cols / (float)dataMap->imgW);
    const int colors_per_cell_y = 1. / ((float)data->rows / (float)dataMap->imgH);

    const int remainder_x = dataMap->imgW - (colors_per_cell_x * data->cols);
    const int remainder_y = dataMap->imgH - (colors_per_cell_y * data->rows);

    int ypos, yend, yrem;
    ypos = yend = 0;
    yrem = remainder_y;
    for (int i = 0; i < data->rows; i++)
    {
        ypos = yend;
        yend = ypos + colors_per_cell_y;

        if (yrem > 0)
        {
            yrem--;
            yend++;
        }

        int xpos, xend, xrem;
        xpos = xend = 0;
        xrem = remainder_x;
        for (int j = 0; j < data->cols; j++)
        {
            xpos = xend;
            xend = xpos + colors_per_cell_x;

            if (xrem > 0)
            {
                xrem--;
                xend++;
            }

            // Fills multiple pixels in map, easiest way to handle this method
            fill_pixels(dataMap, data, j + (i * data->cols), xpos, xend, ypos, yend);
        }
    }
}

// Fills in multiple pixels in the output array,
// equal to a "chunk" of matrix cells, of a given size.
// Math to figure out chunk size is done elsewhere and fed in.
void fill_pixels(Map *dataMap, Matrix *data, int cell_index, int start_x, int end_x, int start_y, int end_y)
{
    long double relativeTemp = data->get_relative_val(data, cell_index);

    Color c;
    if (relativeTemp >= 0)
    {
        float t = relativeTemp / data->range;
        if (t > 1.0)
            t = 1.0;
        
        c = lerp(dataMap->color_norm, dataMap->color_high, t);
    }
    else if (relativeTemp < 0)
    {
        float t = -relativeTemp / data->range;
        if (t > 1.0)
            t = 1.0;
        
        c = lerp(dataMap->color_norm, dataMap->color_low, t);
    }

    for (int i = start_y; i < end_y; i++)
    {
        for (int j = start_x; j < end_x; j++)
        {
            int start_index = (j + (i * dataMap->imgW)) * dataMap->bpp;
            dataMap->map[start_index + 0] = c.b;
            dataMap->map[start_index + 1] = c.g;
            dataMap->map[start_index + 2] = c.r;
        }
    }
}

// Averages a "chunk" of cells in the matrix,
// of a given size. This forms an averaged heat
// pixel in the final image.
// Math done elsewhere for chunk size, and fed in.
Color avg_cell_chunk(Matrix *data, Map *dataMap, int start_x, int end_x, int start_y, int end_y)
{
    int redAvg = 0, greenAvg = 0, blueAvg = 0;
    Color c;

    for (int i = start_y; i < end_y; i++)
    {
        for (int j = start_x; j < end_x; j++)
        {
            float relativeTemp = data->get_relative_val(data, j + (i * data->cols));

            if (relativeTemp >= 0)
            {
                float t = relativeTemp / data->range;
                if (t > 1.0)
                    t = 1.0;

                c = lerp(dataMap->color_norm, dataMap->color_high, t);
            }
            else if (relativeTemp < 0)
            {
                float t = -relativeTemp / data->range;
                if (t > 1.0)
                    t = 1.0;
                
                c = lerp(dataMap->color_norm, dataMap->color_low, t);
            }

            redAvg   += c.r;
            greenAvg += c.g;
            blueAvg  += c.b;
        }
    }

    int total = (end_x - start_x) * (end_y - start_y);

    blueAvg  /= total;
    greenAvg /= total;
    redAvg   /= total;

    blueAvg  = bind(blueAvg, 0, 255);
    greenAvg = bind(greenAvg, 0, 255);
    redAvg   = bind(redAvg, 0, 255);

    Color chunk_p;
    chunk_p.b = blueAvg;
    chunk_p.g = greenAvg;
    chunk_p.r = redAvg;

    return chunk_p;
}

//
/* Utility functions, basic operations used multiple times. */
//
int bind(int val, int min, int max)
{
    if (val < 0)
    {
        val = 0;
    }
    else if (val > 255)
    {
        val = 255;
    }
    
    return val;
}

Color lerp(Color c1, Color c2, float t)
{
    Color newColor;
    newColor.b = c1.b * (1.0 - t) + (c2.b * t);
    newColor.g = c1.g * (1.0 - t) + (c2.g * t);
    newColor.r = c1.r * (1.0 - t) + (c2.r * t);
    return newColor;
}

//
/* Type-specific functions for use as function pointers,          */
/* these are used in structs to specify how to handle the matrix. */
//
long double relative_val_float(Matrix *m, const int i)
{
    float val = ((float *)m->matrix)[i];
    return (long double)(val - m->baseVal);
}

long double relative_val_int(Matrix *m, const int i)
{
    int val = ((int *)m->matrix)[i];
    return (long double)(val - m->baseVal);
}

long double relative_val_double(Matrix *m, const int i)
{
    double val = ((double *)m->matrix)[i];
    return (long double)(val - m->baseVal);
}

long double relative_val_long(Matrix *m, const int i)
{
    long val = ((double *)m->matrix)[i];
    return (long double)(val - m->baseVal);
}