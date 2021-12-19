import os
import sys
import struct

def get_u32_be(buf, off=0):
  return struct.unpack(">I", buf[off:off+4])[0]

def get_u32_le(buf, off=0):
  return struct.unpack("<I", buf[off:off+4])[0]

def extract_layer0(filepath):
  extracted_files = []

  filepath = os.path.realpath(filepath)
  outdir = "%s_extracted" % os.path.splitext(filepath)[0]
  if not os.path.isdir(outdir):
    os.makedirs(outdir)
  with open(filepath, "rb") as f:
    data = f.read()

  if data[:3] != b"AFS":
    print("Bad header")
    exit(1)
  num_files = get_u32_le(data, 0x04)
  toc = data[0x08:0x08+num_files*8]
  for offset, size in struct.iter_unpack("<II", toc):
    outpath = os.path.join(outdir, "%d" % len(extracted_files))
    with open(outpath, "wb") as f:
      f.write(data[offset:offset+size])
    extracted_files.append(outpath)

  return extracted_files

def extract_layer1(filepaths):
  extracted_files = []

  for filepath in filepaths:
    outdir = "%s_extracted" % filepath
    if not os.path.isdir(outdir):
      os.makedirs(outdir)
    with open(filepath, "rb") as f:
      data = f.read()

    toc_offset = 0
    idx = 0
    while True:
      offset = get_u32_be(data, toc_offset)
      if offset == 0: break
      size = get_u32_be(data, toc_offset+0x04)
      type = get_u32_be(data, toc_offset+0x08)
      outpath = os.path.join(outdir, "%d" % type)
      with open(outpath, "wb") as f:
        f.write(data[offset:offset+size])
      extracted_files.append(outpath)
      idx += 1
      toc_offset += 0x10

  return extracted_files

def extract_musyx_songs(filepath):
  outdir = "%s_extracted" % filepath
  if not os.path.isdir(outdir):
    os.makedirs(outdir)
  with open(filepath, "rb") as f:
    data = f.read()

  # SCNK (song chunk) header ...
  num_songs = get_u32_be(data, 0x0C)
  offset = 0x20
  # SONG headers/data ...
  for song_num in range(num_songs):
    size = get_u32_be(data, offset+0x04)
    next_file_offset = get_u32_be(data, offset+0x08)
    offset += 0x20
    outpath = os.path.join(outdir, "%04d.song" % song_num)
    with open(outpath, "wb") as f:
      f.write(data[offset:offset+size])
    offset += next_file_offset

def extract_musyx_group(filepath):
  name = os.path.basename(filepath)
  outdir = "%s_extracted" % filepath
  if not os.path.isdir(outdir):
    os.makedirs(outdir)
  with open(filepath, "rb") as f:
    data = f.read()

  # main header ...
  offset = 0x20
  # musyx file headers/data ...
  while offset < len(data):
    ext = data[offset:offset+4].decode("ascii").lower()
    size = get_u32_be(data, offset+0x04)
    next_file_offset = get_u32_be(data, offset+0x08)
    offset += 0x20
    outpath = os.path.join(outdir, "%s.%s" % (name, ext))
    with open(outpath, "wb") as f:
      f.write(data[offset:offset+size])
    offset += next_file_offset

def extract_layer2(filepaths):
  for filepath in filepaths:
    ftype = int(os.path.basename(filepath))
    if ftype == 0:
      extract_musyx_songs(filepath)
    elif ftype in {1,2}:
      extract_musyx_group(filepath)

def main(argc=len(sys.argv), argv=sys.argv):
  if argc != 2:
    print("Usage: %s Multispq1.afs" % argv[0])
    return 1

  layer0_files = extract_layer0(argv[1])
  layer1_files = extract_layer1(layer0_files)
  layer2_files = extract_layer2(layer1_files)

  return 0

if __name__ == "__main__":
  main()
