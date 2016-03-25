#include <stdio.h>  // fopen, fseek, ftell, fread, fwrite, fclose, (f)printf
#include <stdlib.h> // calloc, free, exit
#include <stdint.h> // uintN_t
#include <string.h> // memcpy

// ALZ decompressor 0.4
// Author: Nisto
// Last revision: 2015, Jan 11

uint32_t filesize(FILE *f)
{
    uint32_t ret = ftell(f);
    fseek(f, 0, SEEK_END);
    uint32_t size = ftell(f);
    fseek(f, ret, SEEK_SET);
    return size;
}

uint32_t get_u32_le(uint8_t *data, uint32_t addr)
{
    uint32_t val = 0;
    val |= data[addr];
    val |= data[addr+1]<<8;
    val |= data[addr+2]<<16;
    val |= data[addr+3]<<24;
    return val;
}

uint8_t get_bit(uint8_t *data, uint32_t *addr, uint8_t *bitnum)
{
    uint8_t bit = (data[*addr] >> *bitnum) & 1;
    if (*bitnum < 7)
        ++*bitnum;
    else
        *bitnum = 0, ++*addr;
    return bit;
}

const uint8_t bitcounts[4] = { 0x02, 0x04, 0x06, 0x0A };

void alz_decompress(char *path_in, char *path_out)
{
    printf("Decompressing: %s\n", path_in);

    FILE *infile = fopen(path_in, "rb");
    if (infile==NULL)
    {
        fprintf(stderr, "ERROR: Could not open input file.\n");
        exit(EXIT_FAILURE);
    }

    uint32_t alzsize = filesize(infile);
    if (alzsize < 5)
    {
        fprintf(stderr, "ERROR: Invalid ALZ file.\n");
        exit(EXIT_FAILURE);
    }

    uint32_t alzaddr = 0;
    uint8_t *alz = (uint8_t *)calloc(alzsize, 1);
    if (alzsize != fread(alz, 1, alzsize, infile))
    {
        fprintf(stderr, "ERROR: Failed reading data from input file.\n");
        exit(EXIT_FAILURE);
    }

    fclose(infile);

    uint8_t alztype = alz[alzaddr];
    alzaddr += 1;
    if (alztype < 1 || alztype > 2)
    {
        fprintf(stderr, "ERROR: ALZ type not supported: %X\n", alztype);
        exit(EXIT_FAILURE);
    }

    uint32_t destsize = get_u32_le(alz, alzaddr);
    alzaddr += 4;
    if (alzsize-5 > destsize)
    {
        fprintf(stderr, "ERROR: Compressed size > decompressed size.\n");
        exit(EXIT_FAILURE);
    }

    uint32_t destaddr = 0;
    uint8_t *dest = (uint8_t *)calloc(destsize, 1);
    if(dest==NULL)
    {
        fprintf(stderr, "ERROR: Failed allocating memory for decompressed data.\n");
        exit(EXIT_FAILURE);
    }

    uint8_t alz_bitnum=0, tmp_bitnum, tmp_index;
    uint32_t tmp_offset, tmp_size;

    while (destaddr < destsize)
    {
        if (!get_bit(alz, &alzaddr, &alz_bitnum))
        {
            tmp_offset = 0;
            if (alztype == 1)
            {
                for (tmp_bitnum=0; tmp_bitnum < 0x0A; tmp_bitnum++)
                    tmp_offset |= (get_bit(alz, &alzaddr, &alz_bitnum) << tmp_bitnum);
            }
            else
            {
                for (tmp_index=0; !get_bit(alz, &alzaddr, &alz_bitnum); tmp_index++);

                if (tmp_index > 3)
                {
                    fprintf(stderr, "ERROR: Bitcount index out of range at 0x%08X\n", alzaddr);
                    exit(EXIT_FAILURE);
                }

                for (tmp_bitnum=0; tmp_bitnum < bitcounts[tmp_index]; tmp_bitnum++)
                    tmp_offset |= (get_bit(alz, &alzaddr, &alz_bitnum) << tmp_bitnum);
            }

            if((destaddr-tmp_offset-1) < 0)
            {
                fprintf(stderr, "ERROR: Invalid source offset at 0x%08X\n", alzaddr);
                exit(EXIT_FAILURE);
            }



            tmp_size = 0;
            for (tmp_index=0; !get_bit(alz, &alzaddr, &alz_bitnum); tmp_index++);

            if (tmp_index > 3)
            {
                fprintf(stderr, "ERROR: Bitcount index out of range at 0x%08X\n", alzaddr);
                exit(EXIT_FAILURE);
            }

            for (tmp_bitnum=0; tmp_bitnum < bitcounts[tmp_index]; tmp_bitnum++)
                tmp_size |= (get_bit(alz, &alzaddr, &alz_bitnum) << tmp_bitnum);

            if(tmp_size > (destsize-(destaddr+tmp_offset+1)))
            {
                fprintf(stderr, "ERROR: Invalid source size at 0x%08X\n", alzaddr);
                exit(EXIT_FAILURE);
            }



            memcpy(&dest[destaddr], &dest[destaddr-tmp_offset-1], tmp_size);

            destaddr += tmp_size;
        }
        else
        {
            dest[destaddr] = 0;
            for (tmp_bitnum=0; tmp_bitnum < 8; tmp_bitnum++)
                dest[destaddr] |= (get_bit(alz, &alzaddr, &alz_bitnum) << tmp_bitnum);

            destaddr += 1;
        }
    }

    free(alz);

    FILE *outfile = fopen(path_out, "wb");
    if(outfile==NULL)
    {
        fprintf(stderr, "ERROR: Could not create/open output file.\n");
        exit(EXIT_FAILURE);
    }

    if(destsize != fwrite(dest, 1, destsize, outfile))
    {
        fprintf(stderr, "ERROR: Failed to write decompressed data.\n");
        exit(EXIT_FAILURE);
    }

    free(dest);
    fclose(outfile);

    printf("Successfully wrote decompressed data.\n");
}

int main(int argc, char *argv[])
{
    printf("ALZ decompressor 0.4 by Nisto\n");

    if (argc != 3)
    {
        fprintf(stderr, "Usage: %s <input> <output>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    alz_decompress(argv[1], argv[2]);

    return 0;
}