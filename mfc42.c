/* mfc42.dll for Windows AXP64 — a reimplementation in DEC Alpha code.
 *
 * Neither Wine nor ReactOS implements MFC42, and no AXP64 build exists, so
 * the classes depends.exe needs are rebuilt here.  Two things must match the
 * original exactly, because depends.exe was compiled against the real
 * headers and its code carries the offsets and vtable slots baked in:
 *
 *   - object layouts (member offsets), and
 *   - virtual function slot order.
 *
 * Every layout below is justified either by MFC 4.2's documented member
 * order or, better, by what depends.exe's own machine code does with it.
 * Example, the first MFC call it ever makes (depends.exe:0x441810):
 *
 *      bsr    ra, <#979>        ; the state getter
 *      ldl    t1, 40(v0)        ; read the dword holding a byte member
 *      andnot t1, 0xff, t3      ; clear low byte      (Alpha has no BWX here)
 *      and    t2, 0xff, t4      ; low byte of arg0
 *      or     t3, t4, t1
 *      stl    t1, 40(v0)        ; store back  -> +40 is a BYTE
 *
 * which pins #979 as AfxGetModuleState and +0x28 as m_bDLL.
 *
 * A function is wired to its export ordinal by the /*ORD n*(/ marker in front
 * of it; genmfc.py reads those and emits tracing stubs for every ordinal that
 * is not implemented yet, so the module always exports all 433.
 */
typedef unsigned char      BYTE;
typedef unsigned short     WORD;
typedef unsigned int       DWORD;
typedef int                BOOL;
typedef void              *HANDLE;
typedef void              *HINSTANCE;
typedef void              *HWND;
typedef const char        *LPCSTR;
typedef char              *LPSTR;
typedef long long          LL;
typedef unsigned long long ULL;
#define NULL ((void*)0)

/* ---- host / kernel32 imports ----------------------------------------- */
extern void  OutputDebugStringA(LPCSTR);
extern void *__sys_mem(unsigned long size);
extern LL    __mfc_trace(LL ord, LL a0, LL a1, LL a2, LL a3, LL a4,
                         LL a5, LL a6, LL a7);
/* run a guest routine under the host's fault guard (a stand-in for the SEH
 * the application would use around its own analysis) */
extern LL    __mfc_guarded_call(void *fn, LL a0, LL a1, LL a2);

/* ---- USER32 (our AXP64 one) ------------------------------------------ */
typedef unsigned long long WPARAM_T;
typedef long long          LPARAM_T;
extern DWORD RegisterClassA(const void *wc);
extern HWND  CreateWindowExA(DWORD ex, LPCSTR cls, LPCSTR name, DWORD style,
                             int x, int y, int w, int h, HWND parent,
                             void *menu, HINSTANCE inst, void *param);
extern BOOL  ShowWindow(HWND, int);
extern BOOL  UpdateWindow(HWND);
extern LL    DefWindowProcA(HWND, DWORD, WPARAM_T, LPARAM_T);
extern int   GetMessageA(void *msg, HWND, DWORD, DWORD);
extern BOOL  TranslateMessage(const void *msg);
extern LL    DispatchMessageA(const void *msg);
extern void  PostQuitMessage(int);
extern void *LoadCursorA(HINSTANCE, LPCSTR);
extern void *CreateSolidBrush(DWORD);
extern BOOL  GetClientRect(HWND, void *rect);
extern BOOL  MoveWindow(HWND, int, int, int, int, BOOL);
extern LL    SendMessageA(HWND, DWORD, WPARAM_T, LPARAM_T);

/* resources out of our own guest image (the host walks the .rsrc tree) */
extern int   __gu_LoadStringFromModule(HINSTANCE mod, DWORD id, char *buf, DWORD cap);
extern void *__gu_LoadMenuFromModule(HINSTANCE mod, DWORD id);
extern void *__gu_ImageListFromBitmap(HINSTANCE mod, DWORD id, int cx, DWORD crMask);
extern int   DrawMenuBar(HWND);
extern void *__gu_ToolbarFromModule(HINSTANCE mod, DWORD id, HWND parent,
                                    DWORD ctlID, int *pHeight);

/* ---- tiny local runtime ---------------------------------------------- */
static unsigned long m_strlen(const char *s){ const char *p=s; while(*p)p++; return (unsigned long)(p-s); }
static void m_memset(void *d,int c,unsigned long n){ BYTE *p=d; while(n--) *p++=(BYTE)c; }
static void m_memcpy(void *d,const void *s,unsigned long n){ BYTE *p=d; const BYTE *q=s; while(n--) *p++=*q++; }

/* compact diagnostic: "<label> <hex> <hex>" on the debug stream */
static void dbg2(const char *label, ULL a, ULL b)
{
    char m[160]; int k = 0; const char *hx = "0123456789abcdef";
    for (const char *p = label; *p && k < 90; p++) m[k++] = *p;
    m[k++] = ' ';
    for (int i = 60; i >= 0; i -= 4) m[k++] = hx[(a >> i) & 15];
    m[k++] = ' ';
    for (int i = 60; i >= 0; i -= 4) m[k++] = hx[(b >> i) & 15];
    m[k++] = '\n'; m[k] = 0; OutputDebugStringA(m);
}


/* ======================================================================
 * AFX_MODULE_STATE
 *
 *   0x00  vptr                       (CNoTrackObject: virtual destructor)
 *   0x08  m_pCurrentWinApp
 *   0x10  m_hCurrentInstanceHandle
 *   0x18  m_hCurrentResourceHandle
 *   0x20  m_lpszCurrentAppName
 *   0x28  m_bDLL  (BYTE)             <- proved by the code above
 *   0x29  m_bSystem (BYTE)
 *   0x2C  m_fRegisteredClasses (DWORD)
 *
 * The trailing slack keeps any member we have not identified yet inside
 * valid zeroed memory, so an unknown offset reads 0 instead of faulting.
 * ==================================================================== */
typedef struct AFX_MODULE_STATE {
    void        *vptr;                      /* 0x00 */
    void        *m_pCurrentWinApp;          /* 0x08 */
    HINSTANCE    m_hCurrentInstanceHandle;  /* 0x10 */
    HINSTANCE    m_hCurrentResourceHandle;  /* 0x18 */
    LPCSTR       m_lpszCurrentAppName;      /* 0x20 */
    BYTE         m_bDLL;                    /* 0x28 */
    BYTE         m_bSystem;                 /* 0x29 */
    BYTE         m_bReserved[2];
    DWORD        m_fRegisteredClasses;      /* 0x2C */
    BYTE         m_slack[1024];             /* room for the not-yet-mapped */
} AFX_MODULE_STATE;

static AFX_MODULE_STATE g_moduleState;
static AFX_MODULE_STATE g_threadState;

/* AfxGetModuleState — every MFC entry point begins here. */
/*ORD 979*/ void *AfxGetModuleState(void) { return &g_moduleState; }

/* The per-thread state has the same "big and zeroed" treatment. */
void *AfxGetThreadState(void) { return &g_threadState; }

/* ======================================================================
 * Virtual dispatch into the application's own classes.
 *
 * depends.exe builds its vtables statically in its own image: the slots it
 * overrides hold its code, the rest hold thunks to our exports.  So MFC
 * never has to synthesise a vtable for the app object — it only has to call
 * the right slot.  The slot numbers below were read straight out of
 * depends.exe's CWinApp-derived vtable at 0x4462b8, where the MFC-owned
 * entries identify themselves by ordinal and pin the order:
 *
 *   [22] InitInstance   (overridden)     [26] OnIdle            #4022
 *   [23] Run            (overridden)     [27] IsIdleMessage     #3402
 *   [24] PreTranslateMessage  #4629      [28] ExitInstance      (overridden)
 *   [25] PumpMessage          #4646      [35] InitApplication   #3263
 *
 * which is exactly CWinThread's declaration order in MFC 4.2.
 * ==================================================================== */
#define VS_INITINSTANCE    22
#define VS_RUN             23
#define VS_EXITINSTANCE    28
#define VS_INITAPPLICATION 35

typedef LL (*VFN0)(void *self);
static LL vcall0(void *obj, int slot)
{
    if (!obj) return 0;
    void **vt = *(void ***)obj;
    if (!vt || !vt[slot]) return 0;
    return ((VFN0)vt[slot])(obj);
}

static void *AfxGetApp(void) { return g_moduleState.m_pCurrentWinApp; }

/* ---- CWinApp ---------------------------------------------------------
 * The constructor's only job that matters this early is to publish the
 * object as "the" application, which is what AfxGetApp and AfxWinMain read.
 */
/*ORD 518*/ void *CWinApp_ctor(void *self, LPCSTR lpszAppName)
{
    g_moduleState.m_pCurrentWinApp = self;
    if (lpszAppName) g_moduleState.m_lpszCurrentAppName = lpszAppName;
    return self;
}

/* ======================================================================
 * CString
 *
 * A CString is one pointer to the characters; the bookkeeping sits in a
 * CStringData header immediately in front of them.  depends.exe's own code
 * pins the layout exactly (depends.exe:0x40c550):
 *
 *      ldq  t1,440(s0)        ; the CString member  -> m_pchData
 *      ldl  t1,-8(t1)         ; its length          -> nDataLength at -8
 *
 * so the header is { long nRefs; int nDataLength; int nAllocLength; } with
 * the characters following at +12: nRefs -12, nDataLength -8, nAlloc -4.
 * ==================================================================== */
typedef struct { int nRefs; int nDataLength; int nAllocLength; } CStringData;

/* The shared empty string.  nRefs = -1 marks it as never-freed, which is
 * what MFC does for the one global empty string every CString starts at. */
static struct { CStringData hdr; char data[4]; } g_empty = { { -1, 0, 0 }, { 0,0,0,0 } };
#define EMPTY_STR (g_empty.data)

/* A small bump allocator: MFC string traffic here is short-lived and the
 * process is not expected to outlive the analysis. */
static char  *heap_p; static unsigned long heap_left;
static void *mfc_alloc(unsigned long n)
{
    n = (n + 15) & ~15UL;
    if (n > heap_left) {
        unsigned long chunk = n > 0x100000UL ? n : 0x100000UL;
        heap_p = __sys_mem(chunk);
        heap_left = heap_p ? chunk : 0;
        if (!heap_p) return NULL;
    }
    char *p = heap_p; heap_p += n; heap_left -= n;
    return p;
}

