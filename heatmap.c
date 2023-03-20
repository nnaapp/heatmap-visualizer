#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include "heatmap.h"

unsigned char *generate_cells_per_pixel(float *, int, int, float, int, int, int);
unsigned char *generate_pixels_per_cell(float *, int, int, float, int, int, int);
void fill_pixels(unsigned char *, float, int, int, int, int, int, int, float);
struct pixel avg_cell_chunk(float *, int, int, int, int, int, float);
int bind(int, int, int);


unsigned char *heatmap_gen(float *matrix, int cols, int rows, int imgW, int imgH, float baseTemp)
{
    unsigned char *map;


    // Two methods of heatmap generation, one for matrix > img, another for img > matrix
    // One has multiple cells in matrix per pixel ("chunk" of cells per pixel)
    // The other has multiple pixels per cell in matrix (inverse "chunk" of pixels per cell)
    if (cols >= imgW && rows >= imgH)
    {
        map = generate_cells_per_pixel(matrix, cols, rows, baseTemp, BPP, imgW, imgH);
    }
    else
    {
        map = generate_pixels_per_cell(matrix, cols, rows, baseTemp, BPP, imgW, imgH);
    }

    return map;
}

// cells per pixel mode
// Generates a 1d array containing color data for a heatmap
// Takes matrix and dimension data to accomplish this
// This one is for matrices larger than the image being generated
unsigned char *generate_cells_per_pixel(float *matrix, int cols, int rows, float base, int pixelBytes, int imgW, int imgH)
{
    unsigned char *map = malloc((imgW * imgH) * pixelBytes * sizeof(*map));

    const int cells_per_pixel_x = cols / imgW;
    const int cells_per_pixel_y = rows / imgH;

    const int remainder_x = cols - (cells_per_pixel_x * imgW);
    const int remainder_y = rows - (cells_per_pixel_y * imgH);

    int ypos, yend, yrem;
    ypos = yend = 0;
    yrem = remainder_y;
    for (int i = 0; i < imgH; i++)
    {
        ypos = yend;
        yend = ypos + cells_per_pixel_y;

        if (yrem > 0)
        {
            yrem--;
            yend++;
        }

        int xpos, xend, xrem;
        xpos = xend = 0;
        xrem = remainder_x;
        for (int j = 0; j < imgW; j++)
        {
            xpos = xend;
            xend = xpos + cells_per_pixel_x;

            if (xrem > 0)
            {
                xrem--;
                xend++;
            }
            
            struct pixel chunk_p = avg_cell_chunk(matrix, xpos, xend, ypos, yend, cols, base);
            map[(j + (i * imgW)) * pixelBytes + 0] = chunk_p.b;
            map[(j + (i * imgW)) * pixelBytes + 1] = chunk_p.g;
            map[(j + (i * imgW)) * pixelBytes + 2] = chunk_p.r;
        }
    }

    return map;
}

// pixels per cell mode
// Generates a 1d array containing color data for a heatmap
// Takes matrix and dimension data to accomplish this
// This one is for matrices smaller than the image being generated
unsigned char *generate_pixels_per_cell(float *matrix, int cols, int rows, float base, int pixelBytes, int imgW, int imgH)
{
    unsigned char *map = malloc((imgW * imgH) * pixelBytes * sizeof(*map));

    // x/y chunks per pixel, and remainder chunks after those
    int pixels_per_cell_x = 1. / ((float)cols / (float)imgW);
    int pixels_per_cell_y = 1. / ((float)rows / (float)imgH);

    int remainder_x = imgW - (pixels_per_cell_x * cols);
    int remainder_y = imgH - (pixels_per_cell_y * rows);

    int ypos, yend, yrem;
    ypos = yend = 0;
    yrem = remainder_y;
    for (int i = 0; i < rows; i++)
    {
        ypos = yend;
        yend = ypos + pixels_per_cell_y;

        if (yrem > 0)
        {
            yrem--;
            yend++;
        }

        int xpos, xend, xrem;
        xpos = xend = 0;
        xrem = remainder_x;
        for (int j = 0; j < cols; j++)
        {
            xpos = xend;
            xend = xpos + pixels_per_cell_x;

            if (xrem > 0)
            {
                xrem--;
                xend++;
            }

            // Fills multiple pixels in map, easiest way to handle this method
            fill_pixels(map, matrix[j + (i * cols)], xpos, xend, ypos, yend, imgW, pixelBytes, base);
        }
    }

    return map;
}

