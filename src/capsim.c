// capsim: minimal Wayland input-method (input-method-v2) client.
// The compositor positions our popup surface at the focused text caret and
// reports the caret rectangle. We draw a "CAPS" pill into that popup only when
//   (a text field is focused) && (caret rect is known) && (caps lock is on).
// Otherwise the popup holds a fully transparent 1x1 buffer.
#define _GNU_SOURCE
#include <cairo/cairo.h>
#include <dirent.h>
#include <math.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <wayland-client.h>
#include "input-method-unstable-v2-client-protocol.h"

#define PW 64
#define PH 20
#define S 3   // render scale; compositor downsamples (crisp on HiDPI)

static struct wl_compositor *comp;
static struct wl_shm *shm;
static struct wl_seat *seat;
static struct zwp_input_method_manager_v2 *mgr;
static struct zwp_input_method_v2 *im;
static struct zwp_input_popup_surface_v2 *popup;
static struct wl_surface *surf;
static struct wl_buffer *buf_on, *buf_off;

static int active, pending_active;
static int rect_known;
static int caps;
static int shown = -1;
static int debug;

static double bg[3] = {0.12, 0.12, 0.12}, fg[3] = {1, 1, 1}, ac[3] = {1, 1, 1};
static char font[128] = "sans";
static char theme_path[512];
static time_t theme_mtime; static ino_t theme_ino;

static void parse_hex(const char *h, double out[3]) {
    unsigned r, g, b;
    if (sscanf(h, "#%2x%2x%2x", &r, &g, &b) == 3) { out[0] = r / 255.0; out[1] = g / 255.0; out[2] = b / 255.0; }
}

// Returns 1 if the theme file changed (or first load) and colors were re-read.
static int load_theme(void) {
    struct stat st;
    if (stat(theme_path, &st)) return 0;
    if (st.st_mtime == theme_mtime && st.st_ino == theme_ino) return 0;
    theme_mtime = st.st_mtime; theme_ino = st.st_ino;
    FILE *f = fopen(theme_path, "r");
    if (!f) return 0;
    char line[256], key[64], val[64];
    while (fgets(line, sizeof line, f))
        if (sscanf(line, " %63[a-z_0-9] = \"%63[^\"]\"", key, val) == 2) {
            if (!strcmp(key, "background")) parse_hex(val, bg);
            else if (!strcmp(key, "foreground")) parse_hex(val, fg);
            else if (!strcmp(key, "accent")) parse_hex(val, ac);
        }
    fclose(f);
    FILE *p = popen("omarchy font current 2>/dev/null", "r");
    if (p) {
        char name[128];
        if (fgets(name, sizeof name, p)) { name[strcspn(name, "\n")] = 0; if (*name) strcpy(font, name); }
        pclose(p);
    }
    return 1;
}

static int caps_on(void) {
    DIR *d = opendir("/sys/class/leds");
    if (!d) return 0;
    struct dirent *e; int on = 0;
    while ((e = readdir(d))) {
        size_t n = strlen(e->d_name);
        if (n < 10 || strcmp(e->d_name + n - 10, "::capslock")) continue;
        char p[512]; snprintf(p, sizeof p, "/sys/class/leds/%s/brightness", e->d_name);
        FILE *f = fopen(p, "r"); int v = 0;
        if (f) { if (fscanf(f, "%d", &v) != 1) v = 0; fclose(f); }
        if (v > 0) on = 1;
    }
    closedir(d);
    return on;
}

static double lum(const double c[3]) { return 0.2126 * c[0] + 0.7152 * c[1] + 0.0722 * c[2]; }

