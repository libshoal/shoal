#!/usr/bin/env python

import sys
import os
sys.path.append(os.getenv('HOME') + '/bin/')

import tools

import subprocess
import re

OUTPUT='%s/papers/oracle/measurements/' % os.getenv('HOME')

def execute(cmd):

    print "Executing [cmd]"
    res = None

    try:

        # Execute process
        proc = subprocess.Popen(cmd, shell=True,
                                stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT)

        # Parse output
        while True:
            line = proc.stdout.readline()
            if line != '':
                print '[BENCH] %s' % line.rstrip()
                m = re.match('^Files are: (\S+) (\S+) (\S+)', line)
                if m:
                    print 'Output files are: %s %s %s' % (m.group(1), m.group(2), m.group(3))
                    res = (m.group(1), m.group(2), m.group(3))
            else:
                # Break if no line could be read
                break

        # Wait for process to terminate
        proc.wait()
        print 'benchmark terminate with return code %d' % proc.returncode
        return res

    except KeyboardInterrupt:
        print 'Killing benchmark'
        subprocess.Popen.kill(proc)
        print 'Done'

        return None

#WORKLOAD='soc-LiveJournal1'
WORKLOAD='twitter_rv'
MACHINE='sgs-r815-03'
#PROG='pagerank'
#PROG='triangle_counting'
PROG='hop_dist'

for conf in [ "", "-d", "-d -r", "-d -r -p", "-d -r -p -h" ]:
    res = execute('scripts/run_remote.sh %s ours %s pr "%s"' %
                  (PROG, WORKLOAD, conf))

    # total, comp, init
    conf_short = ''.join([ x for x in conf if re.match('[a-z]', x) ])

    for (num,name) in [(0, "total"), (1, "comp"), (2, "init")]:
        subprocess.check_call(['cp', res[num], '%s/%s_%s_%s_%s_%s' %
                               (OUTPUT, WORKLOAD, MACHINE, PROG, conf_short, name)])