static char *str_alloc(unsigned long len)
{
    CStringData *d = mfc_alloc(sizeof(CStringData) + len + 1);
    if (!d) return EMPTY_STR;
    d->nRefs = 1; d->nDataLength = (int)len; d->nAllocLength = (int)len;
    char *s = (char *)d + sizeof(CStringData);
    s[len] = 0;
    return s;
}
static char *str_dup(const char *src, unsigned long len)
{
    char *s = str_alloc(len);
    if (s != EMPTY_STR && len) m_memcpy(s, src, len);
    return s;
}
/* set a CString member (by address) to a copy of src */
static void str_set(char **pstr, const char *src)
{
    if (!pstr) return;
    *pstr = (src && *src) ? str_dup(src, m_strlen(src)) : EMPTY_STR;
}

/* CString::CString() — every CString starts life pointing at the shared
 * empty string, which is what makes GetLength() safe on a fresh object. */
/*ORD 501*/ void *CString_ctor(char **self) { if (self) *self = EMPTY_STR; return self; }

/* CString::CString(LPCTSTR) — depends.exe's document builds two of its own
 * members this way (doc+208 and doc+216, from a "" literal in its data), and
 * they stayed null for as long as this was a stub, which is what the analysis
 * then tripped over. */
/*ORD 497*/ void *CString_ctor_psz(char **self, LPCSTR src)
{ if (self) str_set(self, src); return self; }

/* CString::~CString() — the bump allocator never frees, so this only has to
 * leave the object in a safe state. */
/*ORD 690*/ void CString_dtor(char **self) { if (self) *self = EMPTY_STR; }

/* CString::operator=(LPCTSTR) and operator=(const CString&).  Both return a
 * reference to the target, which on this ABI means returning `this`. */
/*ORD 741*/ void *CString_assign_psz(char **self, LPCSTR src)
{ if (self) str_set(self, src); return self; }
/*ORD 739*/ void *CString_assign_str(char **self, char **src)
{ if (self) str_set(self, (src && *src) ? *src : NULL); return self; }

/* CWinApp::GetProfileStringA(section, entry, default) returns a CString by
 * value.  On this ABI a member function returning a class takes `this` in a0
 * and the hidden return buffer in a1 — visible in the call depends.exe makes
 * with (app, stack slot, "External Viewer", ...). */
/*ORD 2859*/ void *CWinApp_GetProfileString(void *self, char **ret, LPCSTR sec,
                                            LPCSTR ent, LPCSTR def)
{ if (ret) str_set(ret, def); return ret; }

/* ======================================================================
 * CCommandLineInfo (MFC 4.2 layout, widened to 64-bit)
 *   0x00 vptr          0x14 m_nShellCommand
 *   0x08 m_bShowSplash 0x18 m_strFileName    0x20 m_strPrinterName
 *   0x0C m_bRunEmbedded0x28 m_strDriverName  0x30 m_strPortName
 *   0x10 m_bRunAutomated
 * ==================================================================== */
typedef struct {
    void *vptr;            /* 0x00 */
    int   m_bShowSplash;   /* 0x08 */
    int   m_bRunEmbedded;  /* 0x0C */
    int   m_bRunAutomated; /* 0x10 */
    int   m_nShellCommand; /* 0x14 */
    char *m_strFileName;   /* 0x18 */
    char *m_strPrinterName;/* 0x20 */
    char *m_strDriverName; /* 0x28 */
    char *m_strPortName;   /* 0x30 */
} CCommandLineInfo;

/*ORD 288*/ void *CCommandLineInfo_ctor(CCommandLineInfo *self)
{
    if (!self) return self;
    self->m_bShowSplash = 1;
    self->m_bRunEmbedded = 0;
    self->m_bRunAutomated = 0;
    self->m_nShellCommand = 0;              /* FileNew */
    self->m_strFileName = EMPTY_STR;
    self->m_strPrinterName = EMPTY_STR;
    self->m_strDriverName = EMPTY_STR;
    self->m_strPortName = EMPTY_STR;
    return self;
}

/* CWinApp::SetRegistryKey — remembered so profile calls have somewhere to
 * claim to read from; nothing is persisted in this build. */
static LPCSTR g_registryKey;
/*ORD 5292*/ void CWinApp_SetRegistryKey(void *self, LPCSTR key) { g_registryKey = key; }

/* CWinApp::GetProfileInt(section, entry, default) — no registry here, so the
 * application always sees its own defaults. */
/*ORD 2858*/ int CWinApp_GetProfileInt(void *self, LPCSTR sec, LPCSTR ent, int def)
{ return def; }

/* CWinApp::InitApplication — in a single-instance app this only sets up the
 * document-template list, which is empty until InitInstance runs. */
/*ORD 3263*/ int CWinApp_InitApplication(void *self) { return 1; }

/* ======================================================================
 * CWnd / CFrameWnd — the window itself
 *
 * CWnd::m_hWnd is the first data member of CWnd, so it sits at
 * sizeof(CCmdTarget) — and that is measured, not assumed.  Scanning every
 * USER32 call in depends.exe whose first argument is an HWND, the HWND is
 * loaded from +64 in 112 places (other offsets are CWnds embedded inside
 * bigger objects).  The x86 MFC42 agrees at exactly half: its CWnd methods
 * do `push 0x20(%esi)`.  The same x86 binary shows CWnd ending at 0x40 and
 * CView::m_pDocument at 0x40 (`and $0,0x40(%eax)` while detaching views),
 * so on AXP64 the view's document pointer is at 128.
 * (An earlier guess of 32 had been storing HWNDs into CCmdTarget's OLE
 * fields, where the application never looked.)
 * ==================================================================== */
#define OFF_M_HWND      64
#define OFF_M_PDOCUMENT 128
#define WM_DESTROY 0x0002
#define WM_CLOSE   0x0010
#define SW_SHOW    5

/* Every window this library creates is remembered here, so a CWnd can be
 * resolved even when the application has reused its m_hWnd slot for its own
 * bookkeeping (depends.exe does exactly that on the module-list view). */
#define MAXOBJW 64
static struct { void *obj; HWND h; } g_objWnd[MAXOBJW];
static int g_nObjWnd;
static void  cwnd_set_hwnd(void *self, HWND h)
{
    if (!self) return;
    *(HWND *)((char *)self + OFF_M_HWND) = h;
    for (int i = 0; i < g_nObjWnd; i++)
        if (g_objWnd[i].obj == self) { g_objWnd[i].h = h; return; }
    if (g_nObjWnd < MAXOBJW) { g_objWnd[g_nObjWnd].obj = self;
                               g_objWnd[g_nObjWnd].h = h; g_nObjWnd++; }
}
static HWND  cwnd_hwnd(void *self)
{
    if (!self) return NULL;
    for (int i = 0; i < g_nObjWnd; i++)
        if (g_objWnd[i].obj == self) return g_objWnd[i].h;
    return *(HWND *)((char *)self + OFF_M_HWND);
}

/* ======================================================================
 * Message maps
 *
 * depends.exe carries its own message maps in its image; the extraction of
 * its main frame map (at 0x44bfe0) shows the shape MFC 4.2 uses, widened to
 * 64-bit:
 *
 *   AFX_MSGMAP        { const AFX_MSGMAP *pBaseMap; const ENTRY *lpEntries; }
 *   AFX_MSGMAP_ENTRY  { UINT nMessage, nCode, nID, nLastID;
 *                       UINT_PTR nSig; AFX_PMSG pfn; }      = 32 bytes
 *
 * and its entries name the handlers directly, e.g.
 *      nMessage 0x0001 (WM_CREATE) -> depends.exe:0x426920
 *      nMessage 0x0005 (WM_SIZE)   -> depends.exe:0x426a50
 * A class hands back its map through GetMessageMap, which is virtual slot 12
 * (inside the CCmdTarget block, so it is the same slot for every derived
 * class — read off depends.exe's own CWinApp vtable).
 * ==================================================================== */
typedef struct AFX_MSGMAP_ENTRY {
    DWORD nMessage, nCode, nID, nLastID;
    ULL   nSig;
    LL  (*pfn)(void);
} AFX_MSGMAP_ENTRY;
/* Reading depends.exe's own map at 0x44BCB0 settles the first field: it holds
 * 0x425eb0, which is inside depends.exe's .text, so this MFC revision stores
 * a *function* that returns the base map, not a pointer to it. */
typedef struct AFX_MSGMAP {
    const struct AFX_MSGMAP *(*pfnGetBaseMap)(void);
    const AFX_MSGMAP_ENTRY  *lpEntries;
} AFX_MSGMAP;

#define VS_GETMESSAGEMAP 12

/* The base maps MFC itself owns.  depends.exe's maps chain into these by
 * ordinal as *data*, so they must be real structures, not thunks. */
static const AFX_MSGMAP_ENTRY g_noEntries[1] = { { 0,0,0,0,0,0 } };
const AFX_MSGMAP AfxBaseMsgMap = { NULL, g_noEntries };
static int in_module(const void *p)      /* cheap sanity filter for map walks */
{ ULL v=(ULL)(unsigned long)p; return v > 0x10000 && v < 0x80000000ULL; }

/* ---- HWND -> CWnd ----------------------------------------------------- */
#define MAXWND 256
static struct { HWND h; void *wnd; } g_wndMap[MAXWND];
static int   g_nWnd;
static void *g_pendingWnd;                 /* set across CreateWindowEx */

static void  wnd_attach(HWND h, void *w)
{ if (g_nWnd < MAXWND) { g_wndMap[g_nWnd].h = h; g_wndMap[g_nWnd].wnd = w; g_nWnd++; } }
static void *wnd_from_handle(HWND h)
{ for (int i = 0; i < g_nWnd; i++) if (g_wndMap[i].h == h) return g_wndMap[i].wnd; return NULL; }

/* forward: CRuntimeClass is defined with the document/view chain below */
typedef struct CRuntimeClass CRuntimeClass;

static void *g_curDoc, *g_childFrame;

/* CCreateContext — what MFC hands a frame so it can build its views.
 * lpCreateParams of the CREATESTRUCT points at it. */
typedef struct {
    CRuntimeClass *m_pNewViewClass;   /* 0x00 */
    void          *m_pCurrentDoc;     /* 0x08 */
    void          *m_pNewDocTemplate; /* 0x10 */
    void          *m_pLastView;       /* 0x18 */
    void          *m_pCurrentFrame;   /* 0x20 */
} CCreateContext;
static CCreateContext g_ctx;

/* Comparing depends.exe's two frame vtables identifies the slot: CMainFrame
 * carries MFC's CMDIFrameWnd::OnCreateClient (#3778) at 57, while CChildFrame
 * overrides it (0x402410) — that override is where its panes get built. */
#define VS_ONCREATECLIENT 57
typedef LL (*PFN_OCC)(void *self, void *lpcs, void *ctx);

