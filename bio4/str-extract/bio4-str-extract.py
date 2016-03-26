# Biohazard 4 (GameCube) stream extractor
# Author: Nisto
# Last revision: 2015, Apr 28

import os
import sys
import struct

if sys.version_info[0] > 2:
    xrange = range

def unpack_u32_be(data):
    return struct.unpack(">I", data)[0]

def read_u32_be(f):
    try:
        data = f.read(4)
    except EnvironmentError:
        sys.exit("ERROR: Read error at 0x%08X" % f.tell())
    return unpack_u32_be(data)

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

def extract_streams(hed, table_offset, stream_path):

    dir_in = os.path.dirname(stream_path)

    stream_name = os.path.basename(stream_path)
    stream_name = os.path.splitext(stream_name)[0]

    hed.seek(table_offset)

    num_entries = read_u32_be(hed)

    table_header_size = read_u32_be(hed)

    hed.seek(table_offset + table_header_size)

    offsets = []

    for i in xrange(num_entries):

        offset = read_u32_be(hed)

        if offset == 0:
            break

        offsets.append(offset)

    with open(stream_path, "rb") as stream:

        dsp_num = 0

        for offset in offsets:

            hed.seek(table_offset + table_header_size + offset)

            dsp_header = hed.read(0x80)

            nibbles = unpack_u32_be(dsp_header[0x08:0x0C])

            # nibbles -> bytes -> interleave of 0x4000 -> stereo (2)
            dsp_size = ((((nibbles // 2) // 0x4000) * 0x4000) + 0x4000) * 2

            dsp_offset = unpack_u32_be(dsp_header[0x1C:0x20])

            stream.seek(dsp_offset)

            dsp_path = os.path.join(dir_in, "%s_%03d.capdsp" % (stream_name, dsp_num))

            with open(dsp_path, "wb") as dsp:

                dsp.write(dsp_header)

                extract_data(stream, dsp, dsp_size)

            dsp_num += 1

def main(argc=len(sys.argv), argv=sys.argv):
    if argc != 2:
        print("Usage: %s <dir>" % argv[0])
        return 1

    dir_in = os.path.realpath(argv[1])
    if os.path.isdir(dir_in) is not True:
        print("ERROR: Invalid directory path")
        return 1

    hed_path = os.path.join(dir_in, "bio4str.hed")
    if os.path.isfile(hed_path) is not True:
        print("ERROR: Could not find bio4str.hed")
        return 1    

    bgm_path = os.path.join(dir_in, "bio4bgm.sbb")
    if os.path.isfile(bgm_path) is not True:
        print("ERROR: Could not find bio4bgm.sbb")
        return 1

    evt_path = os.path.join(dir_in, "bio4evt.sbb")
    if os.path.isfile(evt_path) is not True:
        print("ERROR: Could not find bio4evt.sbb")
        return 1

    with open(hed_path, "rb") as hed:

        bgm_table_offset = read_u32_be(hed)

        evt_table_offset = read_u32_be(hed)

        extract_streams(hed, bgm_table_offset, bgm_path)

        extract_streams(hed, evt_table_offset, evt_path)

    return 0

if __name__=="__main__":
    main()