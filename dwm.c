/*
 * See LICENSE file for copyright and license details. dynamic window
 * manager is designed like any other X client as well. It is driven
 * through handling X events. In contrast to other X clients, a window
 * manager selects for SubstructureRedirectMask on the root window, to
 * receive events about window (dis-)appearance.  Only one X connection at 
 * a time is allowed to select for this event mask. Calls to fetch an X
 * event from the event queue are blocking.  Due reading status text from
 * standard input, a select()-driven main loop has been implemented which
 * selects for reads on the X connection and STDIN_FILENO to handle all
 * data smoothly. The event handlers of dwm are organized in an array
 * which is accessed whenever a new event has been fetched. This allows
 * event dispatching in O(1) time. Each child of the root window is
 * called a client, except windows which have set the override_redirect
 * flag.  Clients are organized in a global doubly-linked client list, the 
 * focus history is remembered through a global stack list. Each client
 * contains an array of Bools of the same size as the global tags array to 
 * indicate the tags of a client. Keys and tagging rules are organized
 * as arrays and defined in config.h. To understand everything else,
 * start reading main(). 
 */
#include <errno.h>
#include <locale.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <regex.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
// #ifdef XINERAMA
#include <X11/extensions/Xinerama.h>
// #endif

/*
 * macros 
 */
#define BUTTONMASK		(ButtonPressMask | ButtonReleaseMask)
#define CLEANMASK(mask)		(mask & ~(numlockmask | LockMask))
#define LENGTH(x)		(sizeof x / sizeof x[0])
#define MAXTAGLEN		16
#define MOUSEMASK		(BUTTONMASK | PointerMotionMask)


/*
 * enums 
 */
enum { BarTop, BarBot, BarOff };        /* bar position */
enum { CurNormal, CurResize, CurMove, CurLast };        /* cursor */
enum { ColBorder, ColFG, ColBG, ColLast };      /* color */
enum { NetSupported, NetWMName, NetLast };      /* EWMH atoms */
enum { WMProtocols, WMDelete, WMName, WMState, WMLast };        /* default 
                                                                 * atoms */

/*
 * typedefs 
 */
typedef struct {
    int x, y, w, h;
    unsigned long norm[ColLast];
    unsigned long sel[ColLast];
    Drawable drawable;
    GC gc;
    struct {
        int ascent;
        int descent;
        int height;
        XFontSet set;
        XFontStruct *xfont;
    } font;
} DC;                           /* draw context */

typedef struct {
    unsigned long mod;
    KeySym keysym;
    void (*func) (const char *arg);
    const char *arg;
} Key;

typedef struct {
    const char *symbol;
    void (*arrange) (void);
} Layout;

typedef struct Client Client;
struct Client {
    Client     *c_next[10];
    Client     *c_prev[10];

    Window      c_win;
    char        c_name[256];
    int         x, y, w, h;
    int         c_xunits, c_yunits;

    /* http://tronche.com/gui/x/icccm/sec-4.html#s-4.1.2.3 */
    int         c_basew, c_baseh;
    int         c_incw, c_inch;
    int         c_minw, c_minh;
    int         c_maxw, c_maxh;
    int         c_minax, c_minay;
    int         c_maxax, c_maxay;

    unsigned    c_border;
    Bool        c_isbanned;
    Bool        c_isfixed;
    Bool        c_isfloating;
    Bool        c_isurgent;
    
    /* Don't want these. */
    Client     *snext;
    int         c_monitor;
};

typedef struct Workspaces {
    int         w_numClients[10];
    Client      w_client;
    int         w_vSplit[10];
} Workspaces;

Workspaces workspaces;
Client *rootClient = &workspaces.w_client;

typedef struct Monitor Monitor;
struct Monitor {
    int         m_screen;
    Window      m_root;
    Window      m_barwin;
    int         m_realXOrig, m_realYOrig, m_realWidth, m_realHeight;
    int         m_xorig, m_yorig, m_width, m_height;
    int         wax, way, wah, waw;
    DC          dc;
    int         m_workspace;

    /* these should probably belong to a workspace */
    Layout     *m_layout;
};

/*
 * User accessable functions.
 */
void fn_primaryKeys(const char *arg);
void fn_secondaryKeys(const char *arg);

void fn_viewNextWorkspace(const char *arg);
void fn_viewPrevWorkspace(const char *arg);
void fn_viewWorkspace(const char *arg);

void fn_addToWorkspace(const char *arg);
void fn_removeFromWorkspace(const char *arg);
void fn_addToAllWorkspaces(const char *arg);
void fn_removeFromAllWorkspaces(const char *arg);

void fn_focusNext(const char *arg);

void fn_exec(const char *arg);
void fn_killWindow(const char *arg);
void fn_killSession(const char *arg);

void layoutTile(void);
void layoutFullscreen(void);

void fn_nextLayout(const char *arg);
//void fn_prevLayout(const char *arg);
//void fn_toggleZoom(const char *arg);

void fn_adjustVSplit(const char *arg);
void fn_adjustMonitorWidth(const char *arg);
void fn_adjustMonitorHeight(const char *arg);

void fn_toggleBar(const char *arg);








void updatebarpos(Monitor * m);

void movemouse(Client * c);
void resizemouse(Client * c);

void arrange(void);

//void attachstack(Client *c);
//void detachstack(Client *c);

void focus(Client * c);

Bool isvisible(Client * c, int monitor);

void resize(Client * c, int x, int y, int w, int h, Bool sizehints);
void restack(void);
int monitorat(void);


/*
 * variables 
 */
char stext[256];
int mcount = 1;
int (*xerrorxlib) (Display *, XErrorEvent *);
unsigned int bh, bpos;
unsigned int blw = 0;
unsigned int numlockmask = 0;
Atom wmatom[WMLast], netatom[NetLast];
Bool isxinerama = False;
Bool readin;
Bool running = True;
Client *sel = NULL;
Client *stack = NULL;
Cursor cursor[CurLast];
Display *dpy;
DC dc = { 0 };
Monitor *monitors;
int selmonitor = 0;

/*
 * configuration, allows nested code to access above variables 
 */
#include "config.h"


/*********************************
 * Trace and Error functions
 *********************************/
void
TRACE(const char *errstr, ...)
{
    va_list ap;

    va_start(ap, errstr);
    vfprintf(stderr, errstr, ap);
    va_end(ap);
}

void
EXIT(const char *errstr, ...)
{
    va_list ap;

    va_start(ap, errstr);
    vfprintf(stderr, errstr, ap);
    va_end(ap);
    fprintf(stderr, "EXITING\n");
    exit(EXIT_FAILURE);
}

void*
emallocz(unsigned int size)
{
    void *res = calloc(1, size);

    if (!res)
        EXIT("fatal: could not malloc() %u bytes\n", size);
    return res;
}


/*********************************
 * Key Functionality
 *********************************/

#define KEYS_MODE_PRIMARY    1
#define KEYS_MODE_SECONDARY  2

int KeysMode = KEYS_MODE_PRIMARY;

void
keys_grabPrimary(void)
{
    unsigned int i, j;
    KeyCode code;
    XModifierKeymap *modmap;

    /*
     * init modifier map 
     */
    modmap = XGetModifierMapping(dpy);
    for (i = 0; i < 8; i++)
        for (j = 0; j < modmap->max_keypermod; j++) {
            if (modmap->modifiermap[i * modmap->max_keypermod + j] ==
                XKeysymToKeycode(dpy, XK_Num_Lock))
                numlockmask = (1 << i);
        }
    XFreeModifiermap(modmap);

    for (i = 0; i < mcount; i++) {
        Monitor *m = &monitors[i];
        XUngrabKey(dpy, AnyKey, AnyModifier, m->m_root);
        for (j = 0; j < LENGTH(KeysPrimary); j++) {
            code = XKeysymToKeycode(dpy, KeysPrimary[j].keysym);
            XGrabKey(dpy, code, KeysPrimary[j].mod, m->m_root, True,
                     GrabModeAsync, GrabModeAsync);
            XGrabKey(dpy, code, KeysPrimary[j].mod | LockMask, m->m_root,
                     True, GrabModeAsync, GrabModeAsync);
            XGrabKey(dpy, code, KeysPrimary[j].mod | numlockmask, m->m_root,
                     True, GrabModeAsync, GrabModeAsync);
            XGrabKey(dpy, code,
                     KeysPrimary[j].mod | numlockmask | LockMask, m->m_root,
                     True, GrabModeAsync, GrabModeAsync);
        }
    }
}