/* the one document template depends.exe registers */
static struct {
    void *self; DWORD nID;
    CRuntimeClass *pDoc, *pFrame, *pView;
} g_tmpl;

/* A pane belongs to the splitter that created it: depends.exe builds two
 * (tree + import/export lists, then the module list + log), so a pane can
 * only be found by asking the right splitter for the right cell. */
static struct { void *wnd; HWND h; DWORD id; void *split; int row, col; } g_pane[16];
static int  g_nPane;

static int g_noOwnerDraw = 0;   /* keep LVS_OWNERDRAWFIXED: depends paints */
static HWND g_toolBar; static int g_toolBarH;
static void frame_layout(void);
static void pane_layout(void);
static HWND g_splitParent;   /* the MDI child window hosting the panes */

typedef LL (*PMSG_CREATE)(void *self, void *lpcs);
typedef LL (*PMSG_VOID)(void *self);
typedef LL (*PMSG_SIZE)(void *self, DWORD type, int cx, int cy);
typedef LL (*PMSG_GEN)(void *self, ULL wp, LL lp);

/* MFC reflects a control's notification back to the control's own CWnd, by
 * re-dispatching it as (message + WM_REFLECT_BASE) with the notification code
 * in the entry's nCode.  depends.exe's tree asks for every label this way. */
#define WM_REFLECT_BASE 0x0000BC00
#define WM_NOTIFY_      0x004E
typedef LL (*PMSG_NOTIFY)(void *self, void *pNMHDR, LL *pResult);

/* Walk the map chain for a handler of nMessage. */
static const AFX_MSGMAP_ENTRY *find_entry(const AFX_MSGMAP *map, DWORD msg)
{
    for (int depth = 0; map && in_module(map) && depth < 16; depth++) {
        const AFX_MSGMAP_ENTRY *e = map->lpEntries;
        if (e && in_module(e))
            for (; e->nMessage || e->nSig || e->pfn; e++)
                if (e->nMessage == msg && e->nID == 0) return e;
        if (!map->pfnGetBaseMap || !in_module((const void *)map->pfnGetBaseMap)) break;
        map = map->pfnGetBaseMap();          /* it is a call, not a deref */
    }
    return NULL;
}

/* find an entry matching both the message and the notification code */
static const AFX_MSGMAP_ENTRY *find_entry_code(const AFX_MSGMAP *map, DWORD msg, DWORD code)
{
    for (int depth = 0; map && in_module(map) && depth < 16; depth++) {
        const AFX_MSGMAP_ENTRY *e = map->lpEntries;
        if (e && in_module(e))
            for (; e->nMessage || e->nSig || e->pfn; e++)
                /* the map stores nCode truncated to 16 bits (the tree's
                 * TVN_GETDISPINFO appears as 0x0000fe6d) while the runtime
                 * notification code arrives sign-extended (0xfffffe6d), so
                 * the comparison is on the low half. */
                if (e->nMessage == msg && (e->nCode & 0xFFFF) == (code & 0xFFFF))
                    return e;
        if (!map->pfnGetBaseMap || !in_module((const void *)map->pfnGetBaseMap)) break;
        map = map->pfnGetBaseMap();
    }
    return NULL;
}

/* WM_NOTIFY arriving at a parent: hand it to the control's own object, the
 * way MFC's reflection does, so the view fills in what the control asked for. */
static int reflect_notify(WPARAM_T w, LPARAM_T l, LL *res)
{
    if (!l) return 0;
    void *nm = (void *)(unsigned long)l;            /* NMHDR: hwndFrom, idFrom, code */
    HWND from = *(HWND *)nm;
    DWORD code = *(DWORD *)((char *)nm + 16);
    void *child = wnd_from_handle(from);
    { static int n; if (n < 300) { n++;
        char m[80]; int k=0; const char*hx="0123456789abcdef";
        const char*p2="[mfc42] WM_NOTIFY code="; while(*p2)m[k++]=*p2++;
        for(int b=28;b>=0;b-=4) m[k++]=hx[(code>>b)&15];
        p2=" child="; while(*p2)m[k++]=*p2++;
        m[k++]= child ? 'y':'n'; m[k++]='\n'; m[k]=0; OutputDebugStringA(m); } }
    if (!child) return 0;
    void **vt = *(void ***)child;
    if (!vt || !vt[VS_GETMESSAGEMAP]) return 0;
    const AFX_MSGMAP *map = (const AFX_MSGMAP *)
        ((const AFX_MSGMAP *(*)(void *))vt[VS_GETMESSAGEMAP])(child);
    const AFX_MSGMAP_ENTRY *e = find_entry_code(map, WM_NOTIFY_ + WM_REFLECT_BASE, code);
    if (!e || !e->pfn) return 0;
    ((PMSG_NOTIFY)e->pfn)(child, nm, res);
    return 1;
}


/* ---- owner-draw reflection -------------------------------------------
 * depends.exe creates every list with LVS_OWNERDRAWFIXED and paints the
 * rows itself: the module names, the icons and the red "File not found"
 * text all come out of its own WM_DRAWITEM handler, not out of the control.
 * WM_DRAWITEM/WM_MEASUREITEM/WM_DELETEITEM arrive at the parent, and MFC
 * hands them straight back to the control's own object as
 * (message + WM_REFLECT_BASE); without that the lists stay blank however
 * many rows have been inserted. */
#define WM_DRAWITEM_    0x002B
#define WM_MEASUREITEM_ 0x002C
#define WM_DELETEITEM_  0x002D
#define OFF_DI_HWNDITEM 0x18          /* DRAWITEMSTRUCT::hwndItem, Win64 */
/* CWnd::OnChildNotify is slot 44 (read from the x86 build's CWnd vftable,
 * the same sources and so the same slot order; slot 25 PreCreateWindow and
 * slot 12 GetMessageMap come from the same table and already check out). */
#define VS_ONCHILDNOTIFY 44
typedef LL (*PFN_OCN)(void *self, DWORD msg, ULL w, LL l, LL *pResult);
typedef LL (*PMSG_PTR)(void *self, void *p);

static void *pane_by_id(DWORD id)
{
    for (int i = 0; i < g_nPane; i++) if (g_pane[i].id == id) return g_pane[i].wnd;
    return NULL;
}

static int reflect_item(DWORD msg, WPARAM_T w, LPARAM_T l, LL *res)
{
    void *child = NULL;
    if (msg == WM_MEASUREITEM_) {
        child = pane_by_id((DWORD)w);            /* no hwnd in the struct */
    } else if (l) {
        HWND from = *(HWND *)((char *)(unsigned long)l + OFF_DI_HWNDITEM);
        child = wnd_from_handle(from);
    }
    if (!child) return 0;
    void **vt = *(void ***)child;
    if (!vt) return 0;
    /* A control that paints itself overrides the virtual OnChildNotify;
     * depends.exe declares no WM_DRAWITEM handler in any of its message
     * maps, so this is the only path its lists can be painted through.
     * Only an override counts — the slot otherwise holds MFC's own
     * implementation, which is ours and would just reflect back here. */
    if (vt[VS_ONCHILDNOTIFY] && in_module(vt[VS_ONCHILDNOTIFY])) {
        LL r = ((PFN_OCN)vt[VS_ONCHILDNOTIFY])(child, msg, (ULL)w, (LL)l, res);
        if (r) return 1;
    }
    if (!vt[VS_GETMESSAGEMAP]) return 0;
    const AFX_MSGMAP *map = (const AFX_MSGMAP *)
        ((const AFX_MSGMAP *(*)(void *))vt[VS_GETMESSAGEMAP])(child);
    const AFX_MSGMAP_ENTRY *e = find_entry(map, msg + WM_REFLECT_BASE);
    if (!e || !e->pfn) return 0;
    *res = ((PMSG_PTR)e->pfn)(child, (void *)(unsigned long)l);
    return 1;
}

/* AfxWndProc — MFC's window procedure.  It is Alpha code: Wine calls this
 * address, the non-executable guest page faults, and the host runs it
 * through the translator.  It finds the C++ object behind the HWND and
 * dispatches through that object's message map into the application's own
 * handlers. */
static LL AfxWndProc(HWND h, DWORD msg, WPARAM_T w, LPARAM_T l)
{
    if (msg == WM_NOTIFY_) {
        LL res = 0;
        if (reflect_notify(w, l, &res)) return res;
    }
    if (msg == WM_DRAWITEM_ || msg == WM_MEASUREITEM_ || msg == WM_DELETEITEM_) {
        LL res = 0;
        if (reflect_item(msg, w, l, &res))
            return msg == WM_DRAWITEM_ ? 1 : res;
    }
    void *self = wnd_from_handle(h);
    if (!self && g_pendingWnd) {
        self = g_pendingWnd;
        wnd_attach(h, self);
        cwnd_set_hwnd(self, h);     /* OnCreate runs before CreateWindowEx returns */
    }

    if (self) {
        void **vt = *(void ***)self;
        if (vt && vt[VS_GETMESSAGEMAP]) {
            const AFX_MSGMAP *map = (const AFX_MSGMAP *)
                ((const AFX_MSGMAP *(*)(void *))vt[VS_GETMESSAGEMAP])(self);
            const AFX_MSGMAP_ENTRY *e = find_entry(map, msg);
            if (msg == 0x0001 && !e && self == g_childFrame) {
                /* No handler of its own: this is what CFrameWnd::OnCreate
                 * does — ask the frame to build its client area. */
                void **vt2 = *(void ***)self;
                if (vt2 && vt2[VS_ONCREATECLIENT]) {
                    void *lpcs = (void *)(unsigned long)l;
                    void *ctx  = lpcs ? *(void **)lpcs : (void *)&g_ctx;
                    if (!ctx || !in_module(ctx)) ctx = (void *)&g_ctx;
                    g_splitParent = h;   /* panes are built before CreateWindowEx returns */
                    OutputDebugStringA("[mfc42] child frame: OnCreateClient\n");
                    LL r = ((PFN_OCC)vt2[VS_ONCREATECLIENT])(self, lpcs, ctx);
                    OutputDebugStringA(r ? "[mfc42] OnCreateClient ok\n"
                                         : "[mfc42] OnCreateClient returned 0\n");
                    return 0;
                }
            }
            if (e && e->pfn) {
                switch (msg) {
                case 0x0001: {  /* WM_CREATE : int OnCreate(LPCREATESTRUCT) */
                    LL r = ((PMSG_CREATE)e->pfn)(self, (void *)(unsigned long)l);
                    OutputDebugStringA(r ? "[mfc42] OnCreate returned nonzero\n"
                                         : "[mfc42] OnCreate returned 0 (ok)\n");
                    return r; }
                case 0x0005:   /* WM_SIZE   : void OnSize(UINT, int, int)  */
                    ((PMSG_SIZE)e->pfn)(self, (DWORD)w,
                                        (int)(short)(l & 0xFFFF),
                                        (int)(short)((l >> 16) & 0xFFFF));
                    frame_layout();
                    pane_layout();
                    return 0;
                case 0x0002:   /* WM_DESTROY */
                    ((PMSG_VOID)e->pfn)(self);
                    PostQuitMessage(0);
                    return 0;
                default:
                    /* Everything else the class registered.  Most MFC
                     * handlers are (WPARAM, LPARAM) returning LRESULT, and on
                     * this ABI a handler that takes fewer arguments simply
                     * ignores the extra registers — so one shape serves.
                     * This is what lets depends.exe's own custom message
                     * (0x08D2, handler 0x426c60 in its frame map) drive the
                     * analysis from the message loop, exactly as it does on
                     * real hardware. */
                    return ((PMSG_GEN)e->pfn)(self, (ULL)w, (LL)l);
                }
            }
        }
    }
    if (msg == WM_DESTROY) { PostQuitMessage(0); return 0; }
    return DefWindowProcA(h, msg, w, l);
}

