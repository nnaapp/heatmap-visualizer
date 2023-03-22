#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <omp.h>
#include <math.h>
#include "heater.h"     // defines and manages heaters
#include "matrix.h"     // defines and manages matrix operations
#include "bmp.h"        // defines and outputs BMP files from color arrays
#include "loadingbar.h" // defines and draws progress bar in console, gives user something to stare at
#include "heatmap.h"

#define EXPECTED_ARGS 9
#define TRANSFER_MAX 1.1000001 // floating point imprecision, man
#define TRASNFER_MIN 1

void fill_heaters(float *, struct Heater *, int, int);
void fill_heaters_parallel(float *, struct Heater *, int, int);
void handle_loading_bar(int, int, struct LoadingBar *);


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


    // temp constants for testing
    const int imgDim = 1024;
    const int imgMaxMul = 5;
    // // // // // // // // // //

    int imgW = imgDim, imgH = imgDim;

    if (numCols > numRows)
    {
        imgW *= ((float)numCols / (float)numRows);
    }
    else if (numRows > numCols)
    {
        imgH *= ((float)numRows / (float)numCols);
    }

    if (imgW > imgDim * imgMaxMul || imgH > imgDim * imgMaxMul)
    {
        printf("\nImage could not be generated. This is likely due to the matrix being extremely lopsided.\n");
        printf("A very lopsided matrix will result in aspect ratio preservation being too extreme.\n");

        free(outImgName);
        free(matrix);

        return 0;
    }

    unsigned char colors[] = {255, 224, 122, 
                              96, 204, 143, 
                              94, 84, 235};
    unsigned char *heatmap = generate_map_float(matrix, numCols, numRows, imgW, imgH, baseTemp, 25.0, colors);

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