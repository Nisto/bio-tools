#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>

#define LSB_MASK(nbits) ((1UL << (nbits)) - 1)

#define ALZ_OFF_TYPE 0x00
#define ALZ_OFF_DCSZ 0x01
#define ALZ_OFF_DATA 0x05

#define ALZ_FLAG_WORD 0
#define ALZ_FLAG_BYTE 1

uint32_t get_u32_le(uint8_t *ptr)
{
    return ptr[0] | (ptr[1]<<8) | (ptr[2]<<16) | (ptr[3]<<24);
}

void put_u32_le(uint8_t *ptr, uint32_t n)
{
    ptr[0] = n & 0xFF;
    ptr[1] = (n >> 8) & 0xFF;
    ptr[2] = (n >> 16) & 0xFF;
    ptr[3] = (n >> 24) & 0xFF;
}

uint32_t filesize(FILE *f)
{
    uint32_t ret, size;
    ret = ftell(f);
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fseek(f, ret, SEEK_SET);
    return size;
}

typedef struct {
    uint8_t *buf;
    uint32_t offset;
    uint32_t size;
    uint8_t bitnum;
} bitstream;

uint8_t get_bit(bitstream *bs)
{
    uint8_t bit;

    if (bs->offset >= bs->size)
    {
        printf("ERROR: 0x%08" PRIX32 "@%" PRIu8 ": buffer overread (attempted reading 1 bit)\n", bs->offset, bs->bitnum);
        exit(EXIT_FAILURE);
    }

    bit = (bs->buf[bs->offset] >> bs->bitnum) & 1;

    if(bs->bitnum < 7)
        ++bs->bitnum;
    else
        ++bs->offset, bs->bitnum=0;

    return bit;
}

uint32_t get_bits(bitstream *bs, uint8_t bits_todo)
{
    uint8_t bits_read = 0, octet_bits_left;
    uint32_t bits = 0;

    if (bits_todo > 32)
    {
        printf("ERROR: 0x%08" PRIX32 "@%" PRIu8 ": can not read more than 32 bits at a time (attempted reading %" PRIu8 " bits)\n", bs->offset, bs->bitnum, bits_todo);
        exit(EXIT_FAILURE);
    }

    if (bs->offset >= bs->size || bits_todo > ((bs->size - bs->offset) * 8) - bs->bitnum)
    {
        printf("ERROR: 0x%08" PRIX32 "@%" PRIu8 ": buffer overread (attempted reading %" PRIu8 " bits)\n", bs->offset, bs->bitnum, bits_todo);
        exit(EXIT_FAILURE);
    }

    while (bits_todo)
    {
        octet_bits_left = 8 - bs->bitnum;
        if (bits_todo >= octet_bits_left)
        {
            bits |= (bs->buf[bs->offset++] >> bs->bitnum) << bits_read;
            bs->bitnum = 0;
            bits_read += octet_bits_left;
            bits_todo -= octet_bits_left;
        }
        else
        {
            bits |= ((bs->buf[bs->offset] >> bs->bitnum) & LSB_MASK(bits_todo)) << bits_read;
            bs->bitnum += bits_todo;
            bits_read += bits_todo;
            bits_todo = 0;
        }
    }

    return bits;
}
 
void put_bit(bitstream *bs, uint8_t x)
{
    if (bs->offset >= bs->size)
    {
        printf("ERROR: 0x%08" PRIX32 "@%" PRIu8 ": buffer overrun (attempted writing 1 bit)\n", bs->offset, bs->bitnum);
        exit(EXIT_FAILURE);
    }

    if (x & 1)
        bs->buf[bs->offset] |= 1 << bs->bitnum;
    else
        bs->buf[bs->offset] &= ~(1 << bs->bitnum);

    if (bs->bitnum < 7)
        ++bs->bitnum;
    else
        ++bs->offset, bs->bitnum=0;
}
 
void put_bits(bitstream *bs, uint8_t bits_todo, uint32_t x)
{
    uint8_t octet_bits_left;

    if (bits_todo > 32)
    {
        printf("ERROR: 0x%08" PRIX32 "@%" PRIu8 ": can not write more than 32 bits at a time (attempted writing %" PRIu8 " bits)\n", bs->offset, bs->bitnum, bits_todo);
        exit(EXIT_FAILURE);
    }

    if (bs->offset >= bs->size || bits_todo > ((bs->size - bs->offset) * 8) - bs->bitnum)
    {
        printf("ERROR: 0x%08" PRIX32 "@%" PRIu8 ": buffer overrun (attempted writing %" PRIu8 " bits)\n", bs->offset, bs->bitnum, bits_todo);
        exit(EXIT_FAILURE);
    }

    while (bits_todo)
    {
        octet_bits_left = 8 - bs->bitnum;
        if (bits_todo >= octet_bits_left)
        {
            bs->buf[bs->offset] &= LSB_MASK(bs->bitnum);
            bs->buf[bs->offset++] |= (x & LSB_MASK(octet_bits_left)) << bs->bitnum;
            bs->bitnum = 0;
            bits_todo -= octet_bits_left;
            x >>= octet_bits_left;
        }
        else
        {
            bs->buf[bs->offset] &= ~(LSB_MASK(bits_todo) << bs->bitnum);
            bs->buf[bs->offset] |= (x & LSB_MASK(bits_todo)) << bs->bitnum;
            bs->bitnum += bits_todo;
            bits_todo = 0;
            x = 0;
        }
    }
}