static int   g_classRegistered;
static const char *AFX_FRAME_CLASS = "AfxFrameOrView42";

static void register_frame_class(void)
{
    if (g_classRegistered) return;
    struct {
        DWORD style; LL (*proc)(HWND,DWORD,WPARAM_T,LPARAM_T);
        int extra1, extra2; HINSTANCE inst; void *icon, *cursor, *brush;
        LPCSTR menu, cls;
    } wc;
    m_memset(&wc, 0, sizeof wc);
    wc.style  = 0x0003;                       /* CS_HREDRAW | CS_VREDRAW */
    wc.proc   = AfxWndProc;
    wc.inst   = g_moduleState.m_hCurrentInstanceHandle;
    wc.cursor = LoadCursorA(NULL, (LPCSTR)(unsigned long)32512);   /* IDC_ARROW */
    wc.brush  = CreateSolidBrush(0x00C8C8C8);
    wc.cls    = AFX_FRAME_CLASS;
    RegisterClassA(&wc);
    g_classRegistered = 1;
}

/* CFrameWnd::LoadFrame(nIDResource, dwDefaultStyle, pParentWnd, pContext)
 * depends.exe calls this with nIDResource=128 and style 0x00CF8000, which is
 * WS_OVERLAPPEDWINDOW | FWS_ADDTOTITLE — the standard main frame. */
/*ORD 3460*/ int CFrameWnd_LoadFrame(void *self, DWORD nIDResource, DWORD dwStyle,
                                     void *pParentWnd, void *pContext)
{
    register_frame_class();
    HINSTANCE inst = g_moduleState.m_hCurrentInstanceHandle;

    /* The frame's caption and menu both come out of the application's own
     * resources under nIDResource, exactly as MFC does it. */
    static char title[256];
    if (!__gu_LoadStringFromModule(inst, nIDResource, title, sizeof title) || !title[0])
        m_memcpy(title, "Dependency Walker", 18);
    void *menu = __gu_LoadMenuFromModule(inst, nIDResource);

    /* WM_NCCREATE/WM_CREATE arrive from inside CreateWindowEx, so the object
     * has to be reachable before the call returns. */
    g_pendingWnd = self;
    HWND h = CreateWindowExA(0, AFX_FRAME_CLASS, title,
                             dwStyle & ~0x00008000u,   /* drop FWS_ADDTOTITLE */
                             20, 20, 980, 700, NULL, menu, inst, NULL);
    g_pendingWnd = NULL;
    if (!h) { OutputDebugStringA("[mfc42] LoadFrame: CreateWindowEx FAILED\n"); return 0; }
    cwnd_set_hwnd(self, h);
    OutputDebugStringA(menu ? "[mfc42] LoadFrame: frame + menu from .rsrc\n"
                            : "[mfc42] LoadFrame: frame created (no menu resource)\n");
    OutputDebugStringA("[mfc42] title: "); OutputDebugStringA(title); OutputDebugStringA("\n");
    ShowWindow(h, SW_SHOW);
    UpdateWindow(h);
    return 1;
}

/* ======================================================================
 * The frame's contents.
 *
 * depends.exe's CMainFrame::OnCreate runs the textbook MFC/MDI sequence;
 * the ordinals were identified by intersecting each one's position in the
 * x86 MFC42 name list with the class it had to belong to:
 *
 *   #3767 CFrameWnd::OnCreate      #5188 CStatusBar::SetIndicators
 *   #1615 CToolBar::CreateEx       #2027 CControlBar::EnableDocking
 *   #3472 CToolBar::LoadToolBar    #2028 CFrameWnd::EnableDocking
 *   #1575 CStatusBar::Create       #1898 CFrameWnd::DockControlBar
 * ==================================================================== */
#define WS_CHILD     0x40000000u
#define WS_VISIBLE   0x10000000u
#define WS_HSCROLL   0x00100000u
#define WS_VSCROLL   0x00200000u
#define WS_EX_CLIENTEDGE 0x00000200u
#define WM_SIZE      0x0005
#define SBT_NOBORDERS 0x0100

typedef struct { void *hWindowMenu; DWORD idFirstChild; } CLIENTCREATESTRUCT;

static HWND g_mdiClient, g_statusBar, g_frameHwnd;

static void frame_layout(void)
{
    if (!g_frameHwnd) return;
    int r[4]; GetClientRect(g_frameHwnd, r);       /* left,top,right,bottom */
    int w = r[2] - r[0], h = r[3] - r[1], sb = 0, tb = 0;
    if (g_toolBar)   { tb = g_toolBarH; MoveWindow(g_toolBar, 0, 0, w, tb, 1); }
    if (g_statusBar) { sb = 22; MoveWindow(g_statusBar, 0, h - sb, w, sb, 1); }
    if (g_mdiClient) MoveWindow(g_mdiClient, 0, tb, w, h - sb - tb, 1);
}

/* CFrameWnd::OnCreate — for an MDI frame this is what brings the MDI client
 * window into being; the client is a real MDICLIENT, created by Wine. */
/*ORD 3767*/ int CFrameWnd_OnCreate(void *self, void *lpcs)
{
    HWND frame = cwnd_hwnd(self);
    if (!frame) return 0;
    g_frameHwnd = frame;
    CLIENTCREATESTRUCT ccs; ccs.hWindowMenu = NULL; ccs.idFirstChild = 0xFF00;
    g_mdiClient = CreateWindowExA(WS_EX_CLIENTEDGE, "MDICLIENT", NULL,
                                  WS_CHILD | WS_VISIBLE | WS_HSCROLL | WS_VSCROLL,
                                  0, 0, 100, 100, frame,
                                  (void *)(unsigned long)0xCAC,
                                  g_moduleState.m_hCurrentInstanceHandle, &ccs);
    OutputDebugStringA(g_mdiClient ? "[mfc42] MDI client created\n"
                                   : "[mfc42] MDI client FAILED\n");
    frame_layout();
    return 0;
}

/* CStatusBar::Create(pParentWnd, dwStyle, nID) */
/*ORD 1575*/ int CStatusBar_Create(void *self, void *pParent, DWORD style, DWORD nID)
{
    HWND parent = pParent ? cwnd_hwnd(pParent) : g_frameHwnd;
    if (!parent) return 0;
    g_statusBar = CreateWindowExA(0, "msctls_statusbar32", "",
                                  WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, parent,
                                  (void *)(unsigned long)0xE001,
                                  g_moduleState.m_hCurrentInstanceHandle, NULL);
    if (g_statusBar) {
        cwnd_set_hwnd(self, g_statusBar);
        /* AFX_IDS_IDLEMESSAGE is MFC's own string, so it is ours to supply. */
        SendMessageA(g_statusBar, 0x0401 /* SB_SETTEXTA */, 0,
                     (LPARAM_T)(unsigned long)"For Help, press F1");
    }
    OutputDebugStringA(g_statusBar ? "[mfc42] status bar created\n"
                                   : "[mfc42] status bar FAILED\n");
    frame_layout();
    return g_statusBar != NULL;
}

/* ---- CToolBar --------------------------------------------------------
 * depends.exe builds its button bar the standard MFC way: construct, then
 * CreateEx(parent, TBSTYLE_FLAT, ..., AFX_IDW_TOOLBAR), then
 * LoadToolBar(IDR_MAINFRAME).  The buttons and their images both come out
 * of the application's own resources — an RT_TOOLBAR listing the command
 * ids and the RT_BITMAP strip of the same id. */
static void *g_toolBarObj, *g_toolBarParent;
static DWORD g_toolBarID;

/* CToolBar::CreateEx(pParentWnd, dwCtrlStyle, dwStyle, rcBorders, nID) —
 * rcBorders is a CRect passed by value, so it occupies two argument slots. */
/*ORD 1615*/ int CToolBar_CreateEx(void *self, void *pParent, DWORD dwCtrlStyle,
                                   DWORD dwStyle, LL rcLo, LL rcHi, DWORD nID)
{
    g_toolBarObj = self;
    g_toolBarParent = pParent;
    g_toolBarID = nID ? nID : 0xE800;
    return 1;                       /* the window waits for LoadToolBar */
}

/* CToolBar::LoadToolBar(nIDResource) */
/*ORD 3472*/ int CToolBar_LoadToolBar(void *self, DWORD nIDResource)
{
    HWND parent = g_toolBarParent ? cwnd_hwnd(g_toolBarParent) : g_frameHwnd;
    if (!parent) parent = g_frameHwnd;
    if (!parent) return 0;
    HINSTANCE inst = g_moduleState.m_hCurrentInstanceHandle;
    int hgt = 0;
    g_toolBar = __gu_ToolbarFromModule(inst, nIDResource, parent, g_toolBarID, &hgt);
    g_toolBarH = hgt > 0 ? hgt : 28;
    if (g_toolBar) cwnd_set_hwnd(self, g_toolBar);
    OutputDebugStringA(g_toolBar ? "[mfc42] toolbar created\n"
                                 : "[mfc42] toolbar FAILED\n");
    frame_layout();
    return g_toolBar != NULL;
}

/* CStatusBar::SetIndicators(const UINT *pIDs, int nIDs) — the pane layout.
 * The strings themselves live in the application's string table. */