void
keys_grabSecondary(void)
{
    unsigned int i, j;
    XModifierKeymap *modmap;

    /*
     * init modifier map 
     */
    modmap = XGetModifierMapping(dpy);
    for (i = 0; i < 8; i++)
        for (j = 0; j < modmap->max_keypermod; j++) {
            if (modmap->modifiermap[i * modmap->max_keypermod + j] ==
                XKeysymToKeycode(dpy, XK_Num_Lock))
                numlockmask = (1 << i);
        }
    XFreeModifiermap(modmap);

    for (i = 0; i < mcount; i++) {
        Monitor *m = &monitors[i];
        XUngrabKey(dpy, AnyKey, AnyModifier, m->m_root);
        XGrabKey(dpy, AnyKey, AnyModifier, m->m_root, True,
                 GrabModeAsync, GrabModeAsync);
    }
}

void
keys_grab(void)
{
    if (KeysMode == KEYS_MODE_PRIMARY) {
        keys_grabPrimary();
    } else {
        keys_grabSecondary();
    }
}

void
keys_pressPrimary(XEvent * e)
{
    unsigned int i;
    KeySym keysym;
    XKeyEvent *ev;

    ev = &e->xkey;
    keysym = XKeycodeToKeysym(dpy, (KeyCode) ev->keycode, 0);
    for (i = 0; i < LENGTH(KeysPrimary); i++)
        if (keysym == KeysPrimary[i].keysym
            && CLEANMASK(KeysPrimary[i].mod) == CLEANMASK(ev->state)) {
            if (KeysPrimary[i].func)
                KeysPrimary[i].func(KeysPrimary[i].arg);
        }
}

void
keys_pressSecondary(XEvent * e)
{
    unsigned int i;
    KeySym keysym;
    XKeyEvent *ev;

    ev = &e->xkey;
    keysym = XKeycodeToKeysym(dpy, (KeyCode) ev->keycode, 0);
    for (i = 0; i < LENGTH(KeysSecondary); i++)
        if (keysym == KeysSecondary[i].keysym
            && CLEANMASK(KeysSecondary[i].mod) == CLEANMASK(ev->state)) {
            if (KeysSecondary[i].func)
                KeysSecondary[i].func(KeysSecondary[i].arg);
        }
}

void
keys_press(XEvent * e)
{
    if (KeysMode == KEYS_MODE_PRIMARY) {
        keys_pressPrimary(e);
    } else {
        keys_pressSecondary(e);
    }
}

void
fn_primaryKeys(const char *arg)
{
    KeysMode = KEYS_MODE_PRIMARY;
    keys_grab();
}

void
fn_secondaryKeys(const char *arg)
{
    KeysMode = KEYS_MODE_SECONDARY;
    keys_grab();
}


/*********************************
 * Workspace Functionality
 *********************************/

void
fn_viewNextWorkspace(const char *arg)
{
    Monitor *m = &monitors[monitorat()];
    m->m_workspace = (m->m_workspace + 1) % 10;
    arrange();
}

void
fn_viewPrevWorkspace(const char *arg)
{
    Monitor *m = &monitors[monitorat()];
    m->m_workspace = (m->m_workspace + 9) % 10;
    arrange();
}

void
fn_viewWorkspace(const char *arg)
{
    Monitor *m = &monitors[monitorat()];
    m->m_workspace = (int) arg;
    arrange();
}

void
ws_attach(Client * c, int workspace)
{
    if (c->c_next[workspace] != NULL) {
        // OOPS
        return;
    }

    TRACE("Adding client to %d\n", workspace);
    c->c_next[workspace] = rootClient;
    c->c_prev[workspace] = rootClient->c_prev[workspace];
    rootClient->c_prev[workspace]->c_next[workspace] = c;
    rootClient->c_prev[workspace] = c;
    workspaces.w_numClients[workspace]++;
}

void
ws_detach(Client * c, int workspace)
{
    if (c->c_next[workspace] == NULL) {
        // OOPS
        return;
    }

    c->c_next[workspace]->c_prev[workspace] = c->c_prev[workspace];
    c->c_prev[workspace]->c_next[workspace] = c->c_next[workspace];
    c->c_next[workspace] = c->c_prev[workspace] = NULL;
    workspaces.w_numClients[workspace]--;
}

void
ws_detachAll(Client * c)
{
    int w;

    for (w = 0; w < 10; w++) {
        ws_detach(c, w);
    }
}


void
fn_addToWorkspace(const char *arg)
{
    int w = (int) arg;
    if (w < 1 || w > 9) {
        // OOPS
        return;
    }

    if (!sel)
        return;

    ws_attach(sel, w);
}

void
fn_removeFromWorkspace(const char *arg)
{
    int w = (int) arg;
    TRACE("%s %d %x\n", __func__, w, sel);
    if (w < 1 || w > 9) {
        // OOPS
        return;
    }

    if (!sel)
        return;

    ws_detach(sel, w);
}

void
fn_addToAllWorkspaces(const char *arg /* not used */ )
{
    int w;
    if (!sel)
        return;

    for (w = 1; w < 10; w++) {
        ws_attach(sel, w);
    }
}

void
fn_removeFromAllWorkspaces(const char *arg /* not used */ )
{
    int w;
    if (!sel)
        return;

    for (w = 1; w < 10; w++) {
        ws_detach(sel, w);
    }
}

/*
 * Arrange Windows.
 */
void
ban(Client * c)
{
    if (c->c_isbanned)
        return;
    XMoveWindow(dpy, c->c_win, c->x + 3 * monitors[c->c_monitor].m_width, c->y);
    c->c_isbanned = True;
}

void
unban(Client * c)
{
    if (!c->c_isbanned)
        return;
    XMoveWindow(dpy, c->c_win, c->x, c->y);
    c->c_isbanned = False;
}

void
arrange(void)
{
    Client *c = rootClient->c_next[0];
    for (; c != rootClient; c = c->c_next[0]) {
        if (isvisible(c, selmonitor))
            unban(c);
        else
            ban(c);
    }

    monitors[selmonitor].m_layout->arrange();
    focus(NULL);
    restack();
}

/*
 * Layout algorithms
 */

void
layoutFullscreen(void)
{
    unsigned int i, nx, ny, nw, nh;
    Client *c;

    for (i = 0; i < mcount; i++) {
        Monitor *m = &monitors[i];
        int workspace = m->m_workspace;

        /*
         * window geoms 
         */
        c = rootClient->c_next[workspace];
        for (; c != rootClient; c = c->c_next[workspace]) {
            if (c->c_isfloating)
                continue;

            nx = m->wax;
            ny = m->way;
            nw = m->waw - 2 * c->c_border;
            nh = m->wah - 2 * c->c_border;

            resize(c, nx, ny, nw, nh, RESIZEHINTS);
            if ((RESIZEHINTS)
                && ((c->h < bh) || (c->h > nh) || (c->w < bh)
                    || (c->w > nw)))
                /*
                 * client doesn't accept size constraints 
                 */
                resize(c, nx, ny, nw, nh, False);
        }
    }
}

void
layoutTile(void)
{
    unsigned int i, j, n, nx, ny, nw, nh, mw, th;
    Client *c, *mc;

    nx = ny = nw = 0;           /* gcc stupidity requires this */

    for (i = 0; i < mcount; i++) {
        Monitor *m = &monitors[i];
        int workspace = m->m_workspace;

        c = rootClient->c_next[workspace];
        for (n = 0; c != rootClient; c = c->c_next[workspace]) {
            if (c->c_isfloating)
                continue;
            n++;
        }

        /*
         * window geoms 
         */
        mw = (n == 1) ? m->waw : workspaces.w_vSplit[workspace];
        th = (n > 1) ? m->wah / (n - 1) : 0;
        if (n > 1 && th < bh)
            th = m->wah;

        j = 0;
        c = mc = rootClient->c_next[workspace];
        for (; c != rootClient; c = c->c_next[workspace]) {
            if (c->c_isfloating)
                continue;
            if (j == 0) {       /* master */
                nx = m->wax;
                ny = m->way;
                nw = mw - 2 * c->c_border;
                nh = m->wah - 2 * c->c_border;
            } else {            /* tile window */
                if (j == 1) {
                    ny = m->way;
                    nx += mc->w + 2 * mc->c_border;
                    nw = m->waw - mw - 2 * c->c_border;
                }
                if (j + 1 == n) /* remainder */
                    nh = (m->way + m->wah) - ny - 2 * c->c_border;
                else
                    nh = th - 2 * c->c_border;
            }
            fprintf(stderr, "tile(%d, %d, %d, %d)\n", nx, ny, nw, nh);
            resize(c, nx, ny, nw, nh, RESIZEHINTS);
            if ((RESIZEHINTS)
                && ((c->h < bh) || (c->h > nh) || (c->w < bh)
                    || (c->w > nw)))
                /*
                 * client doesn't accept size constraints 
                 */
                resize(c, nx, ny, nw, nh, False);
            if (n > 1 && th != m->wah)
                ny = c->y + c->h + 2 * c->c_border;

            j++;
        }
    }
    fprintf(stderr, "done\n");
}


