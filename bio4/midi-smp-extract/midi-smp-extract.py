#
# Biohazard 4 (GC) bio4midi.dat/hed sample extractor   by Nisto
# Last revision: Apr 28, 2015
#
# Developed under Python 3 and may or may not work with Python 2
#

import os
import sys
import struct

if sys.version_info[0] > 2:
    xrange = range

def samples_to_nibbles(samples):
    whole_frames = samples // 14
    remainder = samples % 14
    if remainder > 0:
        return (whole_frames * 16) + remainder + 2
    else:
        return whole_frames * 16

def samples_to_bytes(samples):
    nibbles = samples_to_nibbles(samples)
    return (nibbles // 2) + (nibbles % 2)

def extract_data(src, dst, size):
    read_max = 4096
    left = size
    while left:
        if read_max > left:
            read_max = left
        try:
            data = src.read(read_max)
        except EnvironmentError:
            sys.exit("ERROR: Read error at 0x%08X" % src.tell())
        if data == b"":
            break # EOF
        try:
            dst.write(data)
        except EnvironmentError:
            sys.exit("ERROR: Write error at 0x%08X" % dst.tell())
        left -= read_max

def dsp_header(meta):

    # 0x00 raw samples
    header = struct.pack(">I", meta["samples"])


    # 0x04 nibbles
    nibbles = samples_to_nibbles(meta["samples"])
    header += struct.pack(">I", nibbles)


    # 0x08 sample rate
    header += struct.pack(">I", meta["rate"])


    # 0x0C loop flag
    header += struct.pack(">H", 0)


    # 0x0E format (always zero - ADPCM)
    header += struct.pack(">H", 0)


    # 0x10 loop start address (in nibbles)
    header += struct.pack(">I", 0)


    # 0x14 loop end address (in nibbles)
    header += struct.pack(">I", 0)


    # 0x18 initial offset value (in nibbles)
    header += struct.pack(">I", 2)


    # 0x1C coefficients
    header += meta["coeffs"]


    # 0x3C gain (always zero for ADPCM)
    header += struct.pack(">H", 0)


    # 0x3E predictor/scale
    header += meta["ps"]


    # 0x40 sample history (n-1)
    header += struct.pack(">H", 0)


    # 0x42 sample history (n-2)
    header += struct.pack(">H", 0)


    # 0x44 loop context: predictor/scale
    header += struct.pack(">H", 0)


    # 0x46 loop context: sample history (n-1)
    header += struct.pack(">H", 0)


    # 0x48 loop context: sample history (n-2)
    header += struct.pack(">H", 0)


    # 0x4A pad (reserved)
    header += struct.pack("22x")


    return header

def read_u32_be(f):
    try:
        data = f.read(4)
    except EnvironmentError:
        sys.exit("ERROR: Read error at 0x%08X" % f.tell())
    return struct.unpack(">I", data)[0]

def main(argc=len(sys.argv), argv=sys.argv):
    if argc != 2:
        print("Usage: %s <bgm_dir>" % argv[0])
        return 1

    dir_in = os.path.realpath(argv[1])
    if os.path.isdir(dir_in) is not True:
        print("ERROR: Invalid directory path")
        return 1

    hed_path = os.path.join(dir_in, "bio4midi.hed")
    if os.path.isfile(hed_path) is not True:
        print("ERROR: Could not find bio4midi.hed")
        return 1

    dat_path = os.path.join(dir_in, "bio4midi.dat")
    if os.path.isfile(dat_path) is not True:
        print("ERROR: Could not find bio4midi.dat")
        return 1

    num_banks = os.path.getsize(hed_path) // 4

    bank_offsets = []

    with open(hed_path, "rb") as hed:

        for i in xrange(num_banks):

            bank_offset = read_u32_be(hed)

            if (bank_offset == 0 and hed.tell() == 4) or (bank_offset != 0 and hed.tell() > 4):

                bank_offsets.append(bank_offset)

    with open(dat_path, "rb") as dat:

        bank_num = 0

        for bank_offset in bank_offsets:

            dat.seek(bank_offset + 0x24)

            info_size = read_u32_be(dat)

            dat.seek(4, os.SEEK_CUR)

            info_offset = read_u32_be(dat)

            dat.seek(20, os.SEEK_CUR)

            stream_size = read_u32_be(dat)

            dat.seek(4, os.SEEK_CUR)

            stream_offset = read_u32_be(dat)

            dat.seek(bank_offset + info_offset + 0x20)

            meta1_offset = read_u32_be(dat)

            meta2_offset = read_u32_be(dat)

            meta1_size = meta2_offset - meta1_offset
            meta1_size -= meta1_size % 16

            num_samples = meta1_size // 16

            dat.seek(bank_offset + info_offset + 16 + meta1_offset)

            meta = {}

            for i in xrange(num_samples):

                meta[i] = {}

                meta[i]["rate"] = read_u32_be(dat)
                meta[i]["offset"] = read_u32_be(dat)
                meta[i]["samples"] = read_u32_be(dat)

                # seek past sample number (2) and reserved slot(?) (2)
                dat.seek(4, os.SEEK_CUR)

            dat.seek(bank_offset + info_offset + 16 + meta2_offset)

            for i in xrange(num_samples):
                # coefficients
                meta[i]["coeffs"] = dat.read(32)

                # gain maybe .. ? skipping for now, should always be 0 anyway
                dat.seek(2, os.SEEK_CUR)

                # predictor/scale
                meta[i]["ps"] = dat.read(2)

                # not entirely sure what the other stuff is yet (but presumably just sample history 1/2, etc.)
                dat.seek(10, os.SEEK_CUR)

            for i in xrange(num_samples):

                dat.seek(bank_offset + stream_offset + (meta[i]["offset"] // 2))

                dsp_path = os.path.join(dir_in, ("BANK%02d_%04d.dsp" % (bank_num, i)))

                sample_size = samples_to_bytes(meta[i]["samples"])

                with open(dsp_path, "wb") as dsp:

                    dsp.write( dsp_header(meta[i]) )

                    extract_data(dat, dsp, sample_size)

            bank_num += 1

    return 0

if __name__=="__main__":
    main()