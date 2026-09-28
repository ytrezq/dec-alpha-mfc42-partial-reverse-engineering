# v0.1.0 — AXP64 MFC42, partial

A reimplementation of `MFC42.DLL` for **Windows AXP64** — 64-bit DEC Alpha,
PE32+ machine type `0x0284` — plus the analysis that made it possible: the
ordinal map, the vtable layouts, and the structure offsets.

Enough of MFC 4.2 to run a real application: 42 implemented ordinals covering
`CWinApp`, `CDocument`, `CView`, `CListView`, `CFrameWnd`, `CMDIFrameWnd`,
`CSplitterWnd`, `CToolBar`, `CStatusBar`, `CImageList` and `CFile`, with
message maps, owner-draw reflection through `OnChildNotify` and `DrawItem`,
and the window procedure.

## Assets

`prebuilt/MFC42.dll` — built from `mfc42.c` by the
[cross toolchain](https://github.com/ytrezq/dec-alpha-cross-binutils), image
base `0x5F400000`. Load it alongside the
[AXP64 runtime](https://github.com/ytrezq/test-windows-dec-alpha-builds):

```sh
wine winhost.exe -k <your-axp64-mfc-app.exe> MFC42.dll \
    guest/KERNEL32.dll guest/MSVCRT.dll guest/USER32.dll guest/GDI32.dll \
    guest/ADVAPI32.dll guest/SHELL32.dll guest/COMDLG32.dll guest/COMCTL32.dll
```

## Provenance

Original code, but not written from a published API, and that distinction is
worth stating. The Win32 layer in the runtime repository was written against
the *published* Win32 API — the clean-room position Wine occupies, which is
why Wine ships in distributions at all.

MFC 4.2 does not offer that. It exports by ordinal with no public mapping,
and the layout its clients depend on — vtable slot order, structure offsets,
the message-map record — is undocumented. Recovering them meant analysing
Microsoft's shipped binary and its public PDB: reverse engineering for
interoperability, the same thing Wine does for undocumented interfaces. The
result is still code written from scratch against what the analysis showed,
but it is not the same as implementing a documented API.

No Microsoft binary is redistributed here. The ordinal map in `docs/` is
analysis output — a correspondence between ordinals and the documented names
of a published API.
