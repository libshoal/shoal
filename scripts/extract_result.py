#!/usr/bin/env python

import sys
import os
sys.path.append(os.getenv('HOME') + '/bin/')

import re
import fileinput

# Time for copy: 228.052000
# running time=3207.233000

total=None
copy=None

lines = 0

# Correct output for hop_dist
# --------------------------------------------------
hd_res = {
    0:  0,
    1:  15,
    2:  17,
    3:  18,
    4:  16,
    5:  17,
    6:  16,
    7:  14,
    8:  15,
    9:  16
    }

# Correct output for pagernak
# --------------------------------------------------
# pr_res = {
#     0: 0.000000002,
#     1: 0.000000006,
#     2: 0.000000006,
#     3: 0.000000003
#     }
pr_res = {
    0: 0.000001174,
    1: 0.000004149,
    2: 0.000002173,
    3: 0.000001640,
    }

def verify_hop_dist(line):
    l = re.match('^dist\[([0-9]*)\] = ([0-9]*)', line)
    if l:
        res = hd_res.get(int(l.group(1)), None)
        if not res == int(l.group(2)):
            print 'Result for', int(l.group(1)), 'is', int(l.group(2)), 'expecting', res
        return res == int(l.group(2))
    else:
        return True

# Sample output line:
# rank[0] = 0.000000002
def verify_pagerank(line):
    l = re.match('^rank\[([0-9]*)\] = ([0-9.]*)', line)
    if l:
        res = pr_res.get(int(l.group(1)), None)
        print 'Result for', int(l.group(1)), 'is', float(l.group(2)), 'expecting', res
        return res == float(l.group(2))
    else:
        return True

result = True

while 1:
    line = sys.stdin.readline()
    if not line: break
    print line.rstrip()
    lines += 1
    # Extract time for copy
    l = re.match('Time for copy: ([0-9.]*)', line)
    if l:
        copy = float(l.group(1))
    # Extract runtime
    l = re.match('running time=([0-9.]*)', line)
    if l:
        total = float(l.group(1))

    # check result output
    # --------------------------------------------------
    result = result and \
        verify_hop_dist(line) and \
        verify_pagerank(line)

if total and copy:
    print 'total:    %10.5f' % total
    print 'copy:     %10.5f' % copy
    print 'comp:     %10.5f' % (total-copy)

print 'lines processed:', lines, '- output is:', result
