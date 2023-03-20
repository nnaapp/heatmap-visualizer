#ifndef BMP_H
#define BMP_H

struct color
{
    unsigned char r;
    unsigned char g;
    unsigned char b;
};

void bmp_generate_image(struct color *, int, int, char *);

#endif