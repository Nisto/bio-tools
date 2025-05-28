import os
import sys
import glob
import struct

def get_u32_le(buf, off=0):
  return struct.unpack("<I", buf[off:off+4])[0]

def get_u32_be(buf, off=0):
  return struct.unpack(">I", buf[off:off+4])[0]

SEP_TOPHEADER_LEN = 6
SEP_SUBHEADER_LEN = 13

SEQ_MAGIC = b"pQES"
SEQ_VERSION = b"\0\0\0\1"
SEQ_EOF = b"\xFF\x2F\x00"

# SEP Subheader:
#   00 2 ID
#   02 2 PPQN
#   04 3 tempo
#   07 2 rhythm
#   09 4 size

def get_sep_seqs(sep_data):
  if sep_data[:4] != SEQ_MAGIC:
    print("Not a SEP/SEQ file!")
    sys.exit(1)

  num_seqs = 0
  off = SEP_TOPHEADER_LEN

  while off + SEP_SUBHEADER_LEN <= len(sep_data):
    seq_sd_length = get_u32_be(sep_data, off+0x09)
    seq_sd_start = off + SEP_SUBHEADER_LEN
    seq_sd_end = seq_sd_start + seq_sd_length

    if seq_sd_end <= len(sep_data) and sep_data[seq_sd_end - len(SEQ_EOF) : seq_sd_end] == SEQ_EOF:
      seq_topheader = SEQ_MAGIC + SEQ_VERSION
      seq_subheader = sep_data[off+0x02 : off+0x09]
      seq_scoredata = sep_data[seq_sd_start : seq_sd_end]
      yield (seq_topheader + seq_subheader + seq_scoredata)
      num_seqs += 1

    off = seq_sd_end

  if num_seqs < 1: # guess it's not really a SEP? return it as a SEQ
    yield sep_data

def extract(in_path, split_sep=False, split_vab=False):
  print("Extracting HSB: %s ..." % in_path, end='\t\t')

  stem = os.path.splitext(in_path)[0]

  with open(in_path, "rb") as hsb:
    data = hsb.read()

  if data.count(b"pQES") > 1:
    print("%s might have more than 1 SEQs" % in_path)

  vhoff = get_u32_le(data, len(data) - 0x04)
  sqoff = get_u32_le(data, len(data) - 0x08)
  vboff = get_u32_le(data, len(data) - 0x0C)

  vhsize = sqoff - vhoff
  sqsize = vboff - sqoff
  vbsize = len(data) - 0x0C - vboff

  if split_vab:
    with open("%s.vh" % stem, "wb") as vh:
      vh.write(data[vhoff:vhoff+vhsize])
    with open("%s.vb" % stem, "wb") as vb:
      vb.write(data[vboff:vboff+vbsize])
  else:
    with open("%s.vab" % stem, "wb") as vab:
      vab.write(data[vhoff:vhoff+vhsize])
      vab.write(data[vboff:vboff+vbsize])

  if split_sep:
    seq_idx = 0
    for sep_seq in get_sep_seqs(data[sqoff:sqoff+sqsize]):
      with open("%s_%02d.seq" % (stem, seq_idx), "wb") as seq:
        seq.write(sep_seq)
      seq_idx += 1
  else:
    with open("%s.sep" % stem, "wb") as seq:
      seq.write(data[sqoff:sqoff+sqsize])

  print("OK!")

def main(argc=len(sys.argv), argv=sys.argv):
  if argc < 2:
    print("Usage: %s [options] <HSB>" % argv[0])
    print()
    print("Options:")
    print()
    print("  --split-sep    Split .SEP into .SEQ")
    print("  --split-vab    Split .VAB into .VH + .VB")
    return 1

  split_sep = False
  split_vab = False

  for arg in argv[1:-1]:
    if arg.lower() == "--split-sep":
      split_sep = True
    elif arg.lower() == "--split-vab":
      split_vab = True

  for arg in argv[-1:]:
    in_path = os.path.realpath(arg)
    if os.path.isfile(in_path):
      extract(in_path, split_sep, split_vab)
    elif os.path.isdir(in_path):
      filter = os.path.join(in_path, "*.HSB")
      for hsb_path in glob.glob(filter):
        extract(hsb_path, split_sep, split_vab)
    else:
      print("Not an existing file or directory: %s" % in_path)

if __name__ == "__main__":
    main()