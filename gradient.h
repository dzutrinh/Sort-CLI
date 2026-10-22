/*
 *	GRADIENT.H
 *	----------
 *	Color gradients for bars and matrix cells (shared with Search-CLI): presets, custom color stops,
 *	reversing, and 24-bit color when the terminal has it (nearest xterm-256
 *	color otherwise)
 *	Part of Sort-CLI, coded by Trinh D.D. Nguyen, MIT licensed
 */
#ifndef SORTCLI_GRADIENT_H
#define SORTCLI_GRADIENT_H

#define GRAD_STOPS	18
#define GRAD_LUT	64

typedef struct {
	const char * name;
	const char * desc;
	bool xterm;					/* stops are xterm-256 indices, not RGB */
	int n;
	unsigned stops[GRAD_STOPS];
} GPRESET;

static const GPRESET GPRESETS[] = {
	{ "rainbow", "Sort-CLI's red to violet", true, 18,
	  { 196, 202, 208, 214, 220, 226, 190, 154, 118, 82, 48, 51, 45, 39, 33, 27, 57, 93 } },
	{ "pastel", "Sort-CLI's soft cyan, yellow and pink", true, 18,
	  { 51, 50, 49, 48, 84, 120, 156, 192, 228, 227, 226, 220, 214, 211, 218, 212, 206, 200 } },
	{ "viridis", "purple, teal, yellow (matplotlib)", false, 10,
	  { 0x440154, 0x482878, 0x3e4989, 0x31688e, 0x26828e, 0x1f9e89, 0x35b779, 0x6ece58, 0xb5de2b, 0xfde725 } },
	{ "plasma", "blue, magenta, yellow (matplotlib)", false, 10,
	  { 0x0d0887, 0x46039f, 0x7201a8, 0x9c179e, 0xbd3786, 0xd8576b, 0xed7953, 0xfb9f3a, 0xfdca26, 0xf0f921 } },
	{ "inferno", "black, red, yellow (matplotlib)", false, 10,
	  { 0x000004, 0x1b0c41, 0x4a0c6b, 0x781c6d, 0xa52c60, 0xcf4446, 0xed6925, 0xfb9b06, 0xf7d13d, 0xfcffa4 } },
	{ "magma", "black, purple, peach (matplotlib)", false, 10,
	  { 0x000004, 0x180f3d, 0x440f76, 0x721f81, 0x9e2f7f, 0xcd4071, 0xf1605d, 0xfd9668, 0xfeca8d, 0xfcfdbf } },
	{ "cividis", "navy to yellow, colorblind friendly", false, 10,
	  { 0x00224e, 0x123570, 0x3b496c, 0x575d6d, 0x707173, 0x8a8678, 0xa59c74, 0xc3b369, 0xe1cc55, 0xfee838 } },
	{ "turbo", "a brighter, smoother rainbow (Google)", false, 15,
	  { 0x30123b, 0x4145ab, 0x4675ed, 0x39a2fc, 0x1bcfd4, 0x24eca6, 0x61fc6c, 0xa4fc3b, 0xd1e834, 0xf3c63a,
	    0xfe9b2d, 0xf36315, 0xd93806, 0xb11901, 0x7a0403 } },
	{ "ocean", "deep blue to foam", false, 5, { 0x03045e, 0x0077b6, 0x00b4d8, 0x90e0ef, 0xcaf0f8 } },
	{ "fire", "embers to flame", false, 6, { 0x3d0000, 0x9d0208, 0xdc2f02, 0xf48c06, 0xffba08, 0xfff3b0 } },
	{ "sunset", "dusk blue to peach", false, 5, { 0x355c7d, 0x6c5b7b, 0xc06c84, 0xf67280, 0xf8b195 } },
	{ "forest", "dark to light greens", false, 6, { 0x081c15, 0x1b4332, 0x2d6a4f, 0x52b788, 0x95d5b2, 0xd8f3dc } },
	{ "ice", "navy, teal, white", false, 4, { 0x0b132b, 0x1c2541, 0x5bc0be, 0xffffff } },
	{ "candy", "pink, orange, yellow, blue, purple", false, 5, { 0xff006e, 0xfb5607, 0xffbe0b, 0x3a86ff, 0x8338ec } },
	{ "cool", "cyan to magenta", false, 2, { 0x00ffff, 0xff00ff } },
	{ "claude", "shades of Claude orange", false, 3, { 0x5a2e1e, 0xd77757, 0xf7d3c4 } },
	{ "grayscale", "dark gray to white", false, 2, { 0x3a3a3a, 0xf0f0f0 } },
};
#define NGPRESETS ((int)(sizeof GPRESETS / sizeof GPRESETS[0]))

/* the current gradient */
static char g_grad_name[32] = "rainbow";
static unsigned g_grad_rgb[GRAD_STOPS];
static int g_grad_n = 0;
static bool g_grad_rev = false;
static int g_grad_lut[GRAD_LUT];
static bool g_truecolor = false;

