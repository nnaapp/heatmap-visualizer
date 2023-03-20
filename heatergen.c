#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main(int argc, char **argv)
{
    if (argc != 7)
    {
        printf("Invalid arguments, correct usage: heatergen numHeaters tempMin tempMax height width fileName\n");
        return 1;
    }

    int numHeaters = atoi(argv[1]);
    char *ptr;
    float tempMin = strtod(argv[2], &ptr);
    float tempMax = strtod(argv[3], &ptr);
    int height = atoi(argv[4]);
    int width = atoi(argv[5]);
    char *outFileName = argv[6];

    FILE *outFile = fopen(outFileName, "w");
    fprintf(outFile, "%d\n", numHeaters);

    srand(time(NULL));

    for (int i = 0; i < numHeaters; i++)
    {
        fprintf(outFile, "%d %d %f\n", rand() % height, 
            rand() % width, (((float)rand() / (float)RAND_MAX) * (tempMax - tempMin)) + tempMin);
    }

    fclose(outFile);

    return 0;
}