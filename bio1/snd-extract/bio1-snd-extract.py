# Biohazard (GameCube) .snd extractor
# Author: Nisto
# Last revision: 2016, March 26

import os
import sys
import struct

if sys.version_info[0] > 2:
    raw_input = input

record_t = struct.Struct(">II")

def extract_snd(snd_path):
    xlp = os.path.splitext(snd_path)[0]
    with open(snd_path, "rb") as snd:
        meta = []
        for ext in ["pool", "proj", "sdir", "bin", "song", "samp"]:
            offset, size = record_t.unpack(snd.read(record_t.size))
            meta.append([ext, offset, size])
        # unk_id, bgm_id = record_t.unpack(snd.read(record_t.size))
        for ext, offset, size in meta:
            if ext != "bin":
                with open("%s.%s" % (xlp, ext), "wb") as out:
                    snd.seek(offset)
                    while size > 0:
                        read_max = 4096 if size > 4096 else size
                        if out.write( snd.read(read_max) ) == read_max:
                            size -= read_max
                        else:
                            break
    print("Extracted: %s" % snd_path)

def main(argc=len(sys.argv), argv=sys.argv):
    if argc < 2:
        print("Usage: %s <file/dir> [...]" % argv[0])
        return 1

    for arg in argv[1:]:
        path_in = os.path.realpath(arg)
        if os.path.isfile(path_in) \
            and os.path.splitext(path_in)[1].lower() == ".snd":
                extract_snd(path_in)
        elif os.path.isdir(path_in):
            for filename in os.listdir(path_in):
                snd_path = os.path.join(path_in, filename)
                if os.path.isfile(snd_path) \
                    and os.path.splitext(filename)[1].lower() == ".snd":
                        extract_snd(snd_path)
        else:
            print("Skipped (not a directory or .snd file): %s" % path_in)

    raw_input("\nNo more files to process.\n\nPress ENTER to quit.\n")

    return 0

if __name__=="__main__":
    main()