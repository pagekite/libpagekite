#!/usr/bin/python
#
# This little tool will embed a bunch of files as data blocks to be compiled
# into your binary.
#
import sys

def embed_file(fn):
    embeddable = ['{\n']
    with open(fn, 'r') as fd:
        data = [c for c in fd.read()]
        while data:
            chars = data[:40]
            embeddable.append('  %s,\n' % ', '.join(['0x%x' % ord(c)
                                                     for c in chars]))
            data[:40] = []
    embeddable.append('  0x00}')
    return ''.join(embeddable)


files_data = [(fn, embed_file(fn)) for fn in sys.argv[1:]]
for i, (fn, data) in enumerate(files_data):
    print 'const unsigned char embedded_file_data_%d[] = %s;' % (i, data) 
print
print """\
static const struct embedded_file {
  const char *name;
  const unsigned char *data;
} embedded_files[] = {\
"""
for i, (fn, data) in enumerate(files_data):
     print '  {"%s", embedded_file_data_%d},' % (fn.replace('"', '\\"'), i)
print '  {NULL, NULL}'
print '};'