/*
 * Status Bar functions
 */

void
fn_toggleBar(const char *arg)
{
    if (bpos == BarOff)
        bpos = (BARPOS == BarOff) ? BarTop : BARPOS;
    else
        bpos = BarOff;
    updatebarpos(&monitors[monitorat()]);
    arrange();
}

unsigned int
textnw(Monitor * m, const char *text, unsigned int len)
{
    XRectangle r;

    if (m->dc.font.set) {
        XmbTextExtents(m->dc.font.set, text, len, NULL, &r);
        return r.width;
    }
    return XTextWidth(m->dc.font.xfont, text, len);
}

unsigned int
textw(Monitor * m, const char *text)
{
    return textnw(m, text, strlen(text)) + m->dc.font.height;
}

void
drawsquare(Monitor * m, Bool filled, Bool empty, Bool invert,
           unsigned long col[ColLast])
{
    int x;
    XGCValues gcv;
    XRectangle r = { m->dc.x, m->dc.y, m->dc.w, m->dc.h };

    gcv.foreground = col[invert ? ColBG : ColFG];
    XChangeGC(dpy, m->dc.gc, GCForeground, &gcv);
    x = (m->dc.font.ascent + m->dc.font.descent + 2) / 4;
    r.x = m->dc.x + 1;
    r.y = m->dc.y + 1;
    if (filled) {
        r.width = r.height = x + 1;
        XFillRectangles(dpy, m->dc.drawable, m->dc.gc, &r, 1);
    } else if (empty) {
        r.width = r.height = x;
        XDrawRectangles(dpy, m->dc.drawable, m->dc.gc, &r, 1);
    }
}

void
drawtext(Monitor * m, const char *text, unsigned long col[ColLast],
         Bool invert)
{
    int x, y, w, h;
    static char buf[256];
    unsigned int len, olen;
    XRectangle r = { m->dc.x, m->dc.y, m->dc.w, m->dc.h };

    XSetForeground(dpy, m->dc.gc, col[invert ? ColFG : ColBG]);
    XFillRectangles(dpy, m->dc.drawable, m->dc.gc, &r, 1);
    if (!text)
        return;
    w = 0;
    olen = len = strlen(text);
    if (len >= sizeof buf)
        len = sizeof buf - 1;
    memcpy(buf, text, len);
    buf[len] = 0;
    h = m->dc.font.ascent + m->dc.font.descent;
    y = m->dc.y + (m->dc.h / 2) - (h / 2) + m->dc.font.ascent;
    x = m->dc.x + (h / 2);
    /*
     * shorten text if necessary 
     */
    while (len && (w = textnw(m, buf, len)) > m->dc.w - h)
        buf[--len] = 0;
    if (len < olen) {
        if (len > 1)
            buf[len - 1] = '.';
        if (len > 2)
            buf[len - 2] = '.';
        if (len > 3)
            buf[len - 3] = '.';
    }
    if (w > m->dc.w)
        return;                 /* too long */
    XSetForeground(dpy, m->dc.gc, col[invert ? ColBG : ColFG]);
    if (m->dc.font.set)
        XmbDrawString(dpy, m->dc.drawable, m->dc.font.set, m->dc.gc, x, y,
                      buf, len);
    else
        XDrawString(dpy, m->dc.drawable, m->dc.gc, x, y, buf, len);
}

Bool
isoccupied(unsigned int monitor, unsigned int t)
{
    return True;
}

Bool
isurgent(unsigned int monitor, unsigned int t)
{
    // if(c->c_monitor == monitor && c->isurgent && c->tags[t])
    return True;
}

void
drawbar(void)
{
    char text[512];
    int i, j, x;
    Client *c = NULL;

    for (i = 0; i < mcount; i++) {
        Monitor *m = &monitors[i];
        m->dc.x = 0;
        c = sel;
        fprintf(stderr, "m%d %s\n", i, c ? c->c_name : "NIL");
        for (j = 0; j < LENGTH(tags); j++) {
            m->dc.w = textw(m, tags[j]);
            if (m->m_workspace == ((j + 1) % 10)) {     /* seltags */
                drawtext(m, tags[j], m->dc.sel, isurgent(i, j));
                drawsquare(m, c && c->c_monitor == i,
                           isoccupied(i, j), isurgent(i, j), m->dc.sel);
            } else {
                drawtext(m, tags[j], m->dc.norm, isurgent(i, j));
                drawsquare(m, c && c->c_monitor == i,
                           isoccupied(i, j), isurgent(i, j), m->dc.norm);
            }
            m->dc.x += m->dc.w;
        }
        m->dc.w = blw;
        drawtext(m, m->m_layout->symbol, m->dc.norm, False);
        x = m->dc.x + m->dc.w;
        if (i == selmonitor) {
            m->dc.w = textw(m, stext);
            m->dc.x = m->m_width - m->dc.w;
            if (m->dc.x < x) {
                m->dc.x = x;
                m->dc.w = m->m_width - x;
            }
            drawtext(m, stext, m->dc.norm, False);
        } else {
            m->dc.x = m->m_width;
        }
        if ((m->dc.w = m->dc.x - x) > bh) {
            m->dc.x = x;
            if (c) {
                sprintf(text, "(%d,%d) %s", c->c_xunits, c->c_yunits, c->c_name);
                drawtext(m, text, m->dc.sel, False);
                drawsquare(m, False, c->c_isfloating, False, m->dc.sel);
            } else
                drawtext(m, NULL, m->dc.norm, False);
        }
        XCopyArea(dpy, m->dc.drawable, m->m_barwin, m->dc.gc, 0, 0, m->m_width,
                  bh, 0, 0);
        XSync(dpy, False);
    }
}

/*
 * Stop/Stop applications and dwm.
 */

void
fn_exec(const char *arg)
{
    static char *shell = NULL;

    if (!shell && !(shell = getenv("SHELL")))
        shell = "/bin/sh";
    if (!arg)
        return;

    /*
     * The double-fork construct avoids zombie processes and keeps the
     * code clean from stupid signal handlers. 
     */
    if (fork() == 0) {
        if (fork() == 0) {
            if (dpy)
                close(ConnectionNumber(dpy));
            setsid();
            execl(shell, shell, "-c", arg, (char *) NULL);
            fprintf(stderr, "dwm: execl '%s -c %s'", shell, arg);
            perror(" failed");
        }
        exit(0);
    }
    wait(0);
}

Bool
isprotodel(Client * c)
{
    int i, n;
    Atom *protocols;
    Bool ret = False;

    if (XGetWMProtocols(dpy, c->c_win, &protocols, &n)) {
        for (i = 0; !ret && i < n; i++)
            if (protocols[i] == wmatom[WMDelete])
                ret = True;
        XFree(protocols);
    }
    return ret;
}

void
fn_killWindow(const char *arg)
{
    XEvent ev;

    if (!sel)
        return;
    if (isprotodel(sel)) {
        ev.type = ClientMessage;
        ev.xclient.window = sel->c_win;
        ev.xclient.message_type = wmatom[WMProtocols];
        ev.xclient.format = 32;
        ev.xclient.data.l[0] = wmatom[WMDelete];
        ev.xclient.data.l[1] = CurrentTime;
        XSendEvent(dpy, sel->c_win, False, NoEventMask, &ev);
    } else
        XKillClient(dpy, sel->c_win);
}

void
fn_killSession(const char *arg)
{
    readin = running = False;
}






/*
void
attachstack(Client * c)
{
    c->snext = stack;
    stack = c;
}
*/
void
configure(Client * c)
{
    XConfigureEvent ce;

    ce.type = ConfigureNotify;
    ce.display = dpy;
    ce.event = c->c_win;
    ce.window = c->c_win;
    ce.x = c->x;
    ce.y = c->y;
    ce.width = c->w;
    ce.height = c->h;
    ce.border_width = c->c_border;
    ce.above = None;
    ce.override_redirect = False;
    XSendEvent(dpy, c->c_win, False, StructureNotifyMask, (XEvent *) & ce);
}


/*
void
detachstack(Client * c)
{
    Client **tc;

    for (tc = &stack; *tc && *tc != c; tc = &(*tc)->snext);
    *tc = c->snext;
}
*/


