#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <jpeglib.h>
#include <jerror.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#define RGB565_FROM_RGB(r, g, b)    (((uint16_t)r & 0xF8) << 8) | (((uint16_t)g & 0xFC) << 3) | ((uint16_t)b >> 3)

int main(int argc, char **argv) {
    struct fb_fix_screeninfo finfo = { 0 };
    FILE *fIn = fopen(argv[1], "rb");

    if (argc != 2) {
      fprintf(stderr, "Usage: %s <jpg-file>\n", argv[0]);
      return 1;
    }

    if (fIn == NULL) {
        fprintf(stderr, "Could not open input file.\n");
        return 1;
    }

    struct jpeg_decompress_struct info; //for our jpeg info
    struct jpeg_error_mgr err;          //the error handler

    info.err = jpeg_std_error(&err);
    jpeg_create_decompress(&info);      //fills info structure

    jpeg_stdio_src(&info, fIn);
    jpeg_read_header(&info, true);

    jpeg_start_decompress(&info);

    int w = info.output_width;
    int h = info.output_height;
    int numChannels = info.num_components; // 3 = RGB, 4 = RGBA
    uint32_t dataSize = w * h * numChannels;

    // read RGB(A) scanlines one at a time into jdata[]
    uint8_t *data = (uint8_t *)malloc(dataSize);
    uint8_t* rowptr;

    while(info.output_scanline < h) {
        rowptr = data + info.output_scanline * w * numChannels;
        jpeg_read_scanlines(&info, &rowptr, 1);
    }

    jpeg_finish_decompress(&info);
    fclose(fIn);

    // Open the framebuffer for reading and writing
    int fbfd = open("/dev/fb0", O_RDWR);
    if (fbfd < 0) {
        perror("Error: cannot open framebuffer device.\n");
        return 1;
    }

    // Get fixed screen information
    if (ioctl(fbfd, FBIOGET_FSCREENINFO, &finfo)) {
        perror("Error reading fixed information.\n");
        close(fbfd);
        return 1;
    }

    // Map fb to user mem 
    uint16_t *fbp = (uint16_t*)mmap(0,
                     finfo.smem_len,
                     PROT_READ | PROT_WRITE,
                     MAP_SHARED,
                     fbfd,
                     0);

    // Copy pixels to fb
    for(int i = 0; i < w * h; i++) {
        *fbp++ = RGB565_FROM_RGB(data[i * numChannels], data[(i * 3) + 1], data[(i * 3) + 2]);
    }

    close(fbfd);
    return 0;
}

