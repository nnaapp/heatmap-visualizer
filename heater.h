#ifndef UTIL_H
#define UTIL_H

int get_heater_count(char *);
struct Heater *get_heaters(char *, int);

struct Heater
{
    int row;
    int col;
    float temp;
};

#endif