static unsigned xterm_rgb(int n) {
	static const unsigned BASE[16] = {
		0x000000, 0x800000, 0x008000, 0x808000, 0x000080, 0x800080, 0x008080, 0xc0c0c0,
		0x808080, 0xff0000, 0x00ff00, 0xffff00, 0x0000ff, 0xff00ff, 0x00ffff, 0xffffff };
	if (n < 16) return BASE[n < 0 ? 0 : n];
	if (n >= 232) { unsigned v = 8 + (unsigned)(n - 232) * 10; return v << 16 | v << 8 | v; }
	n -= 16;
	static const unsigned L[6] = { 0, 95, 135, 175, 215, 255 };
	return L[n / 36] << 16 | L[n / 6 % 6] << 8 | L[n % 6];
}

/* nearest color among the 6x6x6 cube and the gray ramp */
static int nearest256(unsigned rgb) {
	int r = rgb >> 16 & 255, g = rgb >> 8 & 255, b = rgb & 255, best = 16;
	long bd = 1L << 40;
	int hi = r > g ? (r > b ? r : b) : (g > b ? g : b), lo = r < g ? (r < b ? r : b) : (g < b ? g : b);
	int last = hi - lo > 24 ? 231 : 255;		/* the gray ramp only for near-gray colors */
	for (int n = 16; n <= last; n++) {
		unsigned c = xterm_rgb(n);
		long dr = r - (int)(c >> 16 & 255), dg = g - (int)(c >> 8 & 255), db = b - (int)(c & 255);
		long d = 3 * dr * dr + 4 * dg * dg + 2 * db * db;		/* weighted for the eye */
		if (d < bd) { bd = d; best = n; }
	}
	return best;
}

/* color value for fg()/bg(): an xterm index, or TRUECOLOR | rgb */
static int color_of(unsigned rgb) { return g_truecolor ? (int)(TRUECOLOR | rgb) : nearest256(rgb); }

static unsigned grad_at(double t) {
	if (g_grad_rev) t = 1 - t;
	if (g_grad_n < 2) return g_grad_n ? g_grad_rgb[0] : 0x808080;
	double x = t * (g_grad_n - 1);
	int i = (int)x;
	if (i >= g_grad_n - 1) return g_grad_rgb[g_grad_n - 1];
	double f = x - i;
	unsigned a = g_grad_rgb[i], b = g_grad_rgb[i + 1], out = 0;
	for (int sh = 16; sh >= 0; sh -= 8) {
		double ca = a >> sh & 255, cb = b >> sh & 255;
		out |= (unsigned)(ca + (cb - ca) * f + 0.5) << sh;
	}
	return out;
}

static void grad_build(void) {
	for (int i = 0; i < GRAD_LUT; i++) g_grad_lut[i] = color_of(grad_at((double)i / (GRAD_LUT - 1)));
}

static void grad_use_preset(int p) {
	const GPRESET * g = &GPRESETS[p];
	snprintf(g_grad_name, sizeof g_grad_name, "%s", g->name);
	g_grad_n = g->n;
	for (int i = 0; i < g->n; i++) g_grad_rgb[i] = g->xterm ? xterm_rgb((int)g->stops[i]) : g->stops[i];
	g_grad_rev = false;
	grad_build();
}

static int grad_find(const char * name) {
	for (int i = 0; i < NGPRESETS; i++) if (!strcmp(name, GPRESETS[i].name)) return i;
	int hit = -1, len = (int)strlen(name);
	if (len >= 3)
		for (int i = 0; i < NGPRESETS; i++)
			if (!strncmp(name, GPRESETS[i].name, (size_t)len)) { if (hit >= 0) return -1; hit = i; }
	return hit;
}

/* one color stop: #rrggbb, rrggbb, #rgb or an xterm index 0-255 */
static bool grad_parse_color(const char * s, unsigned * rgb) {
	char * end;
	const char * h = s[0] == '#' ? s + 1 : s;
	size_t n = strlen(h);
	bool hex = (n == 6 || n == 3) && strspn(h, "0123456789abcdefABCDEF") == n;
	if (hex && (s[0] == '#' || n == 6) && !(n == 3 && s[0] != '#')) {
		unsigned long v = strtoul(h, &end, 16);
		if (n == 3) v = (v >> 8 & 15) * 0x110000 + (v >> 4 & 15) * 0x1100 + (v & 15) * 0x11;
		*rgb = (unsigned)v;
		return true;
	}
	long v = strtol(s, &end, 10);
	if (end != s && !*end && v >= 0 && v <= 255) { *rgb = xterm_rgb((int)v); return true; }
	return false;
}

/* map num/den (0..1) to a color in the current gradient */
static int shade(long num, long den) {
	if (den <= 0) return g_grad_lut[GRAD_LUT / 2];
	if (num < 0) num = 0;
	if (num > den) num = den;
	return g_grad_lut[(int)((num * (GRAD_LUT - 1) + den / 2) / den)];
}

static void grad_init(void) {
	const char * ct = getenv("COLORTERM");
	g_truecolor = ct && (strstr(ct, "truecolor") || strstr(ct, "24bit"));
	grad_use_preset(0);
}

#endif
