/*
 *	TEXT.H
 *	------
 *	String builder, line lists, colors, gradients, markup and word wrap
 *	Part of Sort-CLI, coded by Trinh D.D. Nguyen, MIT licensed
 */
#ifndef SORTCLI_TEXT_H
#define SORTCLI_TEXT_H

#include <stdarg.h>

/*---- STRING BUILDER ----------------------*/
typedef struct { char * s; size_t len, cap; } SB;

static void * xrealloc(void * p, size_t n) {
	p = realloc(p, n ? n : 1);
	if (!p) { fputs("search-cli: out of memory\n", stderr); exit(1); }
	return p;
}

static char * xstrdup(const char * s) {
	size_t n = strlen(s) + 1;
	return memcpy(xrealloc(NULL, n), s, n);
}

static void sb_grow(SB * b, size_t need) {
	if (b->len + need + 1 <= b->cap) return;
	size_t cap = b->cap ? b->cap : 256;
	while (cap < b->len + need + 1) cap *= 2;
	b->s = xrealloc(b->s, cap);
	b->cap = cap;
}

static void sb_putn(SB * b, const char * s, size_t n) {
	sb_grow(b, n);
	memcpy(b->s + b->len, s, n);
	b->len += n;
	b->s[b->len] = 0;
}

static void sb_puts(SB * b, const char * s) { sb_putn(b, s, strlen(s)); }
static void sb_putc(SB * b, char c)        { sb_putn(b, &c, 1); }
static void sb_rep(SB * b, const char * s, int n) { while (n-- > 0) sb_puts(b, s); }
static void sb_clear(SB * b)                { b->len = 0; if (b->s) b->s[0] = 0; }
static void sb_free(SB * b)                 { free(b->s); b->s = NULL; b->len = b->cap = 0; }
static const char * sb_str(const SB * b)    { return b->s ? b->s : ""; }

static void sb_vprintf(SB * b, const char * fmt, va_list ap) {
	va_list cp;
	va_copy(cp, ap);
	int n = vsnprintf(NULL, 0, fmt, cp);
	va_end(cp);
	if (n < 0) return;
	sb_grow(b, (size_t)n);
	vsnprintf(b->s + b->len, (size_t)n + 1, fmt, ap);
	b->len += (size_t)n;
}

static void sb_printf(SB * b, const char * fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	sb_vprintf(b, fmt, ap);
	va_end(ap);
}

/*---- LINE LISTS --------------------------*/
typedef struct { char ** v; int n, cap; } LINES;

static void lines_add(LINES * L, const char * s) {
	if (L->n == L->cap) {
		L->cap = L->cap ? L->cap * 2 : 16;
		L->v = xrealloc(L->v, sizeof(char *) * (size_t)L->cap);
	}
	L->v[L->n++] = xstrdup(s);
}

static void lines_take(LINES * L, SB * b) { lines_add(L, sb_str(b)); sb_clear(b); }

static void lines_clear(LINES * L) {
	for (int i = 0; i < L->n; i++) free(L->v[i]);
	L->n = 0;
}

static void lines_free(LINES * L) {
	lines_clear(L);
	free(L->v);
	L->v = NULL;
	L->cap = 0;
}

static void lines_append(LINES * dst, const LINES * src) {
	for (int i = 0; i < src->n; i++) lines_add(dst, src->v[i]);
}

/*---- VISIBLE WIDTH -----------------------*/
/* Every glyph this program prints is one column wide, so a code point is a column */
static const char * skip_escape(const char * s) {
	if (*s != 27) return s;
	s++;
	if (*s == '[') {
		s++;
		while (*s && !(*s >= 0x40 && *s <= 0x7E)) s++;
		if (*s) s++;
	} else if (*s) s++;
	return s;
}

static int vis_width(const char * s) {
	int w = 0;
	while (*s) {
		if (*s == 27) { s = skip_escape(s); continue; }
		if (((unsigned char)*s & 0xC0) != 0x80) w++;
		s++;
	}
	return w;
}

/* Append s cut to at most maxw columns, keeping its escape sequences */
static int sb_put_clip(SB * b, const char * s, int maxw) {
	int w = 0;
	while (*s) {
		if (*s == 27) {
			const char * e = skip_escape(s);
			sb_putn(b, s, (size_t)(e - s));
			s = e;
			continue;
		}
		if (((unsigned char)*s & 0xC0) != 0x80) {
			if (w >= maxw) break;
			w++;
		}
		sb_putc(b, *s++);
	}
	return w;
}

static void sb_pad(SB * b, int n) { while (n-- > 0) sb_putc(b, ' '); }

/* s left-aligned in a column w wide, counted in visible columns */
static void sb_cell(SB * b, const char * s, int w) {
	sb_puts(b, s);
	sb_pad(b, w - vis_width(s));
}

