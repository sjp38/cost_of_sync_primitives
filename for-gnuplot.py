#!/usr/bin/env python

import os
import sys

sys.path.append(os.environ['HOME'] + '/lazybox/scripts/gen_report')
import ltldat

output = ""
for line in sys.stdin:
    if line.startswith('#'):
        continue
    output += line

records = output.strip().split('\n\n\n')
for r in records:
    lines = r.split('\n')
    print "\n"
    print lines[0] + "-issues"
    for l in lines[1:]:
        fields = l.split()
        print fields[0], fields[1]
    print "\n"
    print lines[0] + "-succ"
    for l in lines[1:]:
        fields = l.split()
        print fields[0], fields[2]
