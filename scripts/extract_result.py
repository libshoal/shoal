#!/usr/bin/env python

import sys
import os
sys.path.append(os.getenv('HOME') + '/bin/')

import re
import fileinput

# Time for copy: 228.052000
# running time=3207.233000

for line in fileinput.input():
    print line.rstrip()
    l = re.match('Time for copy: ([0-9.]*)', line)
    if l:
        copy = float(l.group(1))
    l = re.match('running time=([0-9.]*)', line)
    if l:
        total = float(l.group(1))


print 'total:    %10.5f' % total
print 'copy:     %10.5f' % copy
print 'comp:     %10.5f' % (total-copy)
