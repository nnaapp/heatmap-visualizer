#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <omp.h>
#include <math.h>
#include "heater.h"     // defines and manages heaters
#include "matrix.h"     // defines and manages matrix operations
#include "bmp.h"        // defines and outputs BMP files from color arrays
#include "loadingbar.h" // defines and draws progress bar in console, gives user something to stare at

#define EXPECTED_ARGS 9
#define TRANSFER_MAX 1.1000001 // floating point imprecision, man
#define TRASNFER_MIN 1

#define IMG_DEFAULT 1024
#define IMG_MAX IMG_DEFAULT*5
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

struct color *matrix_generate_heatmap(float *, int, int, float, int, int, int);
struct color *matrix_generate_heatmap_small(float *, int, int, float, int, int, int);
void fill_pixels(struct color *, float, int, int, int, int, int, float);
struct color avg_cell_chunk(float *, int, int, int, int, int, float);
void handle_loading_bar(int, int, struct LoadingBar *);
void fill_heaters(float *, struct Heater *, int, int);
void fill_heaters_parallel(float *, struct Heater *, int, int);


int main(int argc, char **argv)
{
    if (argc != EXPECTED_ARGS)
    {
        printf("Invalid usage.\n");
        printf("Example: ./heat num_threads numRows numCols baseTemp k timesteps heaterFileName outputFileName\n");
        return 1;
    }

    /* Command line arguments and parsing*/
    int numThreads, numRows, numCols, timesteps;
    float baseTemp, transferRate;
    char *heaterFileName;
    char *outFileName;
    char *outImgName;

    numThreads = atoi(argv[1]);
    numRows = atoi(argv[2]);
    numCols = atoi(argv[3]);
    timesteps = atoi(argv[6]);
    char *ptr; // ptr for strtod
    baseTemp = strtod(argv[4], &ptr);
    transferRate = strtod(argv[5], &ptr);
    heaterFileName = argv[7];
    outFileName = argv[8];
    outImgName = (char *)malloc(sizeof(char) * strlen(outFileName) + 4);
    strcpy(outImgName, outFileName);
    strcat(outImgName, ".bmp");


    /* Argument validation and error prevention */
    if (numThreads < 1)
    {
        printf("Invalid number of threads, must be >0.\n");
        return 1;
    }

    if (numRows < 1 || numCols < 1)
    {
        printf("Invalid matrix dimensions, must be 1x1 or greater.\n");
        return 1;
    }

    if (timesteps < 1)
    {
        printf("Invalid number of timesteps, must be >0, time can't go backwards.\n");
        return 1;
    }

    if (transferRate > TRANSFER_MAX || transferRate < TRASNFER_MIN)
    {
        printf("Invalid heat transfer rate, choose a number between 1 and 1.1 (inclusive).\n");
        return 1;
    }


    /* File reading, data and variable initialization */

    // heaters is a pointer array of heater structs to contain the row/col/temp of each heater
    // dynamic arrays are impossible to calculate the length of in code, so that is stored from file
    int heaterCount = get_heater_count(heaterFileName);
    struct Heater *heaters = get_heaters(heaterFileName, heaterCount);
    if (!heaters) // if get_heaters returns null, the file is bad or empty
    {
        printf("ERROR: Heaters could not be found in file.\n");
        return 1;
    }

    // initialize loading bar for use in loop
    struct LoadingBar progress = loadingbar_init(50, '#', '-', '[', ']');
    loadingbar_draw(&progress);

    // initialize matrix of argument size and temp, fill it with heaters from file
    // these matrices persist, and are swapped around instead of re-allocated
    float *matrix = matrix_init(numCols, numRows, baseTemp);
    float *tmpMatrix = matrix_init_empty(numCols, numRows);


    /* Matrix timesteps, data processing into CSV and BMP image */

    // timesteps equate to a "step" in time, the length of which is arbitrary.
    // each time step runs the equation on each cell once, and then the heaters
    // are replaced.
    for (int i = 0; i < timesteps; i++)
    {
        fill_heaters(matrix, heaters, heaterCount, numCols);
        matrix_step_parallel(&matrix, &tmpMatrix, numCols, numRows, transferRate, baseTemp, numThreads);

        handle_loading_bar(i, timesteps - 1, &progress);
    }
    fill_heaters(matrix, heaters, heaterCount, numCols);
    printf("\n");

    matrix_out(matrix, numCols, numRows, outFileName); // out to file

    printf("\nHeat dispersion complete.\n");
    printf("CSV format file saved to:\t%s\n", outFileName);
    free(tmpMatrix);
    free(heaters);

    int imgW, imgH;
    imgW = imgH = IMG_DEFAULT;

    // Resize dimensions of image if matrix is lopsided, to preserve aspect ratio
    if (numCols > numRows)
    {
        imgW = IMG_DEFAULT * ((float)numCols / (float)numRows);
    }
    else if (numRows > numCols)
    {
        imgH = IMG_DEFAULT * ((float)numRows / (float)numCols);
    }

    if (imgW > IMG_MAX || imgH > IMG_MAX)
    {
        printf("\nImage could not be generated. This is likely due to the matrix being extremely lopsided.\n");
        printf("A very lopsided matrix will result in aspect ratio preservation being too extreme.\n");

        free(outImgName);
        free(matrix);

        return 0;
    }

    // Two methods of heatmap generation, one for matrix > img, another for img > matrix
    // One has multiple cells in matrix per pixel ("chunk" of cells per pixel)
    // The other has multiple pixels per cell in matrix (inverse "chunk" of pixels per cell)
    struct color *heatmap;
    if (numCols >= imgW && numRows >= imgH)
    {
        heatmap = matrix_generate_heatmap(matrix, numCols, numRows, baseTemp, BPP, imgW, imgH);
    }
    else
    {
        heatmap = matrix_generate_heatmap_small(matrix, numCols, numRows, baseTemp, BPP, imgW, imgH);
    }

    // formats the above data to a real image
    bmp_generate_image(heatmap, imgH, imgW, outImgName);


    /* Finalization and memory deallocation */
    printf("BMP heatmap image saved to:\t%s\n", outImgName);

    free(outImgName);
    free(matrix);
    free(heatmap);

    return 0;
}


