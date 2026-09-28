#!/usr/bin/env python3
"""Bracket unknown AXP64 MFC42 ordinals against the x86 export list.

Anchors: every EXACT line of mfc42_ordinals.txt (the two lists agree locally)
plus the ordinals proved by running depends.exe.  Because the AXP64 export
list is an order-preserving subsequence of the x86 one, two anchors bound
everything between them; when only one x86 export lies in the gap the
ordinal is pinned exactly.
"""
import json, re, sys

X86 = {int(k): v for k, v in json.load(open('ord2name_x86.json')).items()}
N2O = {}
for o, n in sorted(X86.items()):
    N2O.setdefault(n, o)

ANCHOR = {}
for line in open('/home/claude/work/mfc42_ordinals.txt'):
    m = re.match(r'\s*(\d+)\s+EXACT\s+(\S+)\s*$', line)
    if m:
        o = N2O.get(m.group(2))
        if o: ANCHOR[int(m.group(1))] = o

def bracket(t):
    lo = max([a for a in ANCHOR if a < t], default=None)
    hi = min([a for a in ANCHOR if a > t], default=None)
    if lo is None or hi is None: return None
    lox, hix = ANCHOR[lo], ANCHOR[hi]
    cands = [(o, X86[o]) for o in range(lox + 1, hix) if o in X86]
    return lo, lox, hi, hix, cands

def show(t, maxn=30, filt=None):
    r = bracket(t)
    if not r:
        print(f"=== AXP64 {t} === outside the anchor range"); return
    lo, lox, hi, hix, c = r
    if filt:
        c = [(o, n) for o, n in c if re.search(filt, n)]
    print(f"=== AXP64 {t} ===  axp {lo}->x86 {lox} .. axp {hi}->x86 {hix}"
          f"  ({len(c)} candidates{' matching '+filt if filt else ''})")
    for o, n in c[:maxn]: print(f"    {o}  {n}")
    if len(c) > maxn: print(f"    ... +{len(c)-maxn} more")
    print()

if __name__ == '__main__':
    args = sys.argv[1:]
    filt = None
    if args and args[0].startswith('/'):
        filt = args.pop(0)[1:]
    for a in args: show(int(a), filt=filt)
