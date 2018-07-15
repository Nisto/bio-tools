import os
import sys
import struct

def get_u32_le(buf, off=0):
    return struct.unpack("<I", buf[off:off+4])[0]

def get_u16_le(buf, off=0):
    return struct.unpack("<H", buf[off:off+2])[0]

def zerorange(buf, start, end):
    for offset in range(start, end):
        buf[offset] = 0

def main(argc=len(sys.argv), argv=sys.argv):
    if argc != 2:
        print("Usage: %s <bgm>" % argv[0])
        return 1

    bgmpath = os.path.realpath(argv[1])

    if not os.path.isfile(bgmpath):
        print("Invalid BGM path")
        return 1

    stem = os.path.splitext(bgmpath)[0]

    with open(bgmpath, "rb") as bgm:
        bgmbuf = bgm.read()

    offset = 0

    seqs = []

    while 1:
        cksize = get_u32_le(bgmbuf, offset)

        if offset + cksize > len(bgmbuf):
            break

        tmpoff = offset + cksize - 1

        while bgmbuf[tmpoff] == 0:
            tmpoff -= 1

        if tmpoff > offset:
            tmpoff -= 1

        if bgmbuf[tmpoff:tmpoff+3] != b"\xFF\x2F\x00":
            break

        seqs.append(bgmbuf[offset+4:offset+cksize])

        offset += cksize

    headerbuf = bgmbuf[offset:]

    # fix up sequence header

    for sn in range(len(seqs)):
        seqpbuf  = b"pQES"                      # ID
        seqpbuf += b"\x00\x00\x00\x01"          # Version
        seqpbuf += seqs[sn][0x03:0x05]          # Resoiution of quarter note
        seqpbuf += seqs[sn][0x00:0x03][::-1]    # Tempo (reversed bute order???)
        seqpbuf += seqs[sn][0x06:0x08]          # Rhythm
        seqpbuf += b"\x00"                      # Delta time???
        seqpbuf += seqs[sn][0x08:]              # Score data

        seqs[sn] = seqpbuf

    # fix up bank header

    vhbuf  = b"pBAV"                            # ID
    vhbuf += b"\x05\x00\x00\x00"                # Version
    vhbuf += b"\x00\x00\x00\x00"                # VAB ID
    vhbuf += headerbuf[0x00:0x04]               # Waveform size
    vhbuf += headerbuf[0x10:0x20]               # System reserved (2)
                                                # Number of programs (2)
                                                # Number of tones (2)
                                                # VAG count (2)
                                                # Master volume (1)
                                                # Master pan (1)
                                                # Bank attr 1 (1)
                                                # Bank attr 2 (1)
                                                # System reserved (4)

    # fix up programs

    ps = get_u16_le(headerbuf, 0x12)

    vhbuf += headerbuf[0x20:0x20+ps*16]

    vhbuf += (128 - ps) * b"\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"

    # fix up tone attributes

    offset = 0x20 + ps*16

    max_tones_per_program = get_u32_le(headerbuf, 0x0C)

    for pn in range(ps):
        ts = headerbuf[0x20 + pn*16 + 0x00]
        vhbuf += headerbuf[offset : offset + ts*32]
        tpl = bytearray(headerbuf[offset : offset + 32])
        zerorange(tpl, 0x00, 0x0C)
        zerorange(tpl, 0x10, 0x18)
        vhbuf += (16 - ts) * tpl
        offset += max_tones_per_program * 32

    # fix up VAG offset table

    offset = get_u32_le(headerbuf, 0x08)

    vhbuf += headerbuf[offset:]

    vhbuf += (256 - len(headerbuf[offset:]) // 2) * b"\x00\x00"

    # write the files

    if len(seqs) > 1:
        for seqnum in range(len(seqs)):
            with open("%s_%02d.SEQ" % (stem, seqnum), "wb") as seq:
                seq.write(seqs[seqnum])
    else:
        with open("%s.SEQ" % stem, "wb") as seq:
            seq.write(seqs[0])

    with open("%s.VH" % stem, "wb") as vh:
        vh.write(vhbuf)

    return 0

if __name__ == "__main__":
    main()