// Takes a 2d array matrix, array of heaters, and the number of heaters.
// Returns the matrix with the heaters placed where they belong, based on struct.
void fill_heaters(float *matrix, struct Heater *heaters, int arrayLen, int cols)
{
    for (int i = 0; i < arrayLen; i++)
    {
        matrix[heaters[i].col + (heaters[i].row * cols)] = heaters[i].temp;
    }
}

// not used, was easier and faster runtime to just not
/*void fill_heaters_parallel(float *matrix, struct Heater *heaters, int arrayLen, int cols)
{
    #pragma omp for schedule(static)
    for (int i = 0; i < arrayLen; i++)
    {
        matrix[heaters[i].col + (heaters[i].row * cols)] = heaters[i].temp;
    }
}*/

// cells per pixel mode
// Generates a 1d array containing color data for a heatmap
// Takes matrix and dimension data to accomplish this
// This one is for matrices larger than the image being generated
struct color *matrix_generate_heatmap(float *matrix, int cols, int rows, float base, int pixelBytes, int imgW, int imgH)
{
    struct color *map = (struct color *)malloc((imgW * imgH) * sizeof(struct color));

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
            
            struct color chunk = avg_cell_chunk(matrix, xpos, xend, ypos, yend, cols, base);

            map[j + (i * imgW)] = chunk;
        }
    }

    return map;
}

// pixels per cell mode
// Generates a 1d array containing color data for a heatmap
// Takes matrix and dimension data to accomplish this
// This one is for matrices smaller than the image being generated
struct color *matrix_generate_heatmap_small(float *matrix, int cols, int rows, float base, int pixelBytes, int imgW, int imgH)
{
    struct color *map = (struct color *)malloc((imgW * imgH) * sizeof(struct color));

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

            fill_pixels(map, matrix[j + (i * cols)], xpos, xend, ypos, yend, imgW, base);
        }
    }

    return map;
}