/*ORD 5188*/ int CStatusBar_SetIndicators(void *self, const DWORD *pIDs, int n)
{
    if (!g_statusBar) return 0;
    HINSTANCE inst = g_moduleState.m_hCurrentInstanceHandle;
    static char text[256];
    if (pIDs && n > 0 && in_module(pIDs)) {
        if (__gu_LoadStringFromModule(inst, pIDs[0], text, sizeof text) && text[0])
            SendMessageA(g_statusBar, 0x0401 /* SB_SETTEXTA */, 0 | SBT_NOBORDERS,
                         (LPARAM_T)(unsigned long)text);
    }
    return 1;
}

/* ======================================================================
 * CRuntimeClass and the document/view chain
 *
 * The layout is read straight off depends.exe's own structures — the ones
 * it hands to its CMultiDocTemplate, e.g. at 0x449bd8:
 *
 *   m_lpszClassName   -> "CDocDepends"
 *   m_nObjectSize      = 552          m_wSchema = 0xffff
 *   m_pfnCreateObject  = 0x41d9f0     m_pfnGetBaseClass = 0x41da50
 *
 * m_pfnCreateObject is the application's own factory, so MFC creates the
 * document and the frame by calling back into depends.exe's code.
 * ==================================================================== */
struct CRuntimeClass {
    LPCSTR m_lpszClassName;                       /* 0x00 */
    int    m_nObjectSize;                         /* 0x08 */
    DWORD  m_wSchema;                             /* 0x0C */
    void  *(*m_pfnCreateObject)(void);            /* 0x10 */
    struct CRuntimeClass *(*m_pfnGetBaseClass)(void); /* 0x18 */
    struct CRuntimeClass *m_pNextClass;           /* 0x20 */
};

#define WS_EX_MDICHILD 0x00000040u
#define WS_OVERLAPPEDWINDOW_ 0x00CF0000u

static int   g_mdiClassRegistered;
static const char *AFX_MDICHILD_CLASS = "AfxMDIChild42";

/* Create the MDI child window for a freshly made frame object.  The object
 * is published first, because WM_CREATE reaches its OnCreate from inside
 * CreateWindowEx and the application expects m_hWnd to be live by then. */
static void mdi_child_create(void *pFrame, const char *title)
{
    if (!g_mdiClient) { OutputDebugStringA("[mfc42] no MDI client\n"); return; }
    if (!g_mdiClassRegistered) {
        struct { DWORD style; LL (*proc)(HWND,DWORD,WPARAM_T,LPARAM_T);
                 int e1, e2; HINSTANCE inst; void *icon,*cursor,*brush;
                 LPCSTR menu, cls; } wc;
        m_memset(&wc, 0, sizeof wc);
        wc.style = 0x0003; wc.proc = AfxWndProc;
        wc.inst  = g_moduleState.m_hCurrentInstanceHandle;
        wc.cursor= LoadCursorA(NULL, (LPCSTR)(unsigned long)32512);
        wc.brush = CreateSolidBrush(0x00FFFFFF);
        wc.cls   = AFX_MDICHILD_CLASS;
        RegisterClassA(&wc);
        g_mdiClassRegistered = 1;
    }
    g_ctx.m_pNewViewClass   = g_tmpl.pView;
    g_ctx.m_pCurrentDoc     = g_curDoc;
    g_ctx.m_pNewDocTemplate = g_tmpl.self;
    g_ctx.m_pLastView       = NULL;
    g_ctx.m_pCurrentFrame   = pFrame;
    g_childFrame = pFrame;
    g_pendingWnd = pFrame;
    HWND h = CreateWindowExA(WS_EX_MDICHILD, AFX_MDICHILD_CLASS,
                             title && *title ? title : "Document",
                             WS_CHILD | WS_VISIBLE | WS_OVERLAPPEDWINDOW_,
                             0, 0, 100, 100, g_mdiClient, NULL,
                             g_moduleState.m_hCurrentInstanceHandle, &g_ctx);
    g_pendingWnd = NULL;
    OutputDebugStringA(h ? "[mfc42] MDI child window created\n"
                         : "[mfc42] MDI child window FAILED\n");
    /* With a document open MFC swaps the frame's menu for the document
     * template's own (CMDIFrameWnd::OnUpdateFrameMenu), which is where
     * depends.exe's Edit / Profile / Window menus come from. */
    if (h && g_tmpl.nID) {
        void *dm = __gu_LoadMenuFromModule(g_moduleState.m_hCurrentInstanceHandle,
                                           g_tmpl.nID);
        if (dm && g_mdiClient) {
            SendMessageA(g_mdiClient, 0x0230 /* WM_MDISETMENU */,
                         (WPARAM_T)(unsigned long)dm, 0);
            DrawMenuBar(g_frameHwnd);
            OutputDebugStringA("[mfc42] document menu installed\n");
        }
    }
    if (h) {
        cwnd_set_hwnd(pFrame, h); g_splitParent = h;
        int r[4];                              /* fill the MDI workspace */
        GetClientRect(g_mdiClient, r);
        MoveWindow(h, 0, 0, r[2]-r[0], r[3]-r[1], 1);
        ShowWindow(h, SW_SHOW); UpdateWindow(h);
        pane_layout();
    }
}


/* ---- MFC's own CRuntimeClass objects ---------------------------------
 * depends.exe's classes point at these as their base class, and its document
 * template names CView as the view class, so they must be real structures
 * exported as *data* at the right ordinals — a code thunk would be read as a
 * struct and produce nonsense. */
#define MFC_CLASS(sym, name, size) \
    CRuntimeClass sym = { name, size, 0xFFFF, NULL, NULL, NULL }

MFC_CLASS(classCDocument,     "CDocument",     216);
MFC_CLASS(classCFileDialog,   "CFileDialog",   360);
MFC_CLASS(classCListView,     "CListView",     424);
MFC_CLASS(classCMDIChildWnd,  "CMDIChildWnd",  400);
MFC_CLASS(classCMDIFrameWnd,  "CMDIFrameWnd",  432);
MFC_CLASS(classCRichEditView, "CRichEditView", 520);
MFC_CLASS(classCTreeView,     "CTreeView",     424);
MFC_CLASS(classCView,         "CView",         400);

static LPCSTR rtc_name(CRuntimeClass *c)
{
    if (!c || !in_module(c)) return "?";
    LPCSTR n = c->m_lpszClassName;
    return (n && in_module(n)) ? n : "?";
}


/* CMultiDocTemplate(nIDResource, pDocClass, pFrameClass, pViewClass) */
/*ORD 379*/ void *CMultiDocTemplate_ctor(void *self, DWORD nID, CRuntimeClass *pDoc,
                                         CRuntimeClass *pFrame, CRuntimeClass *pView)
{
    g_tmpl.self = self; g_tmpl.nID = nID;
    g_tmpl.pDoc = pDoc; g_tmpl.pFrame = pFrame; g_tmpl.pView = pView;
    OutputDebugStringA("[mfc42] doc template: doc=");
    OutputDebugStringA(rtc_name(pDoc));
    OutputDebugStringA(" frame="); OutputDebugStringA(rtc_name(pFrame));
    OutputDebugStringA(" view=");  OutputDebugStringA(rtc_name(pView));
    OutputDebugStringA("\n");
    return self;
}
/*ORD 869*/ void CWinApp_AddDocTemplate(void *self, void *pTemplate) { }

/* ---- CCommandLineInfo / shell command --------------------------------- */
extern LPCSTR GetCommandLineA(void);

/* CWinApp::ParseCommandLine(CCommandLineInfo&) — the token after the program
 * name is the module to analyse, which is what turns FileNew into FileOpen. */
/*ORD 4557*/ void CWinApp_ParseCommandLine(void *self, CCommandLineInfo *info)
{
    if (!info) return;
    const char *p = GetCommandLineA();
    if (!p) return;
    if (*p == '"') { p++; while (*p && *p != '"') p++; if (*p) p++; }
    else while (*p && *p != ' ') p++;
    while (*p == ' ') p++;
    if (!*p) return;
    static char fn[512];
    int i = 0;
    if (*p == '"') { p++; while (*p && *p != '"' && i < 511) fn[i++] = *p++; }
    else while (*p && *p != ' ' && i < 511) fn[i++] = *p++;
    fn[i] = 0;
    if (fn[0] == '/' || fn[0] == '-') return;       /* a switch, not a file */
    info->m_strFileName = str_dup(fn, (unsigned long)i);
    info->m_nShellCommand = 1;                      /* FileOpen */
    OutputDebugStringA("[mfc42] command line names a module: ");
    OutputDebugStringA(fn); OutputDebugStringA("\n");
}

/* CWinApp::ProcessShellCommand — for FileOpen this is where MFC builds the
 * document, its MDI child frame and its view, then asks the document to read
 * the file.  Everything it calls is depends.exe's own code. */
/* CDocument's virtuals sit in declaration order; depends.exe's document
 * vtable shows OnNewDocument at 30 and OnCloseDocument at 33, which places
 * OnOpenDocument at 31 (its override lives at 0x41f740) and SetPathName at 23. */