void
grabbuttons(Client * c, Bool focused)
{
    XUngrabButton(dpy, AnyButton, AnyModifier, c->c_win);

    if (focused) {
        XGrabButton(dpy, Button1, Mod1Mask, c->c_win, False, BUTTONMASK,
                    GrabModeAsync, GrabModeSync, None, None);
        XGrabButton(dpy, Button1, Mod1Mask | LockMask, c->c_win, False,
                    BUTTONMASK, GrabModeAsync, GrabModeSync, None, None);
        XGrabButton(dpy, Button1, Mod1Mask | numlockmask, c->c_win, False,
                    BUTTONMASK, GrabModeAsync, GrabModeSync, None, None);
        XGrabButton(dpy, Button1, Mod1Mask | numlockmask | LockMask, c->c_win,
                    False, BUTTONMASK, GrabModeAsync, GrabModeSync, None,
                    None);

        XGrabButton(dpy, Button2, Mod1Mask, c->c_win, False, BUTTONMASK,
                    GrabModeAsync, GrabModeSync, None, None);
        XGrabButton(dpy, Button2, Mod1Mask | LockMask, c->c_win, False,
                    BUTTONMASK, GrabModeAsync, GrabModeSync, None, None);
        XGrabButton(dpy, Button2, Mod1Mask | numlockmask, c->c_win, False,
                    BUTTONMASK, GrabModeAsync, GrabModeSync, None, None);
        XGrabButton(dpy, Button2, Mod1Mask | numlockmask | LockMask, c->c_win,
                    False, BUTTONMASK, GrabModeAsync, GrabModeSync, None,
                    None);

        XGrabButton(dpy, Button3, Mod1Mask, c->c_win, False, BUTTONMASK,
                    GrabModeAsync, GrabModeSync, None, None);
        XGrabButton(dpy, Button3, Mod1Mask | LockMask, c->c_win, False,
                    BUTTONMASK, GrabModeAsync, GrabModeSync, None, None);
        XGrabButton(dpy, Button3, Mod1Mask | numlockmask, c->c_win, False,
                    BUTTONMASK, GrabModeAsync, GrabModeSync, None, None);
        XGrabButton(dpy, Button3, Mod1Mask | numlockmask | LockMask, c->c_win,
                    False, BUTTONMASK, GrabModeAsync, GrabModeSync, None,
                    None);
    } else
        XGrabButton(dpy, AnyButton, AnyModifier, c->c_win, False, BUTTONMASK,
                    GrabModeAsync, GrabModeSync, None, None);
}

void
focus(Client * c)
{
    Monitor *m;

    if (c)
        selmonitor = c->c_monitor;

    m = &monitors[selmonitor];
    if (!c || (c && !isvisible(c, selmonitor)))
        for (c = stack; c && !isvisible(c, c->c_monitor); c = c->snext);
    if (sel && sel != c) {
        grabbuttons(sel, False);
        XSetWindowBorder(dpy, sel->c_win,
                         monitors[sel->c_monitor].dc.norm[ColBorder]);
    }
    if (c) {
        //detachstack(c);
        //attachstack(c);
        grabbuttons(c, True);
    }
    sel = c;
    drawbar();
    if (c) {
        XSetWindowBorder(dpy, c->c_win, m->dc.sel[ColBorder]);
        XSetInputFocus(dpy, c->c_win, RevertToPointerRoot, CurrentTime);
        selmonitor = c->c_monitor;
    } else {
        XSetInputFocus(dpy, m->m_root, RevertToPointerRoot, CurrentTime);
        drawbar();
    }
}

void
fn_focusNext(const char *arg)
{
    int workspace = monitors[selmonitor].m_workspace;
    Client *c;

    TRACE("%s\n", __func__);
    if (sel)
        c = sel->c_next[workspace];
    else
        c = rootClient;

    if (workspaces.w_numClients[workspace] > 0) {
        for (; c == rootClient; c = c->c_next[workspace]);
    } else {
        c = NULL;
    }

    focus(c);
    restack();
}

Client *
getclient(Window w)
{
    Client *c = rootClient->c_next[0];
    for (; c != rootClient; c = c->c_next[0]) {
        if (c->c_win == w)
            return c;
    }
    return NULL;
}

Bool
isvisible(Client * c, int monitor)
{
    Monitor *m = &monitors[monitor];
    return c->c_next[m->m_workspace] != NULL;
}


int
monitorat()
{
    int i, x, y;
    Window win;
    unsigned int mask;

    XQueryPointer(dpy, monitors[selmonitor].m_root, &win, &win, &x, &y, &i,
                  &i, &mask);
    for (i = 0; i < mcount; i++) {
        if ((x >= monitors[i].m_xorig && x < monitors[i].m_xorig + monitors[i].m_width)
            && (y >= monitors[i].m_yorig
                && y < monitors[i].m_yorig + monitors[i].m_height)) {
            return i;
        }
    }
    return 0;
}

void
resize(Client * c, int x, int y, int w, int h, Bool sizehints)
{
    XWindowChanges wc;
    // Monitor scr = monitors[monitorat()];
    // c->c_monitor = monitorat();

    if (sizehints) {
        /*
         * set minimum possible 
         */
        if (w < 1)
            w = 1;
        if (h < 1)
            h = 1;

        /*
         * temporarily remove base dimensions 
         */
        w -= c->c_basew;
        h -= c->c_baseh;

        /*
         * adjust for aspect limits 
         */
        if (c->c_minay > 0 && c->c_maxay > 0 && c->c_minax > 0 && c->c_maxax > 0) {
            if (w * c->c_maxay > h * c->c_maxax)
                w = h * c->c_maxax / c->c_maxay;
            else if (w * c->c_minay < h * c->c_minax)
                h = w * c->c_minay / c->c_minax;
        }

        /*
         * adjust for increment value 
         */
        c->c_xunits = w / c->c_incw;
        c->c_yunits = h / c->c_inch;
        
        w = c->c_xunits * c->c_incw;
        h = c->c_yunits * c->c_inch;
        /*
        c->c_yunits = h / c->c_inch;

        if (c->c_incw)
            w -= w % c->c_incw;
        if (c->c_inch)
            h -= h % c->c_inch;
*/
        /*
         * restore base dimensions 
         */
        w += c->c_basew;
        h += c->c_baseh;

        if (c->c_minw > 0 && w < c->c_minw)
            w = c->c_minw;
        if (c->c_minh > 0 && h < c->c_minh)
            h = c->c_minh;
        if (c->c_maxw > 0 && w > c->c_maxw)
            w = c->c_maxw;
        if (c->c_maxh > 0 && h > c->c_maxh)
            h = c->c_maxh;
    }
    if (w <= 0 || h <= 0)
        return;
    /*
     * TODO: offscreen appearance fixes 
     */
    /*
     * if(x > scr.sw) x = scr.sw - w - 2 * c->c_border; if(y > scr.sh) y =
     * scr.sh - h - 2 * c->c_border; if(x + w + 2 * c->c_border < scr.sx) x =
     * scr.sx; if(y + h + 2 * c->c_border < scr.sy) y = scr.sy; 
     */
    if (c->x != x || c->y != y || c->w != w || c->h != h) {
        c->x = wc.x = x;
        c->y = wc.y = y;
        c->w = wc.width = w;
        c->h = wc.height = h;
        wc.border_width = c->c_border;
        XConfigureWindow(dpy, c->c_win,
                         CWX | CWY | CWWidth | CWHeight | CWBorderWidth,
                         &wc);
        configure(c);
        XSync(dpy, False);
    }
}
void
restack(void)
{
    unsigned int i;
    Client *c;
    XEvent ev;
    XWindowChanges wc;

    drawbar();
    if (!sel)
        return;
    if (sel->c_isfloating)
        XRaiseWindow(dpy, sel->c_win);
    wc.stack_mode = Below;
    wc.sibling = monitors[selmonitor].m_barwin;
    if (!sel->c_isfloating) {
        XConfigureWindow(dpy, sel->c_win, CWSibling | CWStackMode, &wc);
        wc.sibling = sel->c_win;
    }
    for (i = 0; i < mcount; i++) {
        int workspace = monitors[i].m_workspace;
        c = rootClient->c_next[workspace];
        for (; c != rootClient; c = c->c_next[workspace]) {
            if (c->c_isfloating)
                continue;
            if (c == sel)
                continue;
            XConfigureWindow(dpy, c->c_win, CWSibling | CWStackMode,
                             &wc);
            wc.sibling = c->c_win;
        }
    }
    XSync(dpy, False);
    while (XCheckMaskEvent(dpy, EnterWindowMask, &ev));
}

void
fn_nextLayout(const char *arg)
{
    unsigned int i;
    Monitor *m = &monitors[monitorat()];

    TRACE("%s\n", __func__);
    if (!arg) {
        m->m_layout++;
        if (m->m_layout == &layouts[LENGTH(layouts)])
            m->m_layout = &layouts[0];
    } else {
        for (i = 0; i < LENGTH(layouts); i++)
            if (!strcmp(arg, layouts[i].symbol))
                break;
        if (i == LENGTH(layouts))
            return;
        m->m_layout = &layouts[i];
    }
    arrange();
    drawbar();
}

