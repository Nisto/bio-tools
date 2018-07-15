import os
import sys
import struct

def main(argc=len(sys.argv), argv=sys.argv):
    if argc != 2:
        print("Usage: %s <bgm>" % argv[0])
        return 1

    bgmpath = os.path.realpath(argv[1])

    if not os.path.isfile(bgmpath):
        print("BGM path is invalid")
        return 1

    stem = os.path.splitext(bgmpath)[0]

    with open(bgmpath, "rb") as bgm:
        bgmbuf = bgm.read()

    if bgmbuf[-0x10:-0x0C] != b"\0\0\0\0":
        sq1_off = struct.unpack("<I", bgmbuf[-0x04 :      ])[0]
        sq2_off = struct.unpack("<I", bgmbuf[-0x08 : -0x04])[0]
        vh_off  = struct.unpack("<I", bgmbuf[-0x0C : -0x08])[0]
        vb_off  = struct.unpack("<I", bgmbuf[-0x10 : -0x0C])[0]

        with open("%s_00.SQ" % stem, "wb") as sq:
            sq.write(bgmbuf[sq1_off:sq2_off])

        with open("%s_01.SQ" % stem, "wb") as sq:
            sq.write(bgmbuf[sq2_off:vh_off])
    else:
        sq_off  = struct.unpack("<I", bgmbuf[-0x04 :      ])[0]
        vh_off  = struct.unpack("<I", bgmbuf[-0x08 : -0x04])[0]
        vb_off  = struct.unpack("<I", bgmbuf[-0x0C : -0x08])[0]

        with open("%s.SQ" % stem, "wb") as sq:
            sq.write(bgmbuf[sq_off:vh_off])

    with open("%s.VH" % stem, "wb") as vh:
        vh.write(bgmbuf[vh_off:vb_off])

    with open("%s.VB" % stem, "wb") as vb:
        vb.write(bgmbuf[vb_off:-0x10])

    return 0

if __name__ == "__main__":
    main()