const uint8_t alz_bitcounts[4] = { 2, 4, 6, 10 };

uint16_t alz_get_varlen_value(bitstream *bs)
{
    uint8_t i = 0;

    while (get_bit(bs) == 0)
    {
        ++i;

        if (i >= sizeof(alz_bitcounts))
        {
            printf("ERROR: 0x%08" PRIX32 "@%" PRIu8 ": invalid bitcount index code.\n", bs->offset, bs->bitnum);
            exit(EXIT_FAILURE);
        }
    }

    return get_bits(bs, alz_bitcounts[i]);
}

void alz_put_varlen_value(bitstream *bs, uint16_t x)
{
    uint8_t i = 0;

    while (x >> alz_bitcounts[i])
    {
        ++i;

        if (i >= sizeof(alz_bitcounts))
        {
            printf("ERROR: 0x%08" PRIX32 "@%" PRIu8 ": value too large: %" PRIu16 "\n", bs->offset, bs->bitnum, x);
            exit(EXIT_FAILURE);
        }
    }

    put_bits(bs, alz_bitcounts[i]+1+i, (x<<(1+i)) | (1<<i));
}

void alz_decompress(FILE *infile, FILE *outfile)
{
    bitstream
        alz;

    uint8_t
        *dest,
        alz_type;

    uint32_t
        dest_offset,
        dest_size,
        word_offset,
        word_size;

    alz.offset = ALZ_OFF_DATA;

    alz.bitnum = 0;

    alz.size = filesize(infile);

    alz.buf = calloc(alz.size, 1);

    if (!alz.buf)
    {
        printf("ERROR: Could not allocate memory for input data.\n");
        exit(EXIT_FAILURE);
    }

    if (alz.size != fread(alz.buf, 1, alz.size, infile))
    {
        printf("ERROR: Could not read input data.\n");
        exit(EXIT_FAILURE);
    }

    alz_type = alz.buf[ALZ_OFF_TYPE];

    dest_offset = 0;

    dest_size = get_u32_le(&alz.buf[ALZ_OFF_DCSZ]);

    dest = calloc(dest_size, 1);

    if (!dest)
    {
        printf("ERROR: Could not allocate memory for decompressed data.\n");
        exit(EXIT_FAILURE);
    }

    if (alz_type != 0)
    {
        while (dest_offset < dest_size)
        {
            if (get_bit(&alz) == ALZ_FLAG_WORD)
            {
                if (alz_type == 1)
                    word_offset = get_bits(&alz, 10);
                else
                    word_offset = alz_get_varlen_value(&alz);

                word_offset += 1;

                word_size = alz_get_varlen_value(&alz);

                if(word_offset > dest_offset)
                {
                    printf("ERROR: 0x%08" PRIX32 "@%" PRIu8 ": Word offset out of range: %" PRIu32 "\n", alz.offset, alz.bitnum, word_offset);
                    exit(EXIT_FAILURE);
                }

                if(word_size > word_offset)
                {
                    printf("ERROR: 0x%08" PRIX32 "@%" PRIu8 ": Word size too large: %" PRIu32 "\n", alz.offset, alz.bitnum, word_size);
                    exit(EXIT_FAILURE);
                }

                memcpy(&dest[dest_offset], &dest[dest_offset - word_offset], word_size);

                dest_offset += word_size;
            }
            else
            {
                dest[dest_offset] = (uint8_t)get_bits(&alz, 8);

                dest_offset += 1;
            }
        }
    }
    else
    {
        dest = alz.buf + alz.offset;
    }

    if (dest_size != fwrite(dest, 1, dest_size, outfile))
    {
        printf("ERROR: Could not write decompressed data.\n");
        exit(EXIT_FAILURE);
    }

    free(alz.buf);

    free(dest);

    printf("Successfully wrote decompressed data.\n");
}