void
fn_adjustMonitorWidth(const char *arg)
{
    Monitor *m = &monitors[monitorat()];
    int delta;

    if (arg == NULL)
        m->m_width = m->m_realWidth;
    else if (sscanf(arg, "%d", &delta) == 1) {
        if (arg[0] == '+' || arg[0] == '-')
            m->m_width += delta;
        else
            m->m_width = delta;
        if (m->m_width < 300)
            m->m_width = 300;
        else if (m->m_width > m->m_realWidth)
            m->m_width = m->m_realWidth;
    }
    updatebarpos(m);
    arrange();
}

void
fn_adjustMonitorHeight(const char *arg)
{
    Monitor *m = &monitors[monitorat()];
    int delta;

    if (arg == NULL)
        m->m_height = m->m_realHeight;
    else if (sscanf(arg, "%d", &delta) == 1) {
        if (arg[0] == '+' || arg[0] == '-')
            m->m_height += delta;
        else
            m->m_height = delta;
        if (m->m_height < 300)
            m->m_height = 300;
        else if (m->m_height > m->m_realHeight)
            m->m_height = m->m_realHeight;
    }
    updatebarpos(m);
    arrange();
}

void
fn_adjustVSplit(const char *arg)
{
    Monitor *m = &monitors[monitorat()];
    int vsplit = workspaces.w_vSplit[m->m_workspace];
    int delta;
    
    if (arg == NULL)
        vsplit = VSPLIT;
    else if (sscanf(arg, "%d", &delta) == 1) {
        if (arg[0] == '+' || arg[0] == '-')
            vsplit += delta;
        else
            vsplit = delta;
        if (vsplit < 30)
            vsplit = 30;
        else if (vsplit > m->m_width)
            vsplit = m->m_width;
    }

    workspaces.w_vSplit[m->m_workspace] = vsplit;
    arrange();
}

void
togglefloating(const char *arg)
{
    if (!sel)
        return;
    sel->c_isfloating = !sel->c_isfloating;
    if (sel->c_isfloating)
        resize(sel, sel->x, sel->y, sel->w, sel->h, True);
    arrange();
}

void
updatebarpos(Monitor * m)
{
    XEvent ev;

    m->wax = m->m_xorig;
    m->way = m->m_yorig;
    m->wah = m->m_height;
    m->waw = m->m_width;
    switch (bpos) {
    default:
        m->wah -= bh;
        m->way += bh;
        XMoveWindow(dpy, m->m_barwin, m->m_xorig, m->m_yorig);
        break;
    case BarBot:
        m->wah -= bh;
        XMoveWindow(dpy, m->m_barwin, m->m_xorig, m->m_yorig + m->wah);
        break;
    case BarOff:
        XMoveWindow(dpy, m->m_barwin, m->m_xorig, m->m_yorig - bh);
        break;
    }
    XSync(dpy, False);
    while (XCheckMaskEvent(dpy, EnterWindowMask, &ev));
}


/*
 * There's no way to check accesses to destroyed windows, thus those cases 
 * are ignored (especially on UnmapNotify's).  Other types of errors call
 * Xlibs default error handler, which may call exit.  
 */
int
xerror(Display * dpy, XErrorEvent * ee)
{
    if (ee->error_code == BadWindow
        || (ee->request_code == X_SetInputFocus
            && ee->error_code == BadMatch)
        || (ee->request_code == X_PolyText8
            && ee->error_code == BadDrawable)
        || (ee->request_code == X_PolyFillRectangle
            && ee->error_code == BadDrawable)
        || (ee->request_code == X_PolySegment
            && ee->error_code == BadDrawable)
        || (ee->request_code == X_ConfigureWindow
            && ee->error_code == BadMatch)
        || (ee->request_code == X_GrabKey && ee->error_code == BadAccess)
        || (ee->request_code == X_CopyArea
            && ee->error_code == BadDrawable))
        return 0;
    fprintf(stderr, "dwm: fatal error: request code=%d, error code=%d\n",
            ee->request_code, ee->error_code);
    return xerrorxlib(dpy, ee); /* may call exit */
}

int
xerrordummy(Display * dsply, XErrorEvent * ee)
{
    return 0;
}


/*
 * Event Handlers.
 */

void
buttonpress(XEvent * e)
{
    unsigned int i, x;
    Client *c;
    XButtonPressedEvent *ev = &e->xbutton;

    Monitor *m = &monitors[monitorat()];

    if (ev->window == m->m_barwin) {
        x = 0;
        for (i = 0; i < LENGTH(tags); i++) {
            x += textw(m, tags[i]);
            if (ev->x < x) {
                fn_viewWorkspace((const char*)((i+1) % 10));
                return;
            }
        }
        if ((ev->x < x + blw) && ev->button == Button1)
            fn_nextLayout(NULL);
    } else if ((c = getclient(ev->window))) {
        focus(c);
        if (CLEANMASK(ev->state) != Mod1Mask)
            return;
        if (ev->button == Button1) {
            restack();
            movemouse(c);
        } else if (ev->button == Button3 && !c->c_isfixed) {
            restack();
            resizemouse(c);
        }
    }
}

void
configurerequest(XEvent * e)
{
    Client *c;
    XConfigureRequestEvent *ev = &e->xconfigurerequest;
    XWindowChanges wc;

    if ((c = getclient(ev->window))) {
        Monitor *m = &monitors[c->c_monitor];
        if (ev->value_mask & CWBorderWidth)
            c->c_border = ev->border_width;

        if (c->c_isfixed || c->c_isfloating) {
            if (ev->value_mask & CWX)
                c->x = m->m_xorig + ev->x;
            if (ev->value_mask & CWY)
                c->y = m->m_yorig + ev->y;
            if (ev->value_mask & CWWidth)
                c->w = ev->width;
            if (ev->value_mask & CWHeight)
                c->h = ev->height;
            if ((c->x - m->m_xorig + c->w) > m->m_width && c->c_isfloating)
                c->x = m->m_xorig + (m->m_width / 2 - c->w / 2);  /* center in x
                                                         * direction */
            if ((c->y - m->m_yorig + c->h) > m->m_width && c->c_isfloating)
                c->y = m->m_yorig + (m->m_height / 2 - c->h / 2);  /* center in y
                                                         * direction */
            if ((ev->value_mask & (CWX | CWY))
                && !(ev->value_mask & (CWWidth | CWHeight)))
                configure(c);
            if (isvisible(c, monitorat()))
                XMoveResizeWindow(dpy, c->c_win, c->x, c->y, c->w, c->h);
        } else {
            configure(c);
        }
    } else {
        wc.x = ev->x;
        wc.y = ev->y;
        wc.width = ev->width;
        wc.height = ev->height;
        wc.border_width = ev->border_width;
        wc.sibling = ev->above;
        wc.stack_mode = ev->detail;
        XConfigureWindow(dpy, ev->window, ev->value_mask, &wc);
    }
    XSync(dpy, False);
}

void
configurenotify(XEvent * e)
{
    XConfigureEvent *ev = &e->xconfigure;
    Monitor *m = &monitors[selmonitor];

    if (ev->window == m->m_root &&
        (ev->width != m->m_width || ev->height != m->m_height))
    {
        m->m_width = ev->width;
        m->m_height = ev->height;
        XFreePixmap(dpy, dc.drawable);
        dc.drawable =
            XCreatePixmap(dpy, m->m_root, m->m_width, bh,
                          DefaultDepth(dpy, m->m_screen));
        XResizeWindow(dpy, m->m_barwin, m->m_width, bh);
        updatebarpos(m);
        arrange();
    }
}

void
enternotify(XEvent * e)
{
    Client *c;
    XCrossingEvent *ev = &e->xcrossing;

    if (ev->mode != NotifyNormal || ev->detail == NotifyInferior) {
        if (!isxinerama || ev->window != monitors[selmonitor].m_root)
            return;
    }
    if ((c = getclient(ev->window)))
        focus(c);
    else {
        selmonitor = monitorat();
        fprintf(stderr, "updating selmonitor %d\n", selmonitor);
        focus(NULL);
    }
}

void
expose(XEvent * e)
{
    XExposeEvent *ev = &e->xexpose;

    if (ev->count == 0) {
        if (ev->window == monitors[selmonitor].m_barwin)
            drawbar();
    }
}

void
focusin(XEvent * e)
{                               /* there are some broken focus acquiring
                                 * clients */
    XFocusChangeEvent *ev = &e->xfocus;

    if (sel && ev->window != sel->c_win)
        XSetInputFocus(dpy, sel->c_win, RevertToPointerRoot, CurrentTime);
}

void
mappingnotify(XEvent * e)
{
    XMappingEvent *ev = &e->xmapping;

    XRefreshKeyboardMapping(ev);
    if (ev->request == MappingKeyboard)
        keys_grab();
}


