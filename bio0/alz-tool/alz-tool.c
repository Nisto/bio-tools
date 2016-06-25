#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

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

#define LSB_MASK(nbits) ((1UL << (nbits)) - 1)

uint8_t get_bit(bitstream *bs)
{
    uint8_t bit;

    if (bs->offset >= bs->size)
    {
        printf("ERROR: read_bit() buffer overread\n");
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
        printf("ERROR: read_bits() supports reading at most 32 bits at a time (%d bits requested)\n", bits_todo);
        exit(EXIT_FAILURE);
    }

    if (bs->offset >= bs->size || bits_todo > ((bs->size - bs->offset) * 8) - bs->bitnum)
    {
        printf("ERROR: read_bits() buffer overread at 0x%08X@%u (%d bits requested)\n", bs->offset, bs->bitnum, bits_todo);
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
 
void write_bit(bitstream *bs, uint8_t x)
{
    if(x > 1)
    {
        fprintf(stderr, "ERROR: write_bit() expects 0 or 1\n");
        exit(EXIT_FAILURE);
    }

    if (bs->offset >= bs->size)
    {
        fprintf(stderr, "ERROR: write_bit() buffer overrun\n");
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
 
void write_bits(bitstream *bs, uint8_t nbits, uint32_t x)
{
    if (nbits > 32)
    {
        fprintf(stderr, "ERROR: write_bits() supports writing at most 32 bits at a time\n");
        exit(EXIT_FAILURE);
    }

    while (nbits)
    {
        if (bs->offset >= bs->size)
        {
            fprintf(stderr, "ERROR: write_bits() buffer overrun\n");
            exit(EXIT_FAILURE);
        }

        if (x & 1)
            // set current bit
            bs->buf[bs->offset] |= 1 << bs->bitnum;
        else
            // mask all bits except current, effectively clearing it
            bs->buf[bs->offset] &= ~(1 << bs->bitnum);

        if (x)
            x >>= 1;

        if (bs->bitnum < 7)
            ++bs->bitnum;
        else
            ++bs->offset, bs->bitnum=0;

        --nbits;
    }
}



/*
 * ALZ stuff
 */

#define ALZ_OFF_TYPE 0x00
#define ALZ_OFF_DCSZ 0x01
#define ALZ_OFF_DATA 0x05

#define ALZ_FLAG_WORD 0
#define ALZ_FLAG_BYTE 1

const uint8_t alz_bitcounts[4] = { 2, 4, 6, 10 };

uint8_t alz_get_bitcount(bitstream *bs)
{
    uint8_t i = 0, start_bitnum = bs->bitnum;

    uint32_t start_offset = bs->offset;

    while (get_bit(bs) == 0)
    {
        i++;

        if (i >= sizeof(alz_bitcounts))
        {
            printf("ERROR: 0x%08X@%u: invalid bitcount index code.\n", start_offset, start_bitnum);
            exit(EXIT_FAILURE);
        }
    }

    return alz_bitcounts[i];
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

    if(!alz.buf)
    {
        printf("ERROR: Could not allocate memory for compressed data.\n");
        exit(EXIT_FAILURE);
    }

    if(alz.size != fread(alz.buf, 1, alz.size, infile))
    {
        printf("ERROR: Could not read input file.\n");
        exit(EXIT_FAILURE);
    }

    alz_type = alz.buf[ALZ_OFF_TYPE];

    dest_offset = 0;

    dest_size = get_u32_le(&alz.buf[ALZ_OFF_DCSZ]);

    dest = calloc(dest_size, 1);

    if(!dest)
    {
        printf("ERROR: Could not allocate memory for decompressed data.\n");
        exit(EXIT_FAILURE);
    }

    if(alz_type != 0)
    {
        while(dest_offset < dest_size)
        {
            if(get_bit(&alz) == ALZ_FLAG_WORD)
            {
                word_offset = get_bits(&alz, alz_type == 1 ? 10 : alz_get_bitcount(&alz));

                word_size = get_bits(&alz, alz_get_bitcount(&alz));

                if((int)dest_offset - (int)word_offset - 1 < 0)
                {
                    printf("ERROR: 0x%08X@%u: Word offset out of range: %d", alz.offset, alz.bitnum, word_offset);
                    exit(EXIT_FAILURE);
                }

                if(((int)dest_offset - (int)word_offset - 1) + (int)word_size > (int)dest_offset)
                {
                    printf("ERROR: 0x%08X@%u: Word size too large: %d", alz.offset, alz.bitnum, word_size);
                    exit(EXIT_FAILURE);
                }

                memcpy(&dest[dest_offset], &dest[dest_offset - word_offset - 1], word_size);

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

    if(dest_size != fwrite(dest, 1, dest_size, outfile))
    {
        printf("ERROR: Could not write decompressed data.\n");
        exit(EXIT_FAILURE);
    }

    free(alz.buf);

    free(dest);

    printf("Successfully wrote decompressed data.\n");
}

void write_word_param(bitstream *bs, uint32_t value)
{
    if (value > 63) // 10-bit value
    {
        write_bits(bs, 4, 1 << 3); // write 1-terminated bitcount index code
        write_bits(bs, 10, value); // write value
    }
    else if (value > 15) // 6-bit value
    {
        write_bits(bs, 3, 1 << 2);
        write_bits(bs, 6, value);
    }
    else if (value > 3) // 4-bit value
    {
        write_bits(bs, 2, 1 << 1);
        write_bits(bs, 4, value);
    }
    else // 2-bit value
    {
        write_bits(bs, 1, 1 << 0);
        write_bits(bs, 2, value);
    }
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

    // validate type
    if(alz_type < '0' || alz_type > '2')
    {
        printf("ERROR: Invalid ALZ compression type specified.\n");
        exit(EXIT_FAILURE);
    }

    alz_type -= '0';

    // initialize input stuff
    input_offset = 0;
    input_size = filesize(infile);

    if(!input_size)
    {
        fprintf(stderr, "ERROR: Empty input file.\n");
        exit(EXIT_FAILURE);
    }

    input_buf = calloc(input_size, 1);

    if(!input_buf)
    {
        fprintf(stderr, "ERROR: Could not allocate memory for input data.\n");
        exit(EXIT_FAILURE);
    }

    if(input_size != fread(input_buf, 1, input_size, infile))
    {
        fprintf(stderr, "ERROR: Could not read input data.\n");
        exit(EXIT_FAILURE);
    }

    // initialize output stuff
    alz.bitnum = 0;
    alz.offset = ALZ_OFF_DATA;
    alz.size = alz.offset + input_size * 2; 
    alz.buf = calloc(alz.size, 1);

    // write ALZ header
    alz.buf[ALZ_OFF_TYPE] = (uint8_t)alz_type;
    put_u32_le(&alz.buf[ALZ_OFF_DCSZ], input_size);

    if(alz_type != 0)
    {
        // type 1 / 2 - compressed
        while (input_offset < input_size)
        {
            run_length = 0;
            longest_run_offset = 0;
            longest_run_length = 0;

            // dictionary offset
            dictionary_offset = input_offset > 1024 ? input_offset - 1024 : 0;

            // lookahead buffer offset
            lookahead_offset = input_offset;

            // try finding words
            while (dictionary_offset < input_offset && lookahead_offset < input_size)
            {
                if (input_buf[dictionary_offset] == input_buf[lookahead_offset])
                {
                    ++dictionary_offset;
                    ++lookahead_offset;

                    run_length += 1;

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

            // write data
            if(longest_run_length >= 2)
            {
                // type = dictionary word
                write_bit(&alz, ALZ_FLAG_WORD);

                // word offset
                if (alz_type == 1)
                    write_bits(&alz, 10, (input_offset - longest_run_offset) - 1);
                else
                    write_word_param(&alz, (input_offset - longest_run_offset) - 1);

                // even though offsets are 10-bit values (1023) at most,
                // they can still effectively be 1024, since an additional
                // byte is always mixed into the equation. however, the length
                // is limited to 1023 since it's not added to during decompression
                if (longest_run_length == 1024)
                    longest_run_length = 1023;

                // word size
                write_word_param(&alz, longest_run_length);

                input_offset += longest_run_length;
            }
            else
            {
                // type = uncompressed octet
                write_bit(&alz, ALZ_FLAG_BYTE);

                // octet value
                write_bits(&alz, 8, input_buf[input_offset]);

                input_offset += 1;
            }
        }

        if (alz.bitnum != 0)
            alz.offset += 1;
    }
    else
    {
        // type 0 - uncompressed
        memcpy(&alz.buf[alz.offset], input_buf, input_size);
        alz.offset += input_size;
    }

    if(alz.offset != fwrite(alz.buf, 1, alz.offset, outfile))
    {
        fprintf(stderr, "ERROR: Could not write output data.\n");
        exit(EXIT_FAILURE);
    }

    free(input_buf);

    free(alz.buf);

    printf("Successfully wrote compressed data.\n");
}

int main(int argc, char *argv[])
{
    FILE
        *infile,
        *outfile;

    if (argc != 4 || (argv[1][0] != 'c' && argv[1][0] != 'd'))
    {
        printf(
            "Usage: %s d|c[=0|1|2] <input> <output>\n"
            "\n"
            "To compress: c source.ext destination.alz\n"
            "To decompress: d source.alz destination.ext\n"
            "\n"
            "If compressing, an ALZ type may be specified in the first parameter:\n"
            "   0 copies the input (uncompressed)\n"
            "   1 encodes all dictionary offsets as 10-bit values\n"
            "   2 uses variable-width dictionary offsets and is selected by default\n"
            , argv[0]
        );
        exit(EXIT_FAILURE);
    }

    infile = fopen(argv[2], "rb");
    if(!infile)
    {
        printf("ERROR: Could not open input file.\n");
        exit(EXIT_FAILURE);
    }

    outfile = fopen(argv[3], "wb");
    if(!outfile)
    {
        printf("ERROR: Could not create/open output file.\n");
        exit(EXIT_FAILURE);
    }

    switch(argv[1][0])
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