// Fills in multiple pixels in the output array,
// equal to a "chunk" of matrix cells, of a given size.
// Math to figure out chunk size is done elsewhere and fed in.
void fill_pixels(struct color *map, float pixel, int start_x, int end_x, int start_y, int end_y, int imgW, float base)
{
    struct color default_color;
    default_color.b = B_DEFAULT;
    default_color.g = G_DEFAULT;
    default_color.r = R_DEFAULT;

    int r, g, b;
    float relativeTemp = pixel - base;

    if (relativeTemp > 0)
    {
        r = ceil((relativeTemp) * RED_SHIFT);
        g = default_color.g - (relativeTemp * GREEN_SHIFT);
        b = default_color.b;
    }
    else if (relativeTemp < 0)
    {
        b = ceil((relativeTemp) * BLUE_SHIFT);
        g = G_DEFAULT_BRIGHT - (relativeTemp * GREEN_SHIFT);
        r = default_color.r;
    }

    if (g >= default_color.g - 2 && g <= default_color.g + 1)
    {
        g = G_DEFAULT_BRIGHT;
    }

    if (b > 255) { b = 255; }
    if (b < 0)   { b = 0;   }
    if (g > 255) { g = 255; }
    if (g < 0)   { g = 0;   }
    if (r > 255) { r = 255; }
    if (r < 0)   { r = 0;   }

    struct color newColor;
    newColor.b = b;
    newColor.g = g;
    newColor.r = r;

    for (int i = start_y; i < end_y; i++)
    {
        for (int j = start_x; j < end_x; j++)
        {
            map[j + (i * imgW)] = newColor;
        }
    }
}

// Averages a "chunk" of cells in the matrix,
// of a given size. This forms an averaged heat
// pixel in the final image.
// Math done elsewhere for chunk size, and fed in.
struct color avg_cell_chunk(float *matrix, int start_x, int end_x, int start_y, int end_y, int cols, float base)
{
    struct color default_color;
    default_color.b = B_DEFAULT;
    default_color.g = G_DEFAULT;
    default_color.r = R_DEFAULT;

    int redAvg, greenAvg, blueAvg;
    redAvg = greenAvg = blueAvg = 0;

    for (int i = start_y; i < end_y; i++)
    {
        for (int j = start_x; j < end_x; j++)
        {
            int b = default_color.b;
            int g = default_color.g;
            int r = default_color.r;

            float relativeTemp = matrix[j + (i * cols)] - base;

            if (relativeTemp > 0)
            {
                r = ceil((relativeTemp) * RED_SHIFT);
                g = default_color.g - (relativeTemp * GREEN_SHIFT);
                b = default_color.b;
            }
            else if (relativeTemp < 0)
            {
                b = ceil((relativeTemp) * BLUE_SHIFT);
                g = G_DEFAULT_BRIGHT - (relativeTemp * GREEN_SHIFT);
                r = default_color.r;
            }

            if (g >= default_color.g - 2 && g <= default_color.g + 1)
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

    if (blueAvg < 0)    { blueAvg =  0;   }
    if (blueAvg > 255)  { blueAvg =  255; }
    if (greenAvg < 0)   { greenAvg = 0;   }
    if (greenAvg > 255) { greenAvg = 255; }
    if (redAvg < 0)     { redAvg =   0;   }
    if (redAvg > 255)   { redAvg =   255; }

    struct color chunk;
    chunk.b = blueAvg;
    chunk.g = greenAvg;
    chunk.r = redAvg;

    return chunk;
}

// Handles loading bar, checks if it needs an update.
// Conditions for update are an increase in whole-number percent,
// or another filling-character needing to be placed.
void handle_loading_bar(int loop_pos, int timesteps, struct LoadingBar *bar)
{
    unsigned long curProgress = (((unsigned long)loop_pos) * bar->maxLen) / timesteps;
    int curPercent = floor(((float)(loop_pos)) / ((float)(timesteps)) * 100);

    if (curProgress != bar->curLen || curPercent > bar->percent)
    {
        bar->curLen = curProgress;
        bar->percent = curPercent;
        loadingbar_draw(bar);
    }
}