#define VS_SETPATHNAME 23
#define VS_ONOPENDOC   31
#define VS_ONINITIALUPDATE 58
typedef LL (*PFN_STR)(void *self, LPCSTR s);
typedef LL (*PFN_STRB)(void *self, LPCSTR s, int b);
/*ORD 4641*/ int CWinApp_ProcessShellCommand(void *self, CCommandLineInfo *info)
{
    if (!info || info->m_nShellCommand != 1) {
        OutputDebugStringA("[mfc42] ProcessShellCommand: nothing to open\n");
        return 1;
    }
    if (!g_tmpl.pDoc || !in_module(g_tmpl.pDoc) || !g_tmpl.pDoc->m_pfnCreateObject) {
        OutputDebugStringA("[mfc42] ProcessShellCommand: no document template\n");
        return 1;
    }
    void *pDoc = g_tmpl.pDoc->m_pfnCreateObject();
    OutputDebugStringA(pDoc ? "[mfc42] document created\n" : "[mfc42] document FAILED\n");
    if (pDoc) {                       /* are its CString members initialised? */
        char dbg[64]; int k = 0;
        const char *hex = "0123456789abcdef";
        for (int off = 56; off <= 232; off += 8) {
            ULL v = *(ULL *)((char *)pDoc + off);
            k = 0;
            dbg[k++]='['; dbg[k++]='m'; dbg[k++]='f'; dbg[k++]='c'; dbg[k++]=']';
            dbg[k++]=' '; dbg[k++]='+';
            dbg[k++]='0'+(off/100)%10; dbg[k++]='0'+(off/10)%10; dbg[k++]='0'+off%10;
            dbg[k++]='='; 
            for (int b = 60; b >= 0; b -= 4) dbg[k++] = hex[(v >> b) & 15];
            dbg[k++]='\n'; dbg[k]=0;
            OutputDebugStringA(dbg);
        }
    }
    if (!pDoc) return 0;
    g_curDoc = pDoc;

    if (g_tmpl.pFrame && in_module(g_tmpl.pFrame) && g_tmpl.pFrame->m_pfnCreateObject) {
        void *pFrame = g_tmpl.pFrame->m_pfnCreateObject();
        OutputDebugStringA(pFrame ? "[mfc42] child frame object created\n"
                                  : "[mfc42] child frame FAILED\n");
        if (pFrame) mdi_child_create(pFrame, info->m_strFileName);
    }

    /* Now let the document read the module.  This is depends.exe's own
     * analysis code, running as Alpha under the translator. */
    void **vt = *(void ***)pDoc;
    if (vt && vt[VS_ONOPENDOC]) {
        OutputDebugStringA("[mfc42] OnOpenDocument: ");
        OutputDebugStringA(info->m_strFileName); OutputDebugStringA("\n");
        LL ok = __mfc_guarded_call(vt[VS_ONOPENDOC], (LL)(unsigned long)pDoc,
                                   (LL)(unsigned long)info->m_strFileName, 0);
        OutputDebugStringA(ok ? "[mfc42] document opened OK\n"
                              : "[mfc42] OnOpenDocument returned FALSE\n");
        if (ok && vt[VS_SETPATHNAME])
            ((PFN_STRB)vt[VS_SETPATHNAME])(pDoc, info->m_strFileName, 1);

        /* CFrameWnd::InitialUpdateFrame: every view is told to draw itself
         * from the document.  Slot 58 is CView::OnInitialUpdate — bracketed
         * in depends.exe's view vtables by OnPrepareDC (57) and
         * OnActivateView (59), and overridden by the application at
         * 0x42d310, which is where it fills the tree and the lists. */
        if (ok) {
            OutputDebugStringA("[mfc42] initial update of the views\n");
            for (int i = 0; i < g_nPane; i++) {
                void *pv = g_pane[i].wnd;
                if (!pv) continue;
                void **vv = *(void ***)pv;
                if (vv && vv[VS_ONINITIALUPDATE])
                    __mfc_guarded_call(vv[VS_ONINITIALUPDATE],
                                       (LL)(unsigned long)pv, 0, 0);
            }
            OutputDebugStringA("[mfc42] views updated\n");
        }
    }
    return 1;
}

/* ======================================================================
 * Handing a vtable back to the application
 *
 * MSVC's AXP64 code makes indirect calls through v0, not through pv/r27
 * (depends.exe:0x430878 `jsr ra,(v0)`), so a gcc-built routine reached from
 * a vtable would derive gp from whatever r27 happened to hold.  Every entry
 * therefore points at a small thunk that recomputes pv from the PC first —
 * the same shape elf2pe emits for exports, built here at run time:
 *
 *      br   $27, .+4        ; r27 = &next instruction
 *      ldah $27, hi($27)
 *      lda  $27, lo($27)    ; r27 = &target
 *      jmp  $31, ($27)
 * ==================================================================== */
static DWORD alpha_mem(DWORD op, DWORD ra, DWORD rb, int d)
{ return (op << 26) | (ra << 21) | (rb << 16) | ((DWORD)d & 0xFFFF); }

static void *make_pv_thunk(void *target)
{
    DWORD *t = mfc_alloc(16);
    if (!t) return target;
    long long off = (long long)(unsigned long)target - ((long long)(unsigned long)t + 4);
    int lo = (int)(short)(off & 0xFFFF);
    int hi = (int)((off - lo) >> 16);
    t[0] = (0x30u << 26) | (27u << 21);          /* br   $27, .+4     */
    t[1] = alpha_mem(0x09, 27, 27, hi);          /* ldah $27, hi($27) */
    t[2] = alpha_mem(0x08, 27, 27, lo);          /* lda  $27, lo($27) */
    t[3] = alpha_mem(0x1A, 31, 27, 0);           /* jmp  $31, ($27)   */
    return t;
}

/* ======================================================================
 * CFile — how the document reads the module it is analysing.
 *
 * The slot order is read straight out of the x86 MFC42 binary's
 * ??_7CFile@@6B@ vftable (same sources as the AXP64 build):
 *   [5] GetPosition [6] GetFileName  [7] GetFileTitle [8] GetFilePath
 *   [9] SetFilePath [10] Open        [11] Duplicate   [12] Seek
 *  [13] SetLength   [14] GetLength   [15] Read        [16] Write
 *  [17] LockRange   [18] UnlockRange [19] Abort       [20] Flush [21] Close
 * ==================================================================== */
extern long __sys_open(const char *path, int flags);
extern long __sys_read(int fd, void *buf, unsigned long n);
extern long __sys_seek(int fd, long off, int whence);
extern long __sys_close(int fd);
extern long __sys_fsize(int fd);

typedef struct {
    void **vptr;              /* 0x00 */
    LL     m_hFile;           /* 0x08 */
    int    m_bCloseOnDelete;  /* 0x10 */
    char  *m_strFileName;     /* 0x18 */
} MFCFile;

static LL    file_nop(void *self) { return 0; }
static DWORD file_GetLength(MFCFile *f)
{ return f ? (DWORD)__sys_fsize((int)f->m_hFile) : 0; }
static LL    file_Seek(MFCFile *f, LL off, DWORD from)
{ return f ? (LL)__sys_seek((int)f->m_hFile, (long)off, (int)from) : -1; }
static DWORD g_readCalls, g_readBytes;
static DWORD file_Read(MFCFile *f, void *buf, DWORD n)
{ if (!f || !buf) return 0;
  long r = __sys_read((int)f->m_hFile, buf, n);
  if (r > 0) { g_readCalls++; g_readBytes += (DWORD)r; }
  return r < 0 ? 0 : (DWORD)r; }
static void  file_Close(MFCFile *f)
{ if (f && f->m_hFile >= 0) { __sys_close((int)f->m_hFile); f->m_hFile = -1; } }

static void *g_cfileVtbl[22];
static void cfile_init(void)
{
    void *nop = make_pv_thunk((void *)file_nop);
    for (int i = 0; i < 22; i++) g_cfileVtbl[i] = nop;
    g_cfileVtbl[12] = make_pv_thunk((void *)file_Seek);
    g_cfileVtbl[14] = make_pv_thunk((void *)file_GetLength);
    g_cfileVtbl[15] = make_pv_thunk((void *)file_Read);
    g_cfileVtbl[21] = make_pv_thunk((void *)file_Close);
}

/* ---- CDocument's own members -----------------------------------------
 * From the x86 MFC42: SetTitle works on `lea 0x20(%eax)` and SetPathName on
 * `lea 0x24(%edi)`, so m_strTitle is at 0x20 and m_strPathName at 0x24 there.
 * The ×2 widening to 64-bit is confirmed twice over in this hierarchy —
 * CWnd::m_hWnd (x86 0x20) measured at 64, and CView::m_pDocument (x86 0x40,
 * from AddView's `mov %edi,0x40(%esi)`) measured at 128 by depends.exe's own
 * code.  GetPathName and GetTitle are inline in the headers, so these
 * offsets are what the application reads directly. */
#define OFF_DOC_TITLE    64
#define OFF_DOC_PATHNAME 72

/*ORD 314*/ void *CDocument_ctor(void *self)
{
    if (self) {
        *(char **)((char *)self + OFF_DOC_TITLE)    = EMPTY_STR;
        *(char **)((char *)self + OFF_DOC_PATHNAME) = EMPTY_STR;
    }
    return self;
}
/*ORD 5350*/ void CDocument_SetTitle(void *self, LPCSTR title)
{ if (self) str_set((char **)((char *)self + OFF_DOC_TITLE), title); }

/*ORD 5252*/ void CDocument_SetPathName(void *self, LPCSTR path, int bAddToMRU)
{
    if (!self || !path) return;
    str_set((char **)((char *)self + OFF_DOC_PATHNAME), path);
    const char *base = path, *q;            /* the title is the file name */
    for (q = path; *q; q++) if (*q == '\\' || *q == '/') base = q + 1;
    CDocument_SetTitle(self, base);
    OutputDebugStringA("[mfc42] document path set: ");
    OutputDebugStringA(path); OutputDebugStringA("\n");
}

/* CDocument::GetFile(lpszFileName, nOpenFlags, pError) */
/*ORD 2516*/ void *CDocument_GetFile(void *self, LPCSTR name, DWORD flags, void *pErr)
{
    if (!name) return NULL;
    long fd = __sys_open(name, 0);
    if (fd < 0) { OutputDebugStringA("[mfc42] GetFile: cannot open ");
                  OutputDebugStringA(name); OutputDebugStringA("\n"); return NULL; }
    MFCFile *f = mfc_alloc(sizeof *f);
    if (!f) { __sys_close((int)fd); return NULL; }
    f->vptr = g_cfileVtbl;
    f->m_hFile = fd;
    f->m_bCloseOnDelete = 1;
    f->m_strFileName = str_dup(name, m_strlen(name));
    OutputDebugStringA("[mfc42] GetFile: opened "); OutputDebugStringA(name);
    OutputDebugStringA("\n");
    return f;
}
/*ORD 4799*/ void CDocument_ReleaseFile(void *self, void *pFile, int bAbort)
{
    char m[80]; int k=0; const char*hx="0123456789abcdef";
    const char *p="[mfc42] file reads="; while(*p) m[k++]=*p++;
    for(int b=28;b>=0;b-=4) m[k++]=hx[(g_readCalls>>b)&15];
    p=" bytes="; while(*p) m[k++]=*p++;
    for(int b=28;b>=0;b-=4) m[k++]=hx[(g_readBytes>>b)&15];
    m[k++]='\n'; m[k]=0; OutputDebugStringA(m);
    if (pFile) file_Close((MFCFile *)pFile);
}

/* ======================================================================
 * CSplitterWnd — the child frame's panes
 *
 * depends.exe's CChildFrame::OnCreateClient builds its layout with
 *   #1701 CreateStatic  #1709 CreateView  #2826 GetPane  #3258 IdFromRowCol
 * and keeps what GetPane hands back, so these have to return real objects
 * with real windows behind them.  Each pane is a view object made by the
 * application's own factory, wrapped around a genuine common control.
 * ==================================================================== */
#define AFX_IDW_PANE_FIRST 0xE900
#define WS_BORDER_  0x00800000u
#define WS_VSCROLL_ 0x00200000u

/* depends.exe builds two splitters (imports/exports, then modules/profile),
 * so panes are kept in one list and tiled inside the document window. */

/* Dependency Walker's arrangement: the module tree fills the upper left, the
 * import and export lists stack to its right, and the module list runs across
 * the bottom.  The panes arrive in creation order — tree, imports, exports,
 * modules, profile. */
