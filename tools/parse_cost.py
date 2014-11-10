#!/usr/bin/env python

import sys
import fileinput
import re
import logging
import os
sys.path.append(os.getenv('HOME') + '/bin/')

import tools

logging.basicConfig(level=logging.INFO)

# Map strings to something more readable
formula_lookup = {
    'LOOP_CONSTANT': 'k',
    'LOOP_NODES': 'N',
    'LOOP_EDGES_NBS': 'E',
    'LOOP_EDGES': 'E'
}

# Output arrays to generate cost string
output = ({}, {})
arrays =[]
program = 'UNKNOWN'

# Output file for array cost (C header file)
COST_FILE='/tmp/shl_array_cost.h'

def gm_extract_cost(line):
    """
    Extract array access costs from GM program

    """
    # Match GM output line
    m = re.match((
        '^arr access:\s*\[(\w+)\s*\]'
        '\s*indexed\s*\[([X ])\]'
        '\s*write\s*\[([X ])\]'
        '\s*cost\s*\[(.*)\]'
    ), line)

    if m:
        # Debug output
        logging.debug(line)
        logging.debug(('    ', m.group(1), '    ', m.group(2), '    ', m.group(3), '    ', m.group(4)))

        write = 1 if (m.group(3).strip() == "X") else 0
        arr = m.group(1)
        cost = m.group(4)

        if not arr in arrays:
            arrays.append(arr)

        if not arr in output[write]:
            output[write][arr] = ''
        else:
            output[write][arr] += (' + ');

        output[write][arr] += '*'.join([ formula_lookup[s.split('=')[1]] for s in cost.split()[1:] ])

    # Find name of algorithm that is translated
    m = re.match('^../../bin/gm_comp.* ([a-zA-Z-_]+)\.gm$', line)
    if m:
        global program
        program = m.group(1)
        logging.debug(('Found algorithm name:', m.group(1)))


def main():
    """
    Parse GM output file for cost functions and print

    """

    # Parse input
    for line in fileinput.input():
        gm_extract_cost(line.rstrip())

    # Human-readable output
    # --------------------------------------------------
    # Output result (reads)
    print 'reads:'
    for (key, value) in output[0].items():
        print key, '->', value

    # Output result (writes)
    print 'writes:'
    for (key, value) in output[1].items():
        print key, '->', value

    # LaTeX output
    # --------------------------------------------------
    print
    print ('\\begin{center}\n'
           '\\begin{table}[h!]\n'
           '\\begin{tabular}{lll}\n'
           '\\hline\n'
           'array & \#reads & \#writes \\\\\n'
           '\\hline')

    for a in arrays:
        if not a in output[0]:
            output[0][a] = ''
        if not a in output[1]:
            output[1][a] = ''
        a_p = '\\texttt{%s}' % a
        print a_p.replace('shl__G_', '').replace('_', '\\_'), \
            '& $', output[0][a].replace('*',''), '$', \
            '& $', output[1][a].replace('*', ''), '$ \\\\'

    print ('\\hline\n'
           '\\end{tabular}\n'
           '\\caption{%s arrays access cost as extracted by \\project}\n'
           '\\end{table}\n'
           '\\end{center}') % program.replace('_', '\\_')


    # C header output
    # --------------------------------------------------
    print

    f = open(COST_FILE, 'w')
    f.write('#ifndef SHL_COST\n')
    f.write('#define SHL_COST\n')
    f.write('\n')
    f.write('// Automatically generated from <shoal>/tools/parse_cost.py\n')

    for a in arrays:

        # Set default in case array is not written/read
        if not a in output[0]:
            output[0][a] = '0'
        if not a in output[1]:
            output[1][a] = '0'

        f.write('#define %s_wr "%s"\n' % (a, output[0][a]))
        f.write('#define %s_rd "%s"\n' % (a, output[1][a]))

    f.write('#endif /* SHL_COST */\n')
    f.close()

    print

if __name__ == "__main__":
    main()