/*---- COLORS ------------------------------*/
#define RST		"\x1b[0m"
#define BOLD	"\x1b[1m"
#define NOBOLD	"\x1b[22m"
#define INV		"\x1b[7m"
#define DEFFG	"\x1b[39m"

/* palette (xterm 256): accent is the orange of Claude's CLI */
enum {
	C_ACCENT = 173, C_TEXT = 252, C_MUTED = 246, C_DIM = 242, C_FAINT = 238,
	C_BORDER = 244, C_CODE = 147, C_OK = 114, C_ERR = 203, C_WARN = 221,
	C_PROBE = 226, C_FOUND = 46, C_PATH = 220
};

static char g_colbuf[24][24];
static int g_coli = 0;

#define TRUECOLOR	(1 << 24)		/* flag: the low 24 bits are an RGB color */

static const char * color_seq(int layer, int c) {
	char * p = g_colbuf[g_coli++ % 24];
	if (c & TRUECOLOR) snprintf(p, 24, "\x1b[%d;2;%d;%d;%dm", layer, c >> 16 & 255, c >> 8 & 255, c & 255);
	else snprintf(p, 24, "\x1b[%d;5;%dm", layer, c);
	return p;
}

static const char * fg(int c) { return color_seq(38, c); }
static const char * bg(int c) { return color_seq(48, c); }

/*---- MARKUP AND WRAP ---------------------*/
/* `code` turns code-colored, **text** turns bold; base < 0 is the default color */
static void markup(SB * o, const char * s, int base) {
	bool code = false, bold = false;
	while (*s) {
		if (*s == '`') {
			code = !code;
			sb_puts(o, code ? fg(C_CODE) : (base < 0 ? DEFFG : fg(base)));
			s++;
		} else if (s[0] == '*' && s[1] == '*') {
			bold = !bold;
			sb_puts(o, bold ? BOLD : NOBOLD);
			s += 2;
		} else {
			sb_putc(o, *s++);
		}
	}
}

/* SGR state carried across a line break so styles survive wrapping */
typedef struct { char fg[24]; bool bold; } SGRSTATE;

static void sgr_track(SGRSTATE * st, const char * esc, size_t n) {
	if (n >= 4 && esc[n - 1] == 'm') {
		if (n == 4 && esc[2] == '0') { st->fg[0] = 0; st->bold = false; }
		else if (n == 4 && esc[2] == '1') st->bold = true;
		else if (n == 5 && esc[2] == '2' && esc[3] == '2') st->bold = false;
		else if (n == 5 && esc[2] == '3' && esc[3] == '9') st->fg[0] = 0;
		else if (n < sizeof st->fg && strncmp(esc, "\x1b[38;", 5) == 0) {
			memcpy(st->fg, esc, n);
			st->fg[n] = 0;
		}
	}
}

/* Word-wrap styled text to width columns; '\n' forces a break.
   first/next are line prefixes, prefixw their visible width */
static void wrap_text(LINES * out, const char * text, int width,
                      const char * first, const char * next, int prefixw) {
	SB line = {0};
	SGRSTATE st = {{0}, false};
	int avail = width - prefixw;
	if (avail < 8) avail = 8;
	int w = 0;
	bool fresh = true;		/* nothing on the line yet */
	sb_puts(&line, first);
	sb_puts(&line, RST);
	const char * s = text;
	while (1) {
		/* gather one word with its escapes */
		const char * start = s;
		int ww = 0;
		while (*s && *s != ' ' && *s != '\n') {
			if (*s == 27) { s = skip_escape(s); continue; }
			if (((unsigned char)*s & 0xC0) != 0x80) ww++;
			s++;
		}
		if (s > start) {
			if (!fresh && w + 1 + ww > avail) {			/* break the line */
				sb_puts(&line, RST);
				lines_take(out, &line);
				sb_puts(&line, next);
				sb_puts(&line, RST);
				if (st.bold) sb_puts(&line, BOLD);
				sb_puts(&line, st.fg);
				w = 0;
				fresh = true;
			}
			if (!fresh) { sb_putc(&line, ' '); w++; }
			SB word = {0};
			sb_putn(&word, start, (size_t)(s - start));
			w += sb_put_clip(&line, sb_str(&word), avail - w);
			for (const char * p = sb_str(&word); *p; ) {	/* follow styles */
				if (*p == 27) {
					const char * e = skip_escape(p);
					sgr_track(&st, p, (size_t)(e - p));
					p = e;
				} else p++;
			}
			sb_free(&word);
			fresh = false;
		}
		if (*s == ' ') { s++; continue; }
		if (*s == '\n') {
			s++;
			sb_puts(&line, RST);
			lines_take(out, &line);
			sb_puts(&line, next);
			sb_puts(&line, RST);
			if (st.bold) sb_puts(&line, BOLD);
			sb_puts(&line, st.fg);
			w = 0;
			fresh = true;
			continue;
		}
		break;
	}
	sb_puts(&line, RST);
	lines_take(out, &line);
	sb_free(&line);
}

#endif