/*
 * Misc Properties.
 */

void
updatesizehints(Client *c)
{
    long msize;
    XSizeHints size;

    if (!XGetWMNormalHints(dpy, c->c_win, &size, &msize) || !size.flags)
        size.flags = PSize;

    if (size.flags & PBaseSize) {
        c->c_basew = size.base_width;
        c->c_baseh = size.base_height;
    } else if (size.flags & PMinSize) {
        c->c_basew = size.min_width;
        c->c_baseh = size.min_height;
    } else {
        c->c_basew = c->c_baseh = 0;
    }

    if (size.flags & PResizeInc) {
        c->c_incw = size.width_inc;
        c->c_inch = size.height_inc;
    }
    c->c_inch = (c->c_inch > 1) ? c->c_inch : 1;
    c->c_incw = (c->c_incw > 1) ? c->c_incw : 1;

    if (size.flags & PMaxSize) {
        c->c_maxw = size.max_width;
        c->c_maxh = size.max_height;
    } else {
        c->c_maxw = c->c_maxh = 0;
    }

    if (size.flags & PMinSize) {
        c->c_minw = size.min_width;
        c->c_minh = size.min_height;
    } else if (size.flags & PBaseSize) {
        c->c_minw = size.base_width;
        c->c_minh = size.base_height;
    } else {
        c->c_minw = c->c_minh = 0;
    }

    if (size.flags & PAspect) {
        c->c_minax = size.min_aspect.x;
        c->c_maxax = size.max_aspect.x;
        c->c_minay = size.min_aspect.y;
        c->c_maxay = size.max_aspect.y;
    } else {
        c->c_minax = c->c_maxax = c->c_minay = c->c_maxay = 0;
    }

    c->c_isfixed = (c->c_maxw && c->c_minw && c->c_maxh && c->c_minh
                  && c->c_maxw == c->c_minw && c->c_maxh == c->c_minh);
}

Bool
gettextprop(Window w, Atom atom, char *text, unsigned int size)
{
    char **list = NULL;
    int n;
    XTextProperty name;

    if (!text || size == 0)
        return False;
    text[0] = '\0';
    XGetTextProperty(dpy, w, &name, atom);
    if (!name.nitems)
        return False;
    if (name.encoding == XA_STRING)
        strncpy(text, (char *) name.value, size - 1);
    else {
        if (XmbTextPropertyToTextList(dpy, &name, &list, &n) >= Success
            && n > 0 && *list) {
            strncpy(text, *list, size - 1);
            XFreeStringList(list);
        }
    }
    text[size - 1] = '\0';
    XFree(name.value);
    return True;
}

void
updatetitle(Client *c)
{
    if (!gettextprop(c->c_win, netatom[NetWMName], c->c_name, sizeof c->c_name))
        gettextprop(c->c_win, wmatom[WMName], c->c_name, sizeof c->c_name);
}

void
updatewmhints(Client *c)
{
    XWMHints *wmh;

    if ((wmh = XGetWMHints(dpy, c->c_win))) {
        c->c_isurgent = (wmh->flags & XUrgencyHint) ? True : False;
        XFree(wmh);
    }
}

void
propertynotify(XEvent * e)
{
    Client *c;
    Window trans;
    XPropertyEvent *ev = &e->xproperty;

    if (ev->state == PropertyDelete)
        return;                 /* ignore */
    if ((c = getclient(ev->window))) {
        switch (ev->atom) {
        default:
            break;
        case XA_WM_TRANSIENT_FOR:
            XGetTransientForHint(dpy, c->c_win, &trans);
            if (!c->c_isfloating
                && (c->c_isfloating = (getclient(trans) != NULL)))
                arrange();
            break;
        case XA_WM_NORMAL_HINTS:
            updatesizehints(c);
            break;
        case XA_WM_HINTS:
            updatewmhints(c);
            drawbar();
            break;
        }
        if (ev->atom == XA_WM_NAME || ev->atom == netatom[NetWMName]) {
            updatetitle(c);
            if (c == sel)
                drawbar();
        }
    }
}

/*
 * Managing windows.
 */

void
setclientstate(Client * c, long state)
{
    long data[] = { state, None };

    XChangeProperty(dpy, c->c_win, wmatom[WMState], wmatom[WMState], 32,
                    PropModeReplace, (unsigned char *) data, 2);
}

void
manage(Window w, XWindowAttributes * wa)
{
    Client *c;
    Monitor *m;
    Status rettrans;
    Window trans;
    XWindowChanges wc;

    c = emallocz(sizeof(Client));
    c->c_win = w;

    m = &monitors[c->c_monitor];

    c->x = wa->x + m->m_xorig;
    c->y = wa->y + m->m_yorig;
    c->w = wa->width;
    c->h = wa->height;

    if (c->w == m->m_width && c->h == m->m_height) {
        c->x = m->m_xorig;
        c->y = m->m_yorig;
        c->c_border = wa->border_width;
    } else {
        if (c->x + c->w + 2 * c->c_border > m->wax + m->waw)
            c->x = m->wax + m->waw - c->w - 2 * c->c_border;
        if (c->y + c->h + 2 * c->c_border > m->way + m->wah)
            c->y = m->way + m->wah - c->h - 2 * c->c_border;
        if (c->x < m->wax)
            c->x = m->wax;
        if (c->y < m->way)
            c->y = m->way;
        c->c_border = BORDERPX;
    }
    wc.border_width = c->c_border;
    XConfigureWindow(dpy, w, CWBorderWidth, &wc);
    XSetWindowBorder(dpy, w, m->dc.norm[ColBorder]);
    configure(c);               /* propagates border_width, if size
                                 * doesn't change */
    updatesizehints(c);
    XSelectInput(dpy, w,
                 EnterWindowMask | FocusChangeMask | PropertyChangeMask |
                 StructureNotifyMask);
    grabbuttons(c, False);
    updatetitle(c);
    // This is dealing with popup windows/dialog boxes/etc
    if ((rettrans = XGetTransientForHint(dpy, w, &trans) == Success))
        TRACE("TRANSIENT WINDOW\n");
    // for(t = clients; t && t->c_win != trans; t = t->next);
    // if(t)
    // memcpy(c->tags, t->tags, sizeof initags);
    if (!c->c_isfloating)
        c->c_isfloating = (rettrans == Success) || c->c_isfixed;
    //attachstack(c);
    ws_attach(c, 0);
    ws_attach(c, monitors[selmonitor].m_workspace);
    XMoveResizeWindow(dpy, c->c_win, c->x, c->y, c->w, c->h);     /* some
                                                                 * windows 
                                                                 * * * * *
                                                                 * require
                                                                 * * * *
                                                                 * this */
    ban(c);
    XMapWindow(dpy, c->c_win);
    setclientstate(c, NormalState);
    arrange();
}

void
maprequest(XEvent * e)
{
    static XWindowAttributes wa;
    XMapRequestEvent *ev = &e->xmaprequest;

    if (!XGetWindowAttributes(dpy, ev->window, &wa))
        return;
    if (wa.override_redirect)
        return;
    if (!getclient(ev->window))
        manage(ev->window, &wa);
}

void
unmanage(Client *c)
{
    XWindowChanges wc;

    /*
     * The server grab construct avoids race conditions. 
     */
    XGrabServer(dpy);
    XSetErrorHandler(xerrordummy);
    XConfigureWindow(dpy, c->c_win, CWBorderWidth, &wc);  /* restore c_border */
    ws_detachAll(c);
    //detachstack(c);
    if (sel == c)
        focus(NULL);
    XUngrabButton(dpy, AnyButton, AnyModifier, c->c_win);
    setclientstate(c, WithdrawnState);
    free(c);
    XSync(dpy, False);
    XSetErrorHandler(xerror);
    XUngrabServer(dpy);
    arrange();
}

void
unmapnotify(XEvent * e)
{
    Client *c;
    XUnmapEvent *ev = &e->xunmap;

    if ((c = getclient(ev->window)))
        unmanage(c);
}

void
destroynotify(XEvent * e)
{
    Client *c;
    XDestroyWindowEvent *ev = &e->xdestroywindow;

    if ((c = getclient(ev->window)))
        unmanage(c);
}

void (*handler[LASTEvent]) (XEvent *) = {
        [ButtonPress] = buttonpress,
        [ConfigureRequest] = configurerequest,
        [ConfigureNotify] = configurenotify,
        [EnterNotify] = enternotify,
        [Expose] = expose,
        [FocusIn] = focusin,
        [KeyPress] = keys_press,
        [MappingNotify] = mappingnotify,
        [PropertyNotify] = propertynotify,
        
        [MapRequest] = maprequest,
        [UnmapNotify] = unmapnotify,
        [DestroyNotify] = destroynotify,
};