static struct wl_buffer *make_buffer(int lw, int lh, int draw) {
    int w = lw * S, h = lh * S;
    int stride = w * 4, size = stride * h;
    int fd = memfd_create("capsim", MFD_CLOEXEC);
    if (fd < 0 || ftruncate(fd, size)) { perror("shm"); exit(1); }
    void *data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    memset(data, 0, size);
    if (draw) {
        cairo_surface_t *cs = cairo_image_surface_create_for_data(data, CAIRO_FORMAT_ARGB32, w, h, stride);
        cairo_t *c = cairo_create(cs);
        cairo_scale(c, S, S);
        w = lw; h = lh;
        // text/arrow: whichever theme color contrasts most with the accent fill
        const double *ink = fabs(lum(bg) - lum(ac)) > fabs(lum(fg) - lum(ac)) ? bg : fg;
        double r = 6, ww = w, hh = h;
        cairo_new_sub_path(c);
        cairo_arc(c, ww - r, r, r, -(3.14159265358979/2), 0);
        cairo_arc(c, ww - r, hh - r, r, 0, (3.14159265358979/2));
        cairo_arc(c, r, hh - r, r, (3.14159265358979/2), 2 * (3.14159265358979/2));
        cairo_arc(c, r, r, r, 2 * (3.14159265358979/2), 3 * (3.14159265358979/2));
        cairo_close_path(c);
        cairo_set_source_rgb(c, ac[0], ac[1], ac[2]);
        cairo_fill(c);
        // up-arrow with bar (caps glyph)
        cairo_set_source_rgb(c, ink[0], ink[1], ink[2]);
        cairo_move_to(c, 11, 4); cairo_line_to(c, 16, 10); cairo_line_to(c, 13, 10);
        cairo_line_to(c, 13, 13); cairo_line_to(c, 9, 13); cairo_line_to(c, 9, 10);
        cairo_line_to(c, 6, 10); cairo_close_path(c); cairo_fill(c);
        cairo_rectangle(c, 9, 15, 4, 1.5); cairo_fill(c);
        cairo_set_source_rgb(c, ink[0], ink[1], ink[2]);
        cairo_select_font_face(c, font, CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
        cairo_set_font_size(c, 11);
        cairo_move_to(c, 21, 14.5); cairo_show_text(c, "CAPS");
        cairo_destroy(c); cairo_surface_destroy(cs);
    }
    struct wl_shm_pool *pool = wl_shm_create_pool(shm, fd, size);
    struct wl_buffer *b = wl_shm_pool_create_buffer(pool, 0, lw * S, lh * S, stride, WL_SHM_FORMAT_ARGB8888);
    wl_shm_pool_destroy(pool); close(fd);
    return b;
}

static void refresh(void) {
    int want = active && rect_known && caps;
    if (want == shown) return;
    shown = want;
    if (debug) fprintf(stderr, "refresh: active=%d rect=%d caps=%d -> %s\n", active, rect_known, caps, want ? "SHOW" : "hide");
    wl_surface_attach(surf, want ? buf_on : buf_off, 0, 0);
    wl_surface_damage_buffer(surf, 0, 0, want ? PW * S : S, want ? PH * S : S);
    wl_surface_commit(surf);
}

/* input method */
static void im_activate(void *d, struct zwp_input_method_v2 *m) { pending_active = 1; }
static void im_deactivate(void *d, struct zwp_input_method_v2 *m) { pending_active = 0; }
static void im_surrounding(void *d, struct zwp_input_method_v2 *m, const char *t, uint32_t c, uint32_t a) {}
static void im_cause(void *d, struct zwp_input_method_v2 *m, uint32_t c) {}
static void im_ctype(void *d, struct zwp_input_method_v2 *m, uint32_t h, uint32_t p) {}
static void im_done(void *d, struct zwp_input_method_v2 *m) {
    if (!pending_active) rect_known = 0;
    active = pending_active;
    refresh();
}
static void im_unavailable(void *d, struct zwp_input_method_v2 *m) {
    fprintf(stderr, "capsim: another input method owns the seat (stop fcitx5/ibus first)\n");
    exit(1);
}
static const struct zwp_input_method_v2_listener im_l = {
    im_activate, im_deactivate, im_surrounding, im_cause, im_ctype, im_done, im_unavailable};

/* popup surface: caret rectangle (surface-local, text-input surface) */
static void popup_rect(void *d, struct zwp_input_popup_surface_v2 *p, int32_t x, int32_t y, int32_t w, int32_t h) {
    if (debug) fprintf(stderr, "rect %d %d %d %d\n", x, y, w, h);
    rect_known = !(x == 0 && y == 0 && w == 0 && h == 0);
    refresh();
}
static const struct zwp_input_popup_surface_v2_listener popup_l = {popup_rect};

/* registry */
static void reg_global(void *d, struct wl_registry *r, uint32_t name, const char *iface, uint32_t ver) {
    if (!strcmp(iface, "wl_compositor")) comp = wl_registry_bind(r, name, &wl_compositor_interface, 4);
    else if (!strcmp(iface, "wl_shm")) shm = wl_registry_bind(r, name, &wl_shm_interface, 1);
    else if (!strcmp(iface, "wl_seat") && !seat) seat = wl_registry_bind(r, name, &wl_seat_interface, 1);
    else if (!strcmp(iface, "zwp_input_method_manager_v2"))
        mgr = wl_registry_bind(r, name, &zwp_input_method_manager_v2_interface, 1);
}
static void reg_remove(void *d, struct wl_registry *r, uint32_t n) {}
static const struct wl_registry_listener reg_l = {reg_global, reg_remove};

int main(void) {
    debug = getenv("CAPSIM_DEBUG") != NULL;
    struct wl_display *dpy = wl_display_connect(NULL);
    if (!dpy) { fprintf(stderr, "capsim: no wayland display\n"); return 1; }
    struct wl_registry *reg = wl_display_get_registry(dpy);
    wl_registry_add_listener(reg, &reg_l, NULL);
    wl_display_roundtrip(dpy);
    if (!comp || !shm || !seat || !mgr) { fprintf(stderr, "capsim: compositor lacks input-method-v2\n"); return 1; }

    im = zwp_input_method_manager_v2_get_input_method(mgr, seat);
    zwp_input_method_v2_add_listener(im, &im_l, NULL);
    surf = wl_compositor_create_surface(comp);
    struct wl_region *empty = wl_compositor_create_region(comp);   // click-through
    wl_surface_set_input_region(surf, empty); wl_region_destroy(empty);
    popup = zwp_input_method_v2_get_input_popup_surface(im, surf);
    zwp_input_popup_surface_v2_add_listener(popup, &popup_l, NULL);
    const char *home = getenv("HOME");
    snprintf(theme_path, sizeof theme_path, "%s/.local/state/omarchy/current/theme/colors.toml", home ? home : "");
    load_theme();
    wl_surface_set_buffer_scale(surf, S);
    buf_on = make_buffer(PW, PH, 1);
    buf_off = make_buffer(1, 1, 0);   // S x S transparent px
    caps = caps_on();
    refresh();

    while (1) {
        wl_display_flush(dpy);
        while (wl_display_prepare_read(dpy) != 0) wl_display_dispatch_pending(dpy);
        struct pollfd pfd = {wl_display_get_fd(dpy), POLLIN, 0};
        int n = poll(&pfd, 1, 60);
        if (n > 0) { wl_display_read_events(dpy); wl_display_dispatch_pending(dpy); }
        else wl_display_cancel_read(dpy);
        if (n < 0) break;
        if (load_theme()) {
            wl_buffer_destroy(buf_on);
            buf_on = make_buffer(PW, PH, 1);
            if (shown == 1) { shown = -1; refresh(); }
        }
        int c = caps_on();
        if (c != caps) { caps = c; refresh(); }
    }
    return 0;
}
