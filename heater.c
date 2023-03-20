#include <stdlib.h>
#include <stdio.h>
#include "heater.h"

#define READ_BUFFER 1024

// Reads the first line of the named file into a buffer,
// the first line is always the number of heaters,
// and returns that number as an int.
int get_heater_count(char *heaterFileName)
{
    FILE *heaterFile;
    heaterFile = fopen(heaterFileName, "r");
    char buffer[READ_BUFFER]; // byte buffer to store the first line in

    fgets(buffer, READ_BUFFER, heaterFile); // first line (by new line char) contains number of heaters
    int numHeaters = atoi(buffer);   // aka number of lines to read

    fclose(heaterFile);
    return numHeaters;
}

// Takes file name and number of heaters,
// returns null if no heaters, and parses each line of the file
// given by the number of heaters into ints coordinates and float temp.
// Returns heater struct with heater data.
struct Heater *get_heaters(char *heaterFileName, int numHeaters)
{
    if (!numHeaters)
    {
        return NULL;
    }

    FILE *heaterFile;
    heaterFile = fopen(heaterFileName, "r"); // r for read only
    char buffer[READ_BUFFER];

    struct Heater *heaters = malloc(sizeof(struct Heater) * numHeaters); // Allocate room for all heaters in file
    fscanf(heaterFile, "%s", buffer);
    for (int i = 0; i < numHeaters; i++)
    {
        fscanf(heaterFile, "%s", buffer);
        int row = atoi(buffer);

        fscanf(heaterFile, "%s", buffer);
        int col = atoi(buffer);

        fscanf(heaterFile, "%s", buffer);
        char *ptr;
        float t = strtod(buffer, &ptr);

        struct Heater tmp;
        tmp.row = row;
        tmp.col = col;
        tmp.temp = t;

        heaters[i] = tmp;
    }

    fclose(heaterFile);
    return heaters;
}