void alz_compress(FILE *infile, FILE *outfile, char alz_type)
{
    bitstream
        alz;

    uint8_t
        *input_buf;

    uint16_t
        run_length,
        longest_run_offset,
        longest_run_length;

    uint32_t
        input_offset,
        input_size,
        dictionary_offset,
        lookahead_offset;

    if (alz_type < '0' || alz_type > '2')
    {
        printf("ERROR: Invalid ALZ compression type specified.\n");
        exit(EXIT_FAILURE);
    }

    alz_type -= '0';

    input_offset = 0;

    input_size = filesize(infile);

    if (!input_size)
    {
        printf("ERROR: Empty input file.\n");
        exit(EXIT_FAILURE);
    }

    input_buf = calloc(input_size, 1);

    if (!input_buf)
    {
        printf("ERROR: Could not allocate memory for input data.\n");
        exit(EXIT_FAILURE);
    }

    if (input_size != fread(input_buf, 1, input_size, infile))
    {
        printf("ERROR: Could not read input data.\n");
        exit(EXIT_FAILURE);
    }

    alz.bitnum = 0;

    alz.offset = ALZ_OFF_DATA;

    alz.size = alz.offset + input_size * 2; 

    alz.buf = calloc(alz.size, 1);

    if (!alz.buf)
    {
        printf("ERROR: Could not allocate memory for compressed data.\n");
        exit(EXIT_FAILURE);
    }

    alz.buf[ALZ_OFF_TYPE] = (uint8_t)alz_type;

    put_u32_le(&alz.buf[ALZ_OFF_DCSZ], input_size);

    if (alz_type != 0)
    {
        while (input_offset < input_size)
        {
            run_length = 0;
            longest_run_offset = 0;
            longest_run_length = 0;

            dictionary_offset = input_offset > 1024 ? input_offset - 1024 : 0;

            lookahead_offset = input_offset;

            while (dictionary_offset < input_offset)
            {
                if (lookahead_offset < input_size && input_buf[dictionary_offset] == input_buf[lookahead_offset])
                {
                    ++dictionary_offset;
                    ++lookahead_offset;
                    ++run_length;
                    if (run_length >= longest_run_length)
                    {
                        longest_run_offset = dictionary_offset - run_length;
                        longest_run_length = run_length;
                    }
                }
                else
                {
                    dictionary_offset = (dictionary_offset - run_length) + 1;
                    lookahead_offset = input_offset;
                    run_length = 0;
                }
            }

            if (longest_run_length >= 2)
            {
                put_bit(&alz, ALZ_FLAG_WORD);

                longest_run_offset = (input_offset - longest_run_offset) - 1;

                if (longest_run_length >= 1024)
                    longest_run_length = 1023;

                if (alz_type == 1)
                    put_bits(&alz, 10, longest_run_offset);
                else
                    alz_put_varlen_value(&alz, longest_run_offset);

                alz_put_varlen_value(&alz, longest_run_length);

                input_offset += longest_run_length;
            }
            else
            {
                put_bit(&alz, ALZ_FLAG_BYTE);

                put_bits(&alz, 8, input_buf[input_offset]);

                input_offset += 1;
            }
        }

        if (alz.bitnum != 0)
            alz.offset += 1;
    }
    else
    {
        memcpy(&alz.buf[alz.offset], input_buf, input_size);
        alz.offset += input_size;
    }

    if (alz.offset != fwrite(alz.buf, 1, alz.offset, outfile))
    {
        printf("ERROR: Could not write compressed data.\n");
        exit(EXIT_FAILURE);
    }

    printf("Successfully wrote compressed data.\n");

    free(input_buf);

    free(alz.buf);
}

int main(int argc, char *argv[])
{
    FILE
        *infile,
        *outfile;

    if (argc != 4 || (argv[1][0] != 'd' && argv[1][0] != 'c'))
    {
        printf(
            "alz-tool v0.1 by Nisto\n"
            "\n"
            "Usage: %s d|c[=0|1|2] <input> <output>\n"
            "\n"
            "To compress  : c source.ext destination.alz\n"
            "To decompress: d source.alz destination.ext\n"
            "\n"
            "If compressing, an ALZ type may be specified in the first parameter:\n"
            "   0 copies the input (uncompressed)\n"
            "   1 encodes all dictionary offsets as 10-bit values\n"
            "   2 uses variable-length dictionary offsets (default)\n",
            argv[0]
        );
        exit(EXIT_FAILURE);
    }

    infile = fopen(argv[2], "rb");
    if (!infile)
    {
        printf("ERROR: Could not open input file.\n");
        exit(EXIT_FAILURE);
    }

    outfile = fopen(argv[3], "wb");
    if (!outfile)
    {
        printf("ERROR: Could not create/open output file.\n");
        exit(EXIT_FAILURE);
    }

    switch (argv[1][0])
    {
        case 'd':
            alz_decompress(infile, outfile);
            break;

        case 'c':
            alz_compress(infile, outfile, argv[1][1] == '=' ? argv[1][2] : '2');
            break;
    }

    fclose(infile);
    fclose(outfile);

    return 0;
}
