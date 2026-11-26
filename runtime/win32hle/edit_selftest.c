/*
 * edit_selftest.c - the EDIT control's keys, as Windows' single-line EDIT has
 * them: a control character (Enter's '\r', Escape) leaves the text and the
 * selection alone; a typed character replaces the selection; WM_GETDLGCODE
 * is DLGC_WANTARROWS | DLGC_HASSETSEL | DLGC_WANTCHARS. And Enter reaches the
 * focused field as WM_KEYDOWN, never as something else: Galactic
 * Battlegrounds' EDIT subclass takes VK_RETURN there itself (New Player's OK,
 * the chat's send).
 */
#define RECOMP_GENERATED_CODE
#define _GNU_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "win32hle.h"

#define ENTRY_VA 0x00401000u
#define WS_CHILD 0x40000000u
#define WS_VISIBLE 0x10000000u
#define WM_SETTEXT 0x000Cu
#define WM_GETTEXT 0x000Du
#define WM_GETDLGCODE 0x0087u
#define WM_KEYDOWN 0x0100u
#define WM_CHAR 0x0102u
#define EM_GETSEL 0x00B0u
#define EM_SETSEL 0x00B1u
#define PM_REMOVE 1u

static int fails;
static void ok(const char *w, int c) { if (!c) { printf("FAIL %s\n", w); fails++; } }

#define GUEST_PROLOGUE(va) \
    uint32_t _cf = 0; uint32_t _flag_k = FK_NONE; uint32_t _flag_a = 0, _flag_b = 0; RECOMP_ENTER(va)

static uint32_t VA_CreateWindowExA, VA_SetFocus, VA_PeekMessageA;
static uint32_t g_args[12], g_nargs, g_fn, g_ret;

static void guest_call(void) {
    GUEST_PROLOGUE(ENTRY_VA);
    for (int i = (int)g_nargs - 1; i >= 0; i--) PUSH32(esp, g_args[i]);
    RECOMP_ICALL(g_fn);
    g_ret = eax;
}
const recomp_dispatch_entry_t recomp_dispatch_table[] = { { ENTRY_VA, guest_call } };
const uint32_t recomp_dispatch_count = 1;

static uint32_t call(uint32_t fn, int n, const uint32_t *a) {
    for (int i = 0; i < n; i++) g_args[i] = a[i];
    g_fn = fn, g_nargs = (uint32_t)n;
    mach_enter();
    guest_call();
    mach_leave();
    return g_ret;
}
static uint32_t send(uint32_t h, uint32_t m, uint32_t w, uint32_t l) {
    mach_enter();
    uint32_t r = hle_send(h, m, w, l);
    mach_leave();
    return r;
}
static uint32_t g_buf;
static const char *text_of(uint32_t h) {
    send(h, WM_GETTEXT, 64, g_buf);
    return (const char *)hle_ptr(g_buf);
}
static uint32_t guest_str(const char *s) {
    char *p = hle_alloc((uint32_t)strlen(s) + 1);
    strcpy(p, s);
    return hle_va(p);
}

int main(void) {
    win32hle_register(win32hle_user32);
    VA_CreateWindowExA = hle_resolve("CreateWindowExA");
    VA_SetFocus = hle_resolve("SetFocus");
    VA_PeekMessageA = hle_resolve("PeekMessageA");
    ok("the shims resolve", VA_CreateWindowExA && VA_SetFocus && VA_PeekMessageA);
    g_buf = hle_va(hle_alloc(64));
    uint32_t cls_static = guest_str("Static"), cls_edit = guest_str("Edit");

    uint32_t top = call(VA_CreateWindowExA, 12, (uint32_t[]){ 0, cls_static, 0, WS_VISIBLE, 0, 0, 800, 600, 0, 0, 0, 0 });
    uint32_t ed = call(VA_CreateWindowExA, 12, (uint32_t[]){ 0, cls_edit, 0, WS_CHILD | WS_VISIBLE, 10, 10, 158, 20, top, 1, 0, 0 });
    ok("windows created", top && ed);

    ok("WM_GETDLGCODE: arrows, setsel, chars", send(ed, WM_GETDLGCODE, 0, 0) == 0x89u);

    send(ed, WM_SETTEXT, 0, guest_str("Player"));
    send(ed, EM_SETSEL, 0, (uint32_t)-1);
    ok("all of it selected", send(ed, EM_GETSEL, 0, 0) == (6u << 16));
    send(ed, WM_CHAR, '\r', 0);
    ok("Enter's '\\r' keeps a selected name", !strcmp(text_of(ed), "Player"));
    send(ed, WM_CHAR, 0x1B, 0);
    ok("Escape keeps it", !strcmp(text_of(ed), "Player"));
    ok("and the selection", send(ed, EM_GETSEL, 0, 0) == (6u << 16));
    send(ed, WM_CHAR, 'x', 0);
    ok("a letter replaces the selection", !strcmp(text_of(ed), "x"));
    send(ed, WM_CHAR, 8, 0);
    ok("Backspace deletes", !strcmp(text_of(ed), ""));

    /* Enter at the focused field: the key itself, for the game's subclass. */
    call(VA_SetFocus, 1, (uint32_t[]){ ed });
    mach_enter();
    hle_input_key(1, 0x0D, 0x1C);
    mach_leave();
    uint32_t msg = hle_va(hle_alloc(28));
    int seen = 0, other = 0;
    /* the queue as posted (WM_PAINT is made on demand, again and again: not counted) */
    for (int k = 0; k < 32 && call(VA_PeekMessageA, 5, (uint32_t[]){ msg, 0, 0, 0, PM_REMOVE }); k++) {
        if (MEM32(msg) == ed && MEM32(msg + 4) == WM_KEYDOWN && MEM32(msg + 8) == 0x0D &&
            ((MEM32(msg + 12) >> 16) & 0xFF) == 0x1C) seen++;
        else if (MEM32(msg + 4) != 0x000Fu /* WM_PAINT */) other++;
    }
    ok("Enter is a WM_KEYDOWN to the focused field", seen == 1);
    ok("and nothing else (no click)", other == 0);

    if (fails == 0) printf("edit_selftest: all checks passed\n");
    return fails != 0;
}
