#!/usr/bin/env python3
"""Dump an MFC class's vtable layout from the x86 MFC42 binary.

The x86 build is the same sources as the AXP64 one, so its vftable symbols
(??_7CFile@@6B@ and friends) settle any question about virtual slot order
authoritatively — no guessing from header declaration order.

    python3 vtdump.py CFile            # -> slot-by-slot listing
    python3 vtdump.py CWnd 60

Slots whose entry resolves to an unrelated name are usually COMDAT folding:
identical empty bodies (Serialize/AssertValid/Dump) get merged by the linker
and the symbol that wins is arbitrary.  The slot is still the right one.
"""
import re, struct, sys, os, json

HERE = os.path.dirname(os.path.abspath(__file__))
DLL  = os.path.join(HERE, 'mfc42_x86.dll')
PDB  = os.path.join(HERE, 'mfc42_x86.pdb')
CACHE = '/tmp/claude-0/pub_x86.txt'

def publics():
    if not os.path.exists(CACHE):
        os.system(f'llvm-pdbutil dump --publics "{PDB}" > {CACHE} 2>&1')
    return open(CACHE, encoding='utf-8', errors='replace').read()

def main():
    cls = sys.argv[1] if len(sys.argv) > 1 else 'CFile'
    n   = int(sys.argv[2]) if len(sys.argv) > 2 else 30
    sym = f'??_7{cls}@@6B@'

    d = open(DLL, 'rb').read()
    pe = struct.unpack('<I', d[0x3c:0x40])[0]; oh = pe + 24
    base = struct.unpack('<I', d[oh+28:oh+32])[0]
    nsec = struct.unpack('<H', d[pe+6:pe+8])[0]
    ohsz = struct.unpack('<H', d[pe+20:pe+22])[0]
    sh = pe + 24 + ohsz; secs = []; secva = []
    for i in range(nsec):
        s = sh + 40*i
        vs, rva, raw, po = struct.unpack('<IIII', d[s+8:s+24])
        secs.append((rva, vs, po)); secva.append(rva)
    def r2o(r):
        for rva, vs, po in secs:
            if rva <= r < rva+vs: return po + (r - rva)

    rva2name = {}; name2rva = {}
    for m in re.finditer(r'S_PUB32 \[size = \d+\] `([^`]*)`\s*\n\s*flags = [^,]*, addr = (\d+):(\d+)',
                         publics()):
        nm, sec, off = m.group(1), int(m.group(2)), int(m.group(3))
        if 1 <= sec <= len(secva):
            r = secva[sec-1] + off
            rva2name.setdefault(r, nm); name2rva.setdefault(nm, r)

    r = name2rva.get(sym)
    if r is None:
        print(f'{sym}: not found'); return
    o = r2o(r)
    print(f'=== {sym} (rva {r:#x}) ===')
    for i in range(n):
        e = struct.unpack('<I', d[o+4*i:o+4*i+4])[0]
        nm = rva2name.get(e - base)
        if not nm:
            print(f'  [{i:2}] {e:#x}  <end of vtable>'); break
        print(f'  [{i:2}] {nm}')

if __name__ == '__main__':
    main()
