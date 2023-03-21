#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include "heatmap.h"

void generate_cells_per_pixel(Matrix *, Map *, float);
void generate_pixels_per_cell(Matrix *, Map *, float);
void fill_pixels(Map *, float, int, int, int, int, float, float);
Color avg_cell_chunk(Matrix *, Map *, int, int, int, int, float);
void init_matrix(Matrix *, float *, int, int, float);
void init_map(Map *, int, int, int, unsigned char *);
int bind(int, int, int);
Color lerp(Color, Color, float);


unsigned char *heatmap_gen(float *matrix, int cols, int rows, int imgW, int imgH, float baseTemp, float range, unsigned char *colors)
{
    Matrix *data = malloc(sizeof(*data));
    init_matrix(data, matrix, cols, rows, baseTemp);

    Map *dataMap = malloc(sizeof(*dataMap));
    init_map(dataMap, imgW, imgH, BPP, colors);

    // Two methods of heatmap generation, one for matrix > img, another for img > matrix
    // One has multiple cells in matrix per pixel ("chunk" of cells per pixel)
    // The other has multiple pixel per cell in matrix (inverse "chunk" of pixels per cell)
    if (cols >= imgW && rows >= imgH)
    {
        // image will be smaller than matrix, each color being an average of cells
        generate_cells_per_pixel(data, dataMap, range);
    }
    else
    {
        // image will be larger than matrix, each cell having multiple pixels in the output
        generate_pixels_per_cell(data, dataMap, range);
    }

    free(data);
    unsigned char *final_map = dataMap->map;
    free(dataMap);

    return final_map;
}

// cells per pixel mode
// Generates a 1d array containing pixel data for a heatmap
// Takes matrix and dimension data to accomplish this
// This one is for matrices larger than the image being generated
void generate_cells_per_pixel(Matrix *data, Map *dataMap, float range)
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
            
            Color chunk_p = avg_cell_chunk(data, dataMap, xpos, xend, ypos, yend, range);
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
void generate_pixels_per_cell(Matrix *data, Map *dataMap, float range)
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
            fill_pixels(dataMap, data->matrix[j + (i * data->cols)], xpos, xend, ypos, yend, data->baseVal, range);
        }
    }
}

// TODO: somehow reduce this, i hate having this many args
//
// Fills in multiple pixels in the output array,
// equal to a "chunk" of matrix cells, of a given size.
// Math to figure out chunk size is done elsewhere and fed in.
void fill_pixels(Map *dataMap, float cell, int start_x, int end_x, int start_y, int end_y, float base, float range)
{
    //int r, g, b;
    float relativeTemp = cell - base;

    Color c;
    if (relativeTemp >= 0)
    {
        // = ceil((relativeTemp) * RED_SHIFT);
        //g = G_DEFAULT - (relativeTemp * GREEN_SHIFT);
        //b = B_DEFAULT;
        float t = relativeTemp / range;
        if (t > 1.0)
            t = 1.0;
        
        c = lerp(dataMap->color_norm, dataMap->color_high, t);
    }
    else if (relativeTemp < 0)
    {
        //b = ceil((relativeTemp) * BLUE_SHIFT);
        //g = G_DEFAULT_BRIGHT - (relativeTemp * GREEN_SHIFT);
        //r = R_DEFAULT;
        float t = -relativeTemp / range;
        if (t > 1.0)
            t = 1.0;
        
        c = lerp(dataMap->color_norm, dataMap->color_low, t);
    }

    //printf("%d %d %d\n", c.b, c.g, c.r);

    //if (g >= G_DEFAULT - 2 && g <= G_DEFAULT + 1)
    //{
    //    g = G_DEFAULT_BRIGHT;
    //}

    //b = bind(b, 0, 255);
    //g = bind(g, 0, 255);
    //r = bind(r, 0, 255);

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
Color avg_cell_chunk(Matrix *data, Map *dataMap, int start_x, int end_x, int start_y, int end_y, float range)
{
    int redAvg = 0, greenAvg = 0, blueAvg = 0;
    Color c;

    for (int i = start_y; i < end_y; i++)
    {
        for (int j = start_x; j < end_x; j++)
        {
            //int b = B_DEFAULT;
            //int g = G_DEFAULT;
            //int r = R_DEFAULT;

            float relativeTemp = data->matrix[j + (i * data->cols)] - data->baseVal;

            if (relativeTemp >= 0)
            {
                //r = ceil((relativeTemp) * RED_SHIFT);
                //g = G_DEFAULT - (relativeTemp * GREEN_SHIFT);
                //b = B_DEFAULT;
                float t = relativeTemp / range;
                if (t > 1.0)
                    t = 1.0;

                c = lerp(dataMap->color_norm, dataMap->color_high, t);
            }
            else if (relativeTemp < 0)
            {
                //b = ceil((relativeTemp) * BLUE_SHIFT);
                //g = G_DEFAULT_BRIGHT - (relativeTemp * GREEN_SHIFT);
                //r = R_DEFAULT;
                float t = -relativeTemp / range;
                if (t > 1.0)
                    t = 1.0;
                
                c = lerp(dataMap->color_norm, dataMap->color_low, t);
            }

            //if (g >= G_DEFAULT - 2 && g <= G_DEFAULT + 1)
            //{
            //    g = G_DEFAULT_BRIGHT;
            //}

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

void init_matrix(Matrix *m, float *matrix, int cols, int rows, float base)
{
    m->matrix  = matrix;
    m->cols    = cols;
    m->rows    = rows;
    m->baseVal = base;
}

void init_map(Map *dataMap, int width, int height, int bpp, unsigned char *colors)
{
    dataMap->map = malloc((width * height) * bpp * sizeof(*dataMap->map));
    dataMap->imgW = width;
    dataMap->imgH = height;
    dataMap->bpp = bpp;

    Color temp;
    temp.b = colors[0];
    temp.g = colors[1];
    temp.r = colors[2];
    dataMap->color_low = temp;

    temp.b = colors[3];
    temp.g = colors[4];
    temp.r = colors[5];
    dataMap->color_norm = temp;

    temp.b = colors[6];
    temp.g = colors[7];
    temp.r = colors[8];
    dataMap->color_high = temp;
}

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