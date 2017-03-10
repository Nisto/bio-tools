#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define MIN(x, y) (((x) < (y)) ? (x) : (y))

#define HEADER_SIZE 32
#define TOC_ENT_SIZE 32

uint32_t get_u32_be(uint8_t *buf)
{
    return (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | (buf[3] << 0);
}

void parse_filename(char *dst, uint8_t *src)
{
    int i = 0;

    for (i = 0; i < 8 && src[i]; i++) {
        *dst++ = (char)(src[i]);
    }

    if (src[8]) {
        *dst++ = '.';

        for (i = 8; i < 24 && src[i]; i++) {
            *dst++ = (char)(src[i]);
        }
    }

    *dst = '\0';
}

void extract(FILE *fin, uint8_t *toc)
{
    FILE     *fout             = NULL;
    uint8_t  buffer[512]       = {0};
    uint32_t read_size         = 0;
    uint32_t offset            = 0;
    uint32_t size              = 0;
    char     outname[8+1+16+1] = {0};

    offset = get_u32_be(toc+0x00);
    size = get_u32_be(toc+0x04);
    parse_filename(outname, toc+0x08);

    fout = fopen(outname, "wb");

    if (fout == NULL) {
        printf("ERROR: fopen (%s)\n", outname);
        exit(EXIT_FAILURE);
    }

    if (0 != fseek(fin, offset, SEEK_SET)) {
        printf("ERROR: fseek (%s)\n", outname);
        exit(EXIT_FAILURE);
    }

    while (size > 0) {
        read_size = MIN(sizeof(buffer), size);

        if (read_size != fread(buffer, 1, read_size, fin)) {
            printf("ERROR: fread (%s)\n", outname);
            exit(EXIT_FAILURE);
        }

        if (read_size != fwrite(buffer, 1, read_size, fout)) {
            printf("ERROR: fwrite (%s)\n", outname);
            exit(EXIT_FAILURE);
        }

        size -= read_size;
    }
}

int main(int argc, char *argv[])
{
    FILE     *farc               = NULL;
    uint8_t  header[HEADER_SIZE] = {0};
    uint8_t  *toc                = NULL;
    uint32_t toc_count           = 0;
    uint32_t toc_offset          = 0;
    uint32_t toc_size            = 0;

    if (argc != 2) {
        printf("Usage: %s <file.arc>\n", argv[0]);
        return 1;
    }

    farc = fopen(argv[1], "rb");

    if (farc == NULL) {
        printf("ERROR: fopen (arc)\n");
        exit(EXIT_FAILURE);
    }

    if (HEADER_SIZE != fread(header, 1, HEADER_SIZE, farc)) {
        printf("ERROR: fread (header)\n");
        exit(EXIT_FAILURE);
    }

    toc_count = get_u32_be(header+0x04);
    toc_offset = get_u32_be(header+0x08);
    toc_size = TOC_ENT_SIZE * toc_count;
    toc = malloc(toc_size);

    if (toc == NULL) {
        printf("ERROR: malloc (TOC)\n");
        exit(EXIT_FAILURE);
    }

    if (0 != fseek(farc, toc_offset, SEEK_SET)) {
        printf("ERROR: fseek (TOC)\n");
        exit(EXIT_FAILURE);
    }

    if (toc_count != fread(toc, TOC_ENT_SIZE, toc_count, farc)) {
        printf("ERROR: fread (TOC)\n");
        exit(EXIT_FAILURE);
    }

    for (toc_offset = 0; toc_offset < toc_size; toc_offset += TOC_ENT_SIZE) {
        extract(farc, toc+toc_offset);
    }

    return 0;
}