static void pane_layout(void)
{
    if (!g_splitParent || !g_nPane) return;
    int r[4]; GetClientRect(g_splitParent, r);
    int w = r[2] - r[0], h = r[3] - r[1];
    if (w <= 0 || h <= 0) return;
    int left = (w * 46) / 100;          /* tree column   */
    int top  = (h * 56) / 100;          /* tree/list band */
    int botH = h - top;
    if (g_nPane == 1) { MoveWindow(g_pane[0].h, 0, 0, w, h, 1); return; }
    if (g_pane[0].h) MoveWindow(g_pane[0].h, 0, 0, left, top, 1);              /* tree    */
    if (g_nPane > 1 && g_pane[1].h) MoveWindow(g_pane[1].h, left, 0, w-left, top/2, 1);       /* imports */
    if (g_nPane > 2 && g_pane[2].h) MoveWindow(g_pane[2].h, left, top/2, w-left, top-top/2, 1);/* exports */
    if (g_nPane > 3 && g_pane[3].h) MoveWindow(g_pane[3].h, 0, top, w, (botH*62)/100, 1);      /* modules */
    if (g_nPane > 4 && g_pane[4].h)
        MoveWindow(g_pane[4].h, 0, top + (botH*62)/100, w, botH - (botH*62)/100, 1);          /* log     */
}


/* CCtrlView::CCtrlView(lpszClass, dwStyle) — the application names the
 * control class each view wraps ("SysTreeView32", "SysListView32"), so the
 * pane type comes from its own data rather than from guesswork. */
static struct { void *obj; LPCSTR cls; DWORD style; } g_ctrlView[64];
static int g_nCtrlView;
/*ORD 294*/ void *CCtrlView_ctor(void *self, LPCSTR cls, DWORD style)
{
    if (self && g_nCtrlView < 64) {
        g_ctrlView[g_nCtrlView].obj = self;
        g_ctrlView[g_nCtrlView].cls = cls;
        g_ctrlView[g_nCtrlView].style = style;
        g_nCtrlView++;
    }
    return self;
}
static DWORD ctrlview_style(void *obj)
{
    for (int i = 0; i < g_nCtrlView; i++)
        if (g_ctrlView[i].obj == obj) return g_ctrlView[i].style;
    return 0x50800000u;          /* AFX_WS_DEFAULT_VIEW */
}

/* CRichEditView::CRichEditView() — out of line in MFC (unlike CTreeView and
 * CListView, whose constructors are inline and call CCtrlView directly),
 * which is why depends.exe's CRichViewProfile factory reaches it as #480.
 * MFC 4.2 passes class "RICHEDIT" and AFX_WS_DEFAULT_VIEW plus
 * WS_HSCROLL|WS_VSCROLL|ES_AUTOHSCROLL|ES_AUTOVSCROLL|ES_MULTILINE|ES_NOHIDESEL. */
/*ORD 480*/ void *CRichEditView_ctor(void *self)
{
    return CCtrlView_ctor(self, "RICHEDIT",
                          0x50800000u | 0x00100000u | 0x00200000u | 0x80 | 0x40 | 0x4 | 0x100);
}

static LPCSTR ctrlview_class(void *obj)
{
    for (int i = 0; i < g_nCtrlView; i++)
        if (g_ctrlView[i].obj == obj && g_ctrlView[i].cls && in_module(g_ctrlView[i].cls))
            return g_ctrlView[i].cls;
    return NULL;
}

/* CSplitterWnd::CreateStatic(pParentWnd, nRows, nCols, dwStyle, nID) */
/*ORD 1701*/ int CSplitterWnd_CreateStatic(void *self, void *pParent, int rows, int cols,
                                           DWORD style, DWORD nID)
{
    if (self) cwnd_set_hwnd(self, g_splitParent);
    OutputDebugStringA("[mfc42] splitter: static layout created\n");
    return 1;
}

/* CSplitterWnd::IdFromRowCol(row, col) */
/*ORD 3258*/ int CSplitterWnd_IdFromRowCol(void *self, int row, int col)
{ return AFX_IDW_PANE_FIRST + row * 16 + col; }

/* CSplitterWnd::GetPane(row, col) */
/*ORD 2826*/ void *CSplitterWnd_GetPane(void *self, int row, int col)
{
    for (int i = 0; i < g_nPane; i++)
        if (g_pane[i].split == self && g_pane[i].row == row && g_pane[i].col == col) {
            return g_pane[i].wnd;
        }
    return NULL;
}

/* CSplitterWnd::CreateView(row, col, pViewClass, sizeInit, pContext)
 * The view object comes from the application's own CRuntimeClass factory;
 * the window behind it is a real common control, chosen from the class name
 * the application registered (its view classes are named for what they show). */
/* CREATESTRUCTA as Win64 lays it out; handed to the view's PreCreateWindow
 * exactly as MFC's CWnd::CreateEx does, so the application can add its own
 * styles (report mode, tree lines...) before the control is made. */
typedef struct {
    void *lpCreateParams;   /* 0x00 */
    void *hInstance;        /* 0x08 */
    void *hMenu;            /* 0x10 */
    HWND  hwndParent;       /* 0x18 */
    int   cy, cx, y, x;     /* 0x20 */
    int   style;            /* 0x30 */
    LPCSTR lpszName;        /* 0x38 */
    LPCSTR lpszClass;       /* 0x40 */
    DWORD dwExStyle;        /* 0x48 */
} CREATESTRUCTA_;
#define VS_PRECREATEWINDOW 25    /* CWnd::PreCreateWindow, read from 9 vtables */
typedef LL (*PFN_PCW)(void *self, CREATESTRUCTA_ *cs);

/* every view attached to the document, for UpdateAllViews and friends */
static struct { void *view; void *doc; } g_views[32];
static int g_nViews;
static void doc_add_view(void *doc, void *view)
{
    if (!view) return;
    *(void **)((char *)view + OFF_M_PDOCUMENT) = doc;   /* CDocument::AddView */
    if (g_nViews < 32) { g_views[g_nViews].view = view; g_views[g_nViews].doc = doc; g_nViews++; }
}

/*ORD 1709*/ int CSplitterWnd_CreateView(void *self, int row, int col,
                                         CRuntimeClass *pViewClass,
                                         LL sizeInit, void *pContext)
{
    if (!pViewClass || !in_module(pViewClass) || !pViewClass->m_pfnCreateObject) {
        OutputDebugStringA("[mfc42] CreateView: no runtime class\n");
        return 0;
    }
    void *pView = pViewClass->m_pfnCreateObject();
    LPCSTR cname = rtc_name(pViewClass);
    if (!pView) { OutputDebugStringA("[mfc42] CreateView: factory failed\n"); return 0; }

    /* class and default style come from the view's own CCtrlView constructor */
    CREATESTRUCTA_ cs;
    m_memset(&cs, 0, sizeof cs);
    cs.lpCreateParams = pContext;
    cs.hInstance  = g_moduleState.m_hCurrentInstanceHandle;
    cs.hMenu      = (void *)(unsigned long)(AFX_IDW_PANE_FIRST + row * 16 + col);
    cs.hwndParent = g_splitParent;
    cs.cx = 100; cs.cy = 100;
    cs.lpszName   = "";
    cs.lpszClass  = ctrlview_class(pView);
    cs.style      = (int)ctrlview_style(pView);
    if (!cs.lpszClass) cs.lpszClass = "SysListView32";
    cs.style |= (int)(WS_CHILD | WS_VISIBLE);

    /* let the application shape its window, as MFC would */
    void **vt = *(void ***)pView;
    if (vt && vt[VS_PRECREATEWINDOW])
        ((PFN_PCW)vt[VS_PRECREATEWINDOW])(pView, &cs);
    if (g_noOwnerDraw) cs.style &= ~0x0400;   /* experiment: LVS_OWNERDRAWFIXED */

    HWND h = CreateWindowExA(cs.dwExStyle, cs.lpszClass, cs.lpszName, (DWORD)cs.style,
                             cs.x, cs.y, cs.cx, cs.cy, cs.hwndParent, cs.hMenu,
                             cs.hInstance, cs.lpCreateParams);
    if (h) {
        cwnd_set_hwnd(pView, h);
        wnd_attach(h, pView);
        void *doc = g_curDoc;
        if (pContext && in_module(pContext) && ((CCreateContext *)pContext)->m_pCurrentDoc)
            doc = ((CCreateContext *)pContext)->m_pCurrentDoc;
        doc_add_view(doc, pView);
        if (g_nPane < 16) {
            g_pane[g_nPane].wnd = pView; g_pane[g_nPane].h = h;
            g_pane[g_nPane].id  = (DWORD)(AFX_IDW_PANE_FIRST + row * 16 + col);
            g_pane[g_nPane].split = self;
            g_pane[g_nPane].row = row; g_pane[g_nPane].col = col;
            g_nPane++;
        }
    }
    OutputDebugStringA("[mfc42] CreateView: ");
    OutputDebugStringA(cname); OutputDebugStringA(" -> ");
    OutputDebugStringA(h ? cs.lpszClass : "FAILED"); OutputDebugStringA("\n");

    pane_layout();
    return h != NULL;
}

/* ======================================================================
 * CListCtrl / CTreeCtrl
 *
 * MFC's convenience overloads are out of line, so they are real exports —
 * which is why nothing reached the controls while they were stubs.  They
 * only have to marshal into the common-control message the wrapper would
 * send.  depends.exe calls InsertColumn 31 times at start-up with its own
 * headings ("Ordinal", "Hint", "Function", "Entry Point", "Module",
 * "File Time Stamp", ...), one set per list pane.
 * ==================================================================== */
#define LVM_INSERTCOLUMNA 0x101B
#define LVM_INSERTITEMA   0x1007
#define LVM_SETITEMTEXTA  0x102E
#define LVM_DELETEALLITEMS 0x1009
#define LVCF_FMT 1
#define LVCF_WIDTH 2
#define LVCF_TEXT 4
#define LVCF_SUBITEM 8
#define LVIF_TEXT 1

typedef struct {                 /* LVCOLUMNA, Win64 layout */
    DWORD mask; int fmt; int cx;
    LPCSTR pszText; int cchTextMax; int iSubItem; int iImage; int iOrder;
} LVCOLUMNA_;
typedef struct {                 /* LVITEMA, Win64 layout */
    DWORD mask; int iItem; int iSubItem; DWORD state, stateMask;
    LPCSTR pszText; int cchTextMax; int iImage;
    LL lParam; int iIndent; int iGroupId; DWORD cColumns; void *puColumns;
} LVITEMA_;