// Fills in multiple pixels in the output array,
// equal to a "chunk" of matrix cells, of a given size.
// Math to figure out chunk size is done elsewhere and fed in.
void fill_pixels(unsigned char *map, float cell, int start_x, int end_x, int start_y, int end_y, int imgW, int pixelBytes, float base)
{
    int r, g, b;
    float relativeTemp = cell - base;

    if (relativeTemp > 0)
    {
        r = ceil((relativeTemp) * RED_SHIFT);
        g = G_DEFAULT - (relativeTemp * GREEN_SHIFT);
        b = B_DEFAULT;
    }
    else if (relativeTemp < 0)
    {
        b = ceil((relativeTemp) * BLUE_SHIFT);
        g = G_DEFAULT_BRIGHT - (relativeTemp * GREEN_SHIFT);
        r = R_DEFAULT;
    }

    if (g >= G_DEFAULT - 2 && g <= G_DEFAULT + 1)
    {
        g = G_DEFAULT_BRIGHT;
    }

    b = bind(b, 0, 255);
    g = bind(g, 0, 255);
    r = bind(r, 0, 255);

    // Matrix cell's color, used for multiple pixels
    struct pixel cell_color;
    cell_color.b = b;
    cell_color.g = g;
    cell_color.r = r;

    for (int i = start_y; i < end_y; i++)
    {
        for (int j = start_x; j < end_x; j++)
        {
            map[(j + (i * imgW)) * pixelBytes + 0] = cell_color.b;
            map[(j + (i * imgW)) * pixelBytes + 1] = cell_color.g;
            map[(j + (i * imgW)) * pixelBytes + 2] = cell_color.r;
        }
    }
}

// Averages a "chunk" of cells in the matrix,
// of a given size. This forms an averaged heat
// pixel in the final image.
// Math done elsewhere for chunk size, and fed in.
struct pixel avg_cell_chunk(float *matrix, int start_x, int end_x, int start_y, int end_y, int cols, float base)
{
    int redAvg = 0, greenAvg = 0, blueAvg = 0;

    for (int i = start_y; i < end_y; i++)
    {
        for (int j = start_x; j < end_x; j++)
        {
            int b = B_DEFAULT;
            int g = G_DEFAULT;
            int r = R_DEFAULT;

            float relativeTemp = matrix[j + (i * cols)] - base;

            if (relativeTemp > 0)
            {
                r = ceil((relativeTemp) * RED_SHIFT);
                g = G_DEFAULT - (relativeTemp * GREEN_SHIFT);
                b = B_DEFAULT;
            }
            else if (relativeTemp < 0)
            {
                b = ceil((relativeTemp) * BLUE_SHIFT);
                g = G_DEFAULT_BRIGHT - (relativeTemp * GREEN_SHIFT);
                r = R_DEFAULT;
            }

            if (g >= G_DEFAULT - 2 && g <= G_DEFAULT + 1)
            {
                g = G_DEFAULT_BRIGHT;
            }

            redAvg   += r;
            greenAvg += g;
            blueAvg  += b;
        }
    }

    int total = (end_x - start_x) * (end_y - start_y);

    blueAvg  /= total;
    greenAvg /= total;
    redAvg   /= total;

    blueAvg  = bind(blueAvg, 0, 255);
    greenAvg = bind(greenAvg, 0, 255);
    redAvg   = bind(redAvg, 0, 255);

    struct pixel chunk_p;
    chunk_p.b = blueAvg;
    chunk_p.g = greenAvg;
    chunk_p.r = redAvg;

    return chunk_p;
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