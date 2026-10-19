"""Write module Deep to DIR: typedefs T0 = int, T1 = T0, ... TN; P, a pointer
type DEPTH levels deep (when DEPTH is not 0); and U0 = P, U1 = U0, ... UN.

Usage: make-deep-module.py DIR N DEPTH
"""

import os
import sys

out, chain, depth = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
os.makedirs(out, exist_ok=True)
with open(os.path.join(out, "deep.h"), "w") as header:
    header.write("typedef int T0;\n")
    for i in range(1, chain + 1):
        header.write("typedef T%d T%d;\n" % (i - 1, i))
    if depth:
        header.write("typedef int %s P;\n" % ("*" * depth))
        header.write("typedef P U0;\n")
        for i in range(1, chain + 1):
            header.write("typedef U%d U%d;\n" % (i - 1, i))
with open(os.path.join(out, "module.modulemap"), "w") as modulemap:
    modulemap.write('module Deep { header "deep.h" export * }\n')
