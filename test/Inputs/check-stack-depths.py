"""Read a -format=text trace on stdin and check the StackDepthKB of its
DeclRead entries against LIMIT, the size in KB of each stack the reads ran on.

The reads of a recursion that went on on a new stack are written first, from
the deepest; when the recursion is back on the stack it started on, the depth
rises by more than half a stack. Every depth must be below LIMIT, and the reads
before that rise must reach more than 1024 KB below their own stack's base.

Usage: check-stack-depths.py LIMIT < trace.txt
"""

import sys

limit = int(sys.argv[1])
depths = [int(line.split("=")[1]) for line in sys.stdin
          if line.strip().startswith("StackDepthKB =")]
rise = next((i for i in range(1, len(depths))
             if depths[i] - depths[i - 1] > limit // 2), None)
if not depths or max(depths) >= limit:
    sys.exit("error: a depth is not below %d KB: %s" %
             (limit, max(depths) if depths else "no reads"))
if rise is None:
    sys.exit("error: no read returned to the first stack")
deepest = max(depths[:rise])
if deepest <= 1024:
    sys.exit("error: reads on the new stack reach only %d KB" % deepest)
print("reads=%d new-stack-deepest-kb=%d ok" % (len(depths), deepest))