/* CListCtrl::InsertColumn(nCol, heading, nFormat, nWidth, nSubItem) */
/*ORD 3327*/ int CListCtrl_InsertColumn(void *self, int nCol, LPCSTR heading,
                                        int fmt, int width, int subItem)
{
    HWND h = cwnd_hwnd(self);
    { char m[96]; int k=0;
      const char*p2="[mfc42] InsertColumn '"; while(*p2)m[k++]=*p2++;
      if(heading) for(const char*q=heading;*q&&k<88;q++) m[k++]=*q;
      m[k++]='\''; m[k++]='\n'; m[k]=0; OutputDebugStringA(m); }
    if (!h || !heading) return -1;
    LVCOLUMNA_ col;
    m_memset(&col, 0, sizeof col);
    col.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
    col.fmt  = fmt;
    col.cx   = width > 0 ? width : 110;      /* -1 means "pick something" */
    col.pszText  = heading;
    col.iSubItem = subItem >= 0 ? subItem : nCol;
    return (int)SendMessageA(h, LVM_INSERTCOLUMNA, (WPARAM_T)nCol,
                             (LPARAM_T)(unsigned long)&col);
}

/* ---- CWnd::OnChildNotify / CListView::OnChildNotify -------------------
 * This is the hinge of the whole owner-draw path.  depends.exe overrides
 * OnChildNotify on its views, and for WM_DRAWITEM it simply calls the base
 * class — so MFC is the one that has to turn the notification into a call
 * of the view's own virtual DrawItem.  Slot 35 is DrawItem: every class in
 * the x86 build that declares it (CListBox, CListCtrl, CListView,
 * CHeaderCtrl, ...) carries it there, their empty default bodies folded
 * together by the linker. */
/* CListView declares DrawItem itself, so it lands at the very end of the
 * class vtable, not in CWnd's range: slot 68, read from the x86 build (slot
 * 58 in the same table is OnInitialUpdate, which already checks out). */
#define VS_DRAWITEM 68
typedef void (*PFN_DRAWITEM)(void *self, void *lpdis);

/* CWnd::OnChildNotify — hand the message back to the control's own map as
 * (message + WM_REFLECT_BASE), which is MFC's ReflectChildNotify. */
/*ORD 3713*/ LL CWnd_OnChildNotify(void *self, DWORD msg, ULL w, LL l, LL *pResult)
{
    void **vt = self ? *(void ***)self : NULL;
    if (!vt || !vt[VS_GETMESSAGEMAP]) return 0;
    const AFX_MSGMAP *map = (const AFX_MSGMAP *)
        ((const AFX_MSGMAP *(*)(void *))vt[VS_GETMESSAGEMAP])(self);
    const AFX_MSGMAP_ENTRY *e = find_entry(map, msg + WM_REFLECT_BASE);
    if (!e || !e->pfn) return 0;
    LL r = ((PMSG_GEN)e->pfn)(self, w, l);
    if (pResult) *pResult = r;
    return 1;
}

/* CListView::OnChildNotify — WM_DRAWITEM becomes a call of the virtual
 * DrawItem; everything else falls back to CWnd's reflection. */
/*ORD 3709*/ LL CListView_OnChildNotify(void *self, DWORD msg, ULL w, LL l, LL *pResult)
{
    if (msg == WM_DRAWITEM_ && self && l) {
        void **vt = *(void ***)self;
        if (vt && vt[VS_DRAWITEM]) {
            ((PFN_DRAWITEM)vt[VS_DRAWITEM])(self, (void *)(unsigned long)l);
            return 1;
        }
    }
    return CWnd_OnChildNotify(self, msg, w, l, pResult);
}

/* ---- CImageList ------------------------------------------------------
 * depends.exe builds four image lists from its own RT_BITMAP strips (ids
 * 150..153) with magenta as the transparent colour; the arguments are the
 * plain CImageList::Create(nBitmapID, cx, nGrow, crMask) overload.
 * CImageList is CObject plus one handle, so on Win64 m_hImageList sits at
 * +8 — which is exactly the 16-byte spacing of the four objects the
 * application keeps them in.  MFC's CListCtrl::SetImageList and
 * CTreeCtrl::SetImageList are inline, so once the handle is real the
 * application wires the lists to the controls by itself. */
#define OFF_M_HIMAGELIST 8
/*ORD 1549*/ int CImageList_Create(void *self, DWORD nBitmapID, int cx,
                                   int nGrow, DWORD crMask)
{
    HINSTANCE inst = g_moduleState.m_hCurrentResourceHandle
                   ? g_moduleState.m_hCurrentResourceHandle
                   : g_moduleState.m_hCurrentInstanceHandle;
    void *h = __gu_ImageListFromBitmap(inst, nBitmapID, cx, crMask);
    if (self) *(void **)((char *)self + OFF_M_HIMAGELIST) = h;
    dbg2("[mfc42] CImageList::Create id/handle", (ULL)nBitmapID, (ULL)(unsigned long)h);
    return h != 0;
}

/* CListCtrl::InsertItem(nMask, nItem, lpszItem, nState, nStateMask, nImage, lParam)
 * depends.exe fills every list this way: mask 7 (text, image, param), the
 * icon index chosen from the image lists above, and lParam pointing at its
 * own record for the row.  The text arrives later through LVN_GETDISPINFO. */
/*ORD 3331*/ int CListCtrl_InsertItem(void *self, DWORD nMask, int nItem,
                                      LPCSTR lpszItem, DWORD nState,
                                      DWORD nStateMask, int nImage, LL lParam)
{
    HWND h = cwnd_hwnd(self);
    if (!h) return -1;
    LVITEMA_ it;
    m_memset(&it, 0, sizeof it);
    it.mask      = nMask;
    it.iItem     = nItem;
    it.iSubItem  = 0;
    it.state     = nState;
    it.stateMask = nStateMask;
    it.pszText   = lpszItem;
    it.iImage    = nImage;
    it.lParam    = lParam;
    int rc = (int)SendMessageA(h, LVM_INSERTITEMA, 0, (LPARAM_T)(unsigned long)&it);
    return rc;
}

/* CListCtrl::GetItemData(nItem) — depends.exe stores a pointer to its own
 * module record in each row's lParam and reads it back here while it walks
 * the finished list.  While this was a stub the walk dereferenced nothing
 * and the guarded call faulted, which is what left the module list with its
 * redraw still switched off. */
#define LVM_GETITEMA 0x1005
#define LVIF_PARAM   4
/*ORD 2635*/ LL CListCtrl_GetItemData(void *self, int nItem)
{
    HWND h = cwnd_hwnd(self);
    if (!h) return 0;
    LVITEMA_ it;
    m_memset(&it, 0, sizeof it);
    it.mask  = LVIF_PARAM;
    it.iItem = nItem;
    if (!SendMessageA(h, LVM_GETITEMA, 0, (LPARAM_T)(unsigned long)&it)) return 0;
    return it.lParam;
}

/* ---- CTreeCtrl::InsertItem -------------------------------------------
 * depends.exe calls this 84 times on the module tree with nMask 0x2F and
 * lpszItem = (LPCTSTR)-1, i.e. LPSTR_TEXTCALLBACK: the control asks for each
 * label when it paints, through a TVN_GETDISPINFO notification. */
#define TVM_INSERTITEMA 0x1100
#define TVIF_TEXT 1

typedef struct {                  /* TVITEMA, Win64 layout */
    DWORD mask; void *hItem; DWORD state, stateMask;
    LPCSTR pszText; int cchTextMax; int iImage; int iSelectedImage;
    int cChildren; LL lParam;
} TVITEMA_;
typedef struct { void *hParent, *hInsertAfter; TVITEMA_ item; } TVINSERTSTRUCTA_;

/*ORD 3335*/ void *CTreeCtrl_InsertItem(void *self, DWORD mask, LPCSTR text,
                                        int image, int selImage,
                                        DWORD state, DWORD stateMask,
                                        LL lParam, void *hParent, void *hAfter)
{
    HWND h = cwnd_hwnd(self);
    if (!h) return NULL;
    TVINSERTSTRUCTA_ is;
    m_memset(&is, 0, sizeof is);
    is.hParent      = hParent;
    is.hInsertAfter = hAfter;
    is.item.mask           = mask;
    is.item.pszText        = text;
    is.item.iImage         = image;
    is.item.iSelectedImage = selImage;
    is.item.state          = state;
    is.item.stateMask      = stateMask;
    is.item.lParam         = lParam;
    return (void *)(unsigned long)SendMessageA(h, TVM_INSERTITEMA, 0,
                                               (LPARAM_T)(unsigned long)&is);
}

/* CWinThread::Run — the message pump every MFC application ends up in. */
/*ORD 4913*/ int CWinThread_Run(void *self)
{
    unsigned char msg[64];
    OutputDebugStringA("[mfc42] Run: entering message loop\n");
    while (GetMessageA(msg, NULL, 0, 0) > 0) {
        TranslateMessage(msg);
        DispatchMessageA(msg);
    }
    OutputDebugStringA("[mfc42] Run: message loop finished\n");
    return 0;
}

/* ---- AfxWinMain -------------------------------------------------------
 * WinMain in the application is a one-liner that calls this; everything
 * below is MFC's standard startup, driving the app's own overrides.
 */
/*ORD 1095*/ int AfxWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                            LPSTR lpCmdLine, int nCmdShow)
{
    int rc = -1;
    void *pApp = AfxGetApp();

    g_moduleState.m_hCurrentInstanceHandle = hInstance;
    g_moduleState.m_hCurrentResourceHandle = hInstance;

    OutputDebugStringA("[mfc42] AfxWinMain: InitApplication\n");
    if (pApp && !vcall0(pApp, VS_INITAPPLICATION)) goto done;

    OutputDebugStringA("[mfc42] AfxWinMain: InitInstance\n");
    if (!vcall0(pApp, VS_INITINSTANCE)) {
        OutputDebugStringA("[mfc42] InitInstance returned FALSE\n");
        rc = (int)vcall0(pApp, VS_EXITINSTANCE);
        goto done;
    }

    OutputDebugStringA("[mfc42] AfxWinMain: Run\n");
    rc = (int)vcall0(pApp, VS_RUN);

done:
    OutputDebugStringA("[mfc42] AfxWinMain returning\n");
    return rc;
}

int DllMain(void *inst, unsigned reason, void *reserved)
{
    m_memset(&g_moduleState, 0, sizeof g_moduleState);
    m_memset(&g_threadState, 0, sizeof g_threadState);
    cfile_init();
    OutputDebugStringA("[mfc42] AXP64 MFC starting\n");
    return 1;
}
