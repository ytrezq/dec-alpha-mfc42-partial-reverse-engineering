# Prebuilt MFC42

`MFC42.dll` — Windows AXP64 (PE32+, machine `0x0284`), image base
`0x5F400000`, 42 implemented ordinals plus the stub table, built from
`mfc42.c` in this repository by the
[cross toolchain](https://github.com/ytrezq/dec-alpha-cross-binutils).

It is loaded alongside the
[AXP64 runtime](https://github.com/ytrezq/test-windows-dec-alpha-builds):

```sh
wine winhost.exe -k <your-axp64-mfc-app.exe> MFC42.dll \
    guest/KERNEL32.dll guest/MSVCRT.dll guest/USER32.dll guest/GDI32.dll \
    guest/ADVAPI32.dll guest/SHELL32.dll guest/COMDLG32.dll guest/COMCTL32.dll
```

## Provenance

Original code, but not written from a published API: MFC 4.2 exports by
ordinal with no public mapping, and the layout its clients depend on — vtable
slot order, structure offsets, the message-map record — is undocumented, so
it was recovered by analysing Microsoft's shipped binary and its public PDB.
Reverse engineering for interoperability, the same thing Wine does for
undocumented interfaces. See the repository README.

No Microsoft binary is redistributed here.
