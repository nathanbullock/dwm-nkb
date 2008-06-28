/* See LICENSE file for copyright and license details. */

/* appearance */
#define BARPOS			BarTop /* BarBot, BarOff */
#define BORDERPX		1
#define FONT			"-*-terminus-medium-*-*-*-*-*-*-*-*-*-*-*"
#define NORMBORDERCOLOR		"#cccccc"
#define NORMBGCOLOR		"#cccccc"
#define NORMFGCOLOR		"#000000"
#define SELBORDERCOLOR		"#0066ff"
#define SELBGCOLOR		"#0066ff"
#define SELFGCOLOR		"#ffffff"

/* tagging */
const char tags[][MAXTAGLEN] = {
    "1", "2", "3", "4", "5", "6", "7", "8", "9" };

/* layout(s) */
#define MWFACT			0.5	/* master width factor [0.1 .. 0.9] */
#define RESIZEHINTS		True	/* False - respect size hints in tiled resizals */
#define SNAP			32	/* snap pixel */

Layout layouts[] = {
	/* symbol		function */
	{ "[]=",		layoutTile },
	{ "[X]",		layoutFullscreen },
};

Key KeysPrimary[] = {
    /* modifier	          key	     function	           argument */
    { Mod1Mask|ShiftMask, XK_Return, fn_exec,              "exec uxterm +sb" },
    { Mod1Mask,	          XK_q,      fn_secondaryKeys,     NULL },
    
    { Mod1Mask,	          XK_h,      fn_viewPrevWorkspace, NULL },
    { Mod1Mask,	          XK_j,      fn_focusNext,         NULL },
    { Mod1Mask,	          XK_k,      fn_nextLayout,        NULL },
    { Mod1Mask,	          XK_l,      fn_viewNextWorkspace, NULL },
};

Key KeysSecondary[] = {
    /* modifier			key		function	argument */
    { 0,            XK_0,   fn_addToAllWorkspaces,      NULL },
    { 0,            XK_1,   fn_addToWorkspace,          (char*)1 },
    { 0,            XK_2,   fn_addToWorkspace,          (char*)2 },
    { 0,            XK_3,   fn_addToWorkspace,          (char*)3 },
    { 0,            XK_4,   fn_addToWorkspace,          (char*)4 },
    
    { ShiftMask,    XK_0,   fn_removeFromAllWorkspaces, NULL },
    { ShiftMask,    XK_1,   fn_removeFromWorkspace,     (char*)1 },
    { ShiftMask,    XK_2,   fn_removeFromWorkspace,     (char*)2 },
    { ShiftMask,    XK_3,   fn_removeFromWorkspace,     (char*)3 },
    { ShiftMask,    XK_4,   fn_removeFromWorkspace,     (char*)4 },

    { Mod1Mask,     XK_0,   fn_viewWorkspace,           (char*)0 },
    { Mod1Mask,     XK_1,   fn_viewWorkspace,           (char*)1 },
    { Mod1Mask,     XK_2,   fn_viewWorkspace,           (char*)2 },
    { Mod1Mask,     XK_3,   fn_viewWorkspace,           (char*)3 },
    { Mod1Mask,     XK_4,   fn_viewWorkspace,           (char*)4 },
    
    { Mod1Mask,			XK_space,	fn_nextLayout,	NULL },
    { Mod1Mask,			XK_b,		fn_toggleBar,	NULL },
    { Mod1Mask,			XK_j,		fn_focusNext,	NULL },
    { 0,			XK_Escape,	fn_primaryKeys,	NULL },
    { Mod1Mask,			XK_h,		fn_setmwfact,	"-0.01" },
    { Mod1Mask,			XK_l,		fn_setmwfact,	"+0.01" },
    { Mod1Mask|ShiftMask,	XK_c,		fn_killWindow,	NULL },
    { Mod1Mask|ShiftMask,	XK_q,		fn_killSession,	NULL },
    { Mod1Mask,			XK_a,		fn_adjustMonitorWidth,	"-5" },
    { Mod1Mask,			XK_f,		fn_adjustMonitorWidth,	"+5" },
    { Mod1Mask,			XK_s,		fn_adjustMonitorHeight,	"-5" },
    { Mod1Mask,			XK_d,		fn_adjustMonitorHeight,	"+5" },
};


