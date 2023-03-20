#include <stdio.h>
#include <stdlib.h>
#include "bmp.h"

#define BYTES_PER_PIXEL 3 // rgb, no alpha or depth
#define HEADER_SIZE 14
#define INFO_SIZE 40

unsigned char *bmp_generate_header(int, int, int);
unsigned char *bmp_generate_info(int, int);

void bmp_generate_image(struct color *img, int height, int width, char *fileName)
{
    int byteWidth = width * BYTES_PER_PIXEL;

    unsigned char padding[] = {0, 0, 0};
    int paddingSize = (4 - (byteWidth) % 4) % 4; // how many padding bytes to add

    FILE *imgFile = fopen(fileName, "wb");

    unsigned char *header = bmp_generate_header(height, width, paddingSize);
    fwrite(header, 1, HEADER_SIZE, imgFile);

    unsigned char *info = bmp_generate_info(height, width);
    fwrite(info, 1, INFO_SIZE, imgFile);

    for (int i = height - 1; i >= 0; i--)
    {
        for (int j = 0; j < width; j++)
        {
            unsigned char color[] = {img[j + (i * width)].b, img[j + (i * width)].g, img[j + (i * width)].r};
            fwrite(color, 1, BYTES_PER_PIXEL, imgFile);
        }
        fwrite(padding, 1, paddingSize, imgFile);
    }

    free(header);
    free(info);
    fclose(imgFile);
}

unsigned char *bmp_generate_header(int height, int width, int padding)
{
    int size = HEADER_SIZE + INFO_SIZE + width * height * BYTES_PER_PIXEL + padding * height;

    unsigned char *header = (unsigned char *)malloc(sizeof(char) * HEADER_SIZE);
    for (int i = 0; i < HEADER_SIZE; i++)
        header[i] = 0;

    header[0]  = (unsigned char)'B';
    header[1]  = (unsigned char)'M';
    header[2]  = (unsigned char)(size);
    header[3]  = (unsigned char)(size >> 8);
    header[4]  = (unsigned char)(size >> 16);
    header[5]  = (unsigned char)(size >> 24);
    header[10] = (unsigned char)(HEADER_SIZE + INFO_SIZE);
    
    return header;
}

unsigned char *bmp_generate_info(int height, int width)
{
    unsigned char *info = (unsigned char *)malloc(sizeof(char) * INFO_SIZE);
    for (int i = 0; i < INFO_SIZE; i++)
        info[i] = 0;

    info[0]  = (unsigned char)INFO_SIZE;
    info[4]  = (unsigned char)(width);
    info[5]  = (unsigned char)(width >> 8);
    info[6]  = (unsigned char)(width >> 16);
    info[7]  = (unsigned char)(width >> 24);
    info[8]  = (unsigned char)(height);
    info[9]  = (unsigned char)(height >> 8);
    info[10] = (unsigned char)(height >> 16);
    info[11] = (unsigned char)(height >> 24);
    info[12] = 1;
    info[14] = (unsigned char)(BYTES_PER_PIXEL * 8);

    return info;
}