void
movemouse(Client * c)
{
    int x1, y1, ocx, ocy, di, nx, ny;
    unsigned int dui;
    Window dummy;
    XEvent ev;

    ocx = nx = c->x;
    ocy = ny = c->y;
    if (XGrabPointer
        (dpy, monitors[selmonitor].m_root, False, MOUSEMASK, GrabModeAsync,
         GrabModeAsync, None, cursor[CurMove], CurrentTime) != GrabSuccess)
        return;
    XQueryPointer(dpy, monitors[selmonitor].m_root, &dummy, &dummy, &x1, &y1,
                  &di, &di, &dui);
    for (;;) {
        XMaskEvent(dpy,
                   MOUSEMASK | ExposureMask | SubstructureRedirectMask,
                   &ev);
        switch (ev.type) {
        case ButtonRelease:
            XUngrabPointer(dpy, CurrentTime);
            return;
        case ConfigureRequest:
        case Expose:
        case MapRequest:
            handler[ev.type] (&ev);
            break;
        case MotionNotify:
            XSync(dpy, False);
            nx = ocx + (ev.xmotion.x - x1);
            ny = ocy + (ev.xmotion.y - y1);
            Monitor *m = &monitors[monitorat()];
            if (abs(m->wax - nx) < SNAP)
                nx = m->wax;
            else if (abs((m->wax + m->waw) - (nx + c->w + 2 * c->c_border)) <
                     SNAP)
                nx = m->wax + m->waw - c->w - 2 * c->c_border;
            if (abs(m->way - ny) < SNAP)
                ny = m->way;
            else if (abs((m->way + m->wah) - (ny + c->h + 2 * c->c_border)) <
                     SNAP)
                ny = m->way + m->wah - c->h - 2 * c->c_border;
            if ((abs(nx - c->x) > SNAP) || (abs(ny - c->y) > SNAP))
                togglefloating(NULL);
            if (c->c_isfloating)
                resize(c, nx, ny, c->w, c->h, False);
            /*
             * memcpy(c->tags, monitors[monitorat()].seltags, sizeof
             * initags); 
             */
            break;
        }
    }
}

void
resizemouse(Client * c)
{
    int ocx, ocy;
    int nw, nh;
    XEvent ev;

    ocx = c->x;
    ocy = c->y;
    if (XGrabPointer
        (dpy, monitors[selmonitor].m_root, False, MOUSEMASK, GrabModeAsync,
         GrabModeAsync, None, cursor[CurResize],
         CurrentTime) != GrabSuccess)
        return;
    XWarpPointer(dpy, None, c->c_win, 0, 0, 0, 0, c->w + c->c_border - 1,
                 c->h + c->c_border - 1);
    for (;;) {
        XMaskEvent(dpy,
                   MOUSEMASK | ExposureMask | SubstructureRedirectMask,
                   &ev);
        switch (ev.type) {
        case ButtonRelease:
            XWarpPointer(dpy, None, c->c_win, 0, 0, 0, 0,
                         c->w + c->c_border - 1, c->h + c->c_border - 1);
            XUngrabPointer(dpy, CurrentTime);
            while (XCheckMaskEvent(dpy, EnterWindowMask, &ev));
            return;
        case ConfigureRequest:
        case Expose:
        case MapRequest:
            handler[ev.type] (&ev);
            break;
        case MotionNotify:
            XSync(dpy, False);
            if ((nw = ev.xmotion.x - ocx - 2 * c->c_border + 1) <= 0)
                nw = 1;
            if ((nh = ev.xmotion.y - ocy - 2 * c->c_border + 1) <= 0)
                nh = 1;
            if (abs(nw - c->w) > SNAP || abs(nh - c->h) > SNAP)
                togglefloating(NULL);
            if (c->c_isfloating)
                resize(c, c->x, c->y, nw, nh, True);
            break;
        }
    }
}


/*
 * The following is all the basic startup code.
 */

/*
 * Startup Error handler to check if another window manager is already
 * running. 
 */
Bool otherwm;

int
xerrorstart(Display * dsply, XErrorEvent * ee)
{
    otherwm = True;
    return -1;
}

void
checkotherwm(void)
{
    otherwm = False;
    XSetErrorHandler(xerrorstart);

    /*
     * this causes an error if some other window manager is running 
     */
    XSelectInput(dpy, DefaultRootWindow(dpy), SubstructureRedirectMask);
    XSync(dpy, False);
    if (otherwm) {
        EXIT("dwm: another window manager is already running\n");
    }

    XSync(dpy, False);
    XSetErrorHandler(NULL);
    xerrorxlib = XSetErrorHandler(xerror);
    XSync(dpy, False);
}

unsigned long
getcolor(const char *colstr, int screen)
{
    Colormap cmap = DefaultColormap(dpy, screen);
    XColor color;

    if (!XAllocNamedColor(dpy, cmap, colstr, &color, &color))
        EXIT("error, cannot allocate color '%s'\n", colstr);
    return color.pixel;
}

void
initfont(Monitor * m, const char *fontstr)
{
    char *def, **missing;
    int i, n;

    missing = NULL;
    if (m->dc.font.set)
        XFreeFontSet(dpy, m->dc.font.set);
    m->dc.font.set = XCreateFontSet(dpy, fontstr, &missing, &n, &def);
    if (missing) {
        while (n--)
            fprintf(stderr, "dwm: missing fontset: %s\n", missing[n]);
        XFreeStringList(missing);
    }
    if (m->dc.font.set) {
        XFontSetExtents *font_extents;
        XFontStruct **xfonts;
        char **font_names;
        m->dc.font.ascent = m->dc.font.descent = 0;
        font_extents = XExtentsOfFontSet(m->dc.font.set);
        n = XFontsOfFontSet(m->dc.font.set, &xfonts, &font_names);
        for (i = 0, m->dc.font.ascent = 0, m->dc.font.descent = 0; i < n;
             i++) {
            if (m->dc.font.ascent < (*xfonts)->ascent)
                m->dc.font.ascent = (*xfonts)->ascent;
            if (m->dc.font.descent < (*xfonts)->descent)
                m->dc.font.descent = (*xfonts)->descent;
            xfonts++;
        }
    } else {
        if (m->dc.font.xfont)
            XFreeFont(dpy, m->dc.font.xfont);
        m->dc.font.xfont = NULL;
        if (!(m->dc.font.xfont = XLoadQueryFont(dpy, fontstr))
            && !(m->dc.font.xfont = XLoadQueryFont(dpy, "fixed")))
            EXIT("error, cannot load font: '%s'\n", fontstr);
        m->dc.font.ascent = m->dc.font.xfont->ascent;
        m->dc.font.descent = m->dc.font.xfont->descent;
    }
    m->dc.font.height = m->dc.font.ascent + m->dc.font.descent;
}

void
setup(void)
{
    unsigned int i, j, k;
    Monitor *m;
    XSetWindowAttributes wa;
    XineramaScreenInfo *info = NULL;

    /*
     * init atoms 
     */
    wmatom[WMProtocols] = XInternAtom(dpy, "WM_PROTOCOLS", False);
    wmatom[WMDelete] = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
    wmatom[WMName] = XInternAtom(dpy, "WM_NAME", False);
    wmatom[WMState] = XInternAtom(dpy, "WM_STATE", False);
    netatom[NetSupported] = XInternAtom(dpy, "_NET_SUPPORTED", False);
    netatom[NetWMName] = XInternAtom(dpy, "_NET_WM_NAME", False);

    /*
     * init cursors 
     */
    wa.cursor = cursor[CurNormal] = XCreateFontCursor(dpy, XC_left_ptr);
    cursor[CurResize] = XCreateFontCursor(dpy, XC_sizing);
    cursor[CurMove] = XCreateFontCursor(dpy, XC_fleur);

    for (i = 0; i < 10; i++) {
        workspaces.w_numClients[i] = 0;
        workspaces.w_client.c_next[i] = rootClient;
        workspaces.w_client.c_prev[i] = rootClient;
        workspaces.w_vSplit[i] = VSPLIT;
    }

    // init screens/monitors first
    mcount = 1;
    if ((isxinerama = XineramaIsActive(dpy))) {
        info = XineramaQueryScreens(dpy, &mcount);
    }
    monitors = emallocz(mcount * sizeof(Monitor));

    for (i = 0; i < mcount; i++) {
        /*
         * init geometry 
         */
        m = &monitors[i];

        m->m_workspace = 1;
        m->m_screen = isxinerama ? 0 : i;
        m->m_root = RootWindow(dpy, m->m_screen);

        if (mcount != 1 && isxinerama) {
            m->m_realXOrig = info[i].x_org;
            m->m_realYOrig = info[i].y_org;
            m->m_realWidth = info[i].width;
            m->m_realWidth = info[i].height;
        } else {
            m->m_realXOrig = m->m_realYOrig = 0;
            m->m_realWidth = DisplayWidth(dpy, m->m_screen);
            m->m_realHeight = DisplayHeight(dpy, m->m_screen);
        }

        m->m_xorig = m->m_realXOrig;
        m->m_yorig = m->m_realYOrig;
        m->m_width = m->m_realWidth;
        m->m_height = m->m_realHeight;
        fprintf(stderr, "monitor[%d]: %d,%d,%d,%d\n",
                i, m->m_xorig, m->m_yorig, m->m_width, m->m_height);

        /*
         * init appearance 
         */
        m->dc.norm[ColBorder] = getcolor(NORMBORDERCOLOR, m->m_screen);
        m->dc.norm[ColBG] = getcolor(NORMBGCOLOR, m->m_screen);
        m->dc.norm[ColFG] = getcolor(NORMFGCOLOR, m->m_screen);
        m->dc.sel[ColBorder] = getcolor(SELBORDERCOLOR, m->m_screen);
        m->dc.sel[ColBG] = getcolor(SELBGCOLOR, m->m_screen);
        m->dc.sel[ColFG] = getcolor(SELFGCOLOR, m->m_screen);
        initfont(m, FONT);
        m->dc.h = bh = m->dc.font.height + 2;

        /*
         * init layouts 
         */
        m->m_layout = &layouts[0];
        for (blw = k = 0; k < LENGTH(layouts); k++) {
            j = textw(m, layouts[k].symbol);
            if (j > blw)
                blw = j;
        }

        // TODO: bpos per screen?
        bpos = BARPOS;
        wa.override_redirect = 1;
        wa.background_pixmap = ParentRelative;
        wa.event_mask = ButtonPressMask | ExposureMask;

        /*
         * init bars 
         */
        m->m_barwin = XCreateWindow(dpy, m->m_root, m->m_xorig, m->m_yorig, m->m_width, bh, 0,
                                  DefaultDepth(dpy, m->m_screen),
                                  CopyFromParent, DefaultVisual(dpy,
                                                                m->m_screen),
                                  CWOverrideRedirect | CWBackPixmap |
                                  CWEventMask, &wa);
        XDefineCursor(dpy, m->m_barwin, cursor[CurNormal]);
        updatebarpos(m);
        XMapRaised(dpy, m->m_barwin);
        strcpy(stext, "dwm-" VERSION);
        m->dc.drawable = XCreatePixmap(dpy, m->m_root, m->m_width, bh,
                                       DefaultDepth(dpy, m->m_screen));
        m->dc.gc = XCreateGC(dpy, m->m_root, 0, 0);
        XSetLineAttributes(dpy, m->dc.gc, 1, LineSolid, CapButt,
                           JoinMiter);
        if (!m->dc.font.set)
            XSetFont(dpy, m->dc.gc, m->dc.font.xfont->fid);

        /*
         * EWMH support per monitor 
         */
        XChangeProperty(dpy, m->m_root, netatom[NetSupported], XA_ATOM, 32,
                        PropModeReplace, (unsigned char *) netatom,
                        NetLast);

        /*
         * select for events 
         */
        wa.event_mask = SubstructureRedirectMask | SubstructureNotifyMask
            | EnterWindowMask | LeaveWindowMask | StructureNotifyMask;
        XChangeWindowAttributes(dpy, m->m_root, CWEventMask | CWCursor, &wa);
        XSelectInput(dpy, m->m_root, wa.event_mask);
    }
    if (info)
        XFree(info);

    /*
     * grab keys 
     */
    keys_grab();

    selmonitor = monitorat();
    fprintf(stderr, "selmonitor == %d\n", selmonitor);
}

long
getstate(Window w)
{
    int format, status;
    long result = -1;
    unsigned char *p = NULL;
    unsigned long n, extra;
    Atom real;

    status =
        XGetWindowProperty(dpy, w, wmatom[WMState], 0L, 2L, False,
                           wmatom[WMState], &real, &format, &n, &extra,
                           (unsigned char **) &p);
    if (status != Success)
        return -1;
    if (n != 0)
        result = *p;
    XFree(p);
    return result;
}

void
scan(void)
{
    unsigned int i, j, num;
    Window *wins, d1, d2;
    XWindowAttributes wa;

    for (i = 0; i < mcount; i++) {
        Monitor *m = &monitors[i];
        wins = NULL;
        if (XQueryTree(dpy, m->m_root, &d1, &d2, &wins, &num)) {
            for (j = 0; j < num; j++) {
                if (!XGetWindowAttributes(dpy, wins[j], &wa) ||
                    wa.override_redirect ||
                    XGetTransientForHint(dpy, wins[j], &d1)) {
                    continue;
                }

                if (wa.map_state == IsViewable ||
                    getstate(wins[j]) == IconicState) {
                    manage(wins[j], &wa);
                }
            }
            for (j = 0; j < num; j++) { /* now the transients */
                if (!XGetWindowAttributes(dpy, wins[j], &wa)) {
                    continue;
                }
                if (XGetTransientForHint(dpy, wins[j], &d1) &&
                    (wa.map_state == IsViewable ||
                     getstate(wins[j]) == IconicState)) {
                    manage(wins[j], &wa);
                }
            }
        }
        if (wins)
            XFree(wins);
    }
}

void
run(void)
{
    char *p;
    char buf[sizeof stext];
    fd_set rd;
    int r, xfd;
    unsigned int len, offset;
    XEvent ev;

    /*
     * main event loop, also reads status text from stdin 
     */
    XSync(dpy, False);
    xfd = ConnectionNumber(dpy);
    readin = True;
    offset = 0;
    len = sizeof stext - 1;
    buf[len] = stext[len] = '\0';       /* 0-terminator is never touched */
    while (running) {
        FD_ZERO(&rd);
        if (readin)
            FD_SET(STDIN_FILENO, &rd);
        FD_SET(xfd, &rd);
        if (select(xfd + 1, &rd, NULL, NULL, NULL) == -1) {
            if (errno == EINTR)
                continue;
            EXIT("select failed\n");
        }
        if (FD_ISSET(STDIN_FILENO, &rd)) {
            switch ((r = read(STDIN_FILENO, buf + offset, len - offset))) {
            case -1:
                strncpy(stext, strerror(errno), len);
                readin = False;
                break;
            case 0:
                strncpy(stext, "EOF", 4);
                readin = False;
                break;
            default:
                for (p = buf + offset; r > 0; p++, r--, offset++)
                    if (*p == '\n' || *p == '\0') {
                        *p = '\0';
                        strncpy(stext, buf, len);
                        p += r - 1;     /* p is buf + offset + r - 1 */
                        for (r = 0; *(p - r) && *(p - r) != '\n'; r++);
                        offset = r;
                        if (r)
                            memmove(buf, p - r + 1, r);
                        break;
                    }
                break;
            }
            drawbar();
        }
        while (XPending(dpy)) {
            XNextEvent(dpy, &ev);
            if (handler[ev.type])
                (handler[ev.type]) (&ev);       /* call handler */
        }
    }
}

void
cleanup(void)
{
    unsigned int i;
    close(STDIN_FILENO);
    while (stack) {
        unban(stack);
        unmanage(stack);
    }
    for (i = 0; i < mcount; i++) {
        Monitor *m = &monitors[i];
        if (m->dc.font.set)
            XFreeFontSet(dpy, m->dc.font.set);
        else
            XFreeFont(dpy, m->dc.font.xfont);
        XUngrabKey(dpy, AnyKey, AnyModifier, m->m_root);
        XFreePixmap(dpy, m->dc.drawable);
        XFreeGC(dpy, m->dc.gc);
        XDestroyWindow(dpy, m->m_barwin);
        XFreeCursor(dpy, cursor[CurNormal]);
        XFreeCursor(dpy, cursor[CurResize]);
        XFreeCursor(dpy, cursor[CurMove]);
        XSetInputFocus(dpy, PointerRoot, RevertToPointerRoot, CurrentTime);
        XSync(dpy, False);
    }
}

int
main(int argc, char *argv[])
{
    if (argc == 2 && !strcmp("-v", argv[1])) {
        EXIT("dwm-" VERSION
             ", © 2006-2007 Anselm R. Garbe, Sander van Dijk, "
             "Jukka Salmi, Premysl Hruby, Szabolcs Nagy, Christof Musik\n");
    } else if (argc != 1) {
        EXIT("usage: dwm [-v]\n");
    }

    setlocale(LC_CTYPE, "");
    if (!(dpy = XOpenDisplay(0))) {
        EXIT("dwm: cannot open display\n");
    }

    checkotherwm();
    setup();
    drawbar();
    scan();
    run();
    cleanup();

    XCloseDisplay(dpy);
    return 0;
}
