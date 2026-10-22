/*
 *	UI.H
 *	----
 *	A chat-style terminal UI in the manner of Claude's CLI: a transcript of
 *	committed blocks that scrolls normally, and a live region below it
 *	(animation, spinner, input box, hints) that is redrawn in place.
 *	Part of Sort-CLI, coded by Trinh D.D. Nguyen, MIT licensed
 */
#ifndef SORTCLI_UI_H
#define SORTCLI_UI_H

/*---- COMPANION HOOK ----------------------*/
/* A companion (an animated pet, say) can live in the live region. It may ask
   for extra rows, it draws the strip above the prompt (given the spinner or
   blank line that would otherwise go there) and it hears about activity.
   pet.h implements them with Bitsy. */
enum { CE_KEY, CE_START, CE_OK, CE_ERR };
static int companion_rows(void);
static void companion_strip(LINES * out, const char * line);
static void companion_event(int what);

/*---- TRANSCRIPT --------------------------*/
enum { B_BLANK, B_USER, B_BULLET, B_RESULT, B_TEXT, B_RAW, B_BANNER };

typedef struct {
	int kind, color;
	char * text;
	LINES raw;
} BLOCK;

#define MAX_BLOCKS	800

static BLOCK * g_blocks = NULL;
static int g_nblocks = 0, g_cblocks = 0;
static int g_flushed = 0;			/* blocks already written out */
static int g_cols = 80, g_rows = 24;
static int g_dyn = 0;				/* lines of the live region on screen */
static bool g_mute = false;			/* keep the transcript, print nothing (tests) */

static void banner_lines(LINES * out, int cols);		/* in cmds.h */
static void idle_panel(LINES * out);					/* in cmds.h */

static void block_free(BLOCK * b) {
	free(b->text);
	lines_free(&b->raw);
}

static void block_add(int kind, int color, const char * text, const LINES * raw) {
	if (g_nblocks == MAX_BLOCKS) {			/* forget the oldest quarter */
		int drop = MAX_BLOCKS / 4;
		for (int i = 0; i < drop; i++) block_free(&g_blocks[i]);
		memmove(g_blocks, g_blocks + drop, sizeof(BLOCK) * (size_t)(g_nblocks - drop));
		g_nblocks -= drop;
		g_flushed = g_flushed > drop ? g_flushed - drop : 0;
	}
	if (g_nblocks == g_cblocks) {
		g_cblocks = g_cblocks ? g_cblocks * 2 : 64;
		g_blocks = xrealloc(g_blocks, sizeof(BLOCK) * (size_t)g_cblocks);
	}
	BLOCK * b = &g_blocks[g_nblocks++];
	memset(b, 0, sizeof *b);
	b->kind = kind;
	b->color = color;
	b->text = xstrdup(text ? text : "");
	if (raw) lines_append(&b->raw, raw);
}

static void block_render(const BLOCK * b, int cols, LINES * out) {
	SB t = {0}, p = {0};
	int width = cols - 1;
	switch (b->kind) {
	case B_BLANK:
		lines_add(out, "");
		break;
	case B_USER:
		sb_printf(&t, "%s%s", fg(C_MUTED), b->text);
		sb_printf(&p, "%s> ", fg(C_MUTED));
		wrap_text(out, sb_str(&t), width, sb_str(&p), "  ", 2);
		break;
	case B_BULLET:
		markup(&t, b->text, -1);
		sb_printf(&p, "%s⏺" RST " ", fg(b->color));
		wrap_text(out, sb_str(&t), width, sb_str(&p), "  ", 2);
		break;
	case B_RESULT:
		sb_puts(&t, fg(C_MUTED));
		markup(&t, b->text, C_MUTED);
		sb_printf(&p, "  %s⎿" RST "  ", fg(C_DIM));
		wrap_text(out, sb_str(&t), width, sb_str(&p), "     ", 5);
		break;
	case B_TEXT:
		markup(&t, b->text, -1);
		wrap_text(out, sb_str(&t), width, " ", "    ", 1);
		break;
	case B_RAW:
		for (int i = 0; i < b->raw.n; i++) {
			sb_put_clip(&t, b->raw.v[i], width);
			sb_puts(&t, RST);
			lines_take(out, &t);
		}
		break;
	case B_BANNER:
		banner_lines(out, cols);
		break;
	}
	sb_free(&t);
	sb_free(&p);
}

static void say_v(int kind, int color, const char * fmt, va_list ap) {
	SB t = {0};
	sb_vprintf(&t, fmt, ap);
	block_add(kind, color, sb_str(&t), NULL);
	sb_free(&t);
}

/* ⏺ message, after a blank line */
static void say(int color, const char * fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	block_add(B_BLANK, 0, NULL, NULL);
	say_v(B_BULLET, color, fmt, ap);
	va_end(ap);
	if (color == C_ERR) companion_event(CE_ERR);
}

/* ⎿ detail under the previous message */
static void say_more(const char * fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	say_v(B_RESULT, 0, fmt, ap);
	va_end(ap);
}

static void say_text(const char * fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	say_v(B_TEXT, 0, fmt, ap);
	va_end(ap);
}

static void say_raw(const LINES * L) { block_add(B_RAW, 0, NULL, L); }

static void say_user(const char * text) {
	block_add(B_BLANK, 0, NULL, NULL);
	block_add(B_USER, 0, text, NULL);
}

/*---- PLAYER ------------------------------*/
typedef struct PLAYER PLAYER;
struct PLAYER {
	int n, k;					/* positions 0..n-1 */
	int stride;					/* positions per frame */
	bool paused;
	const char * verb;
	void (*frame)(const PLAYER *, int k, int cols, int maxrows, LINES * out);
	void * data;
};

static PLAYER * g_play = NULL;
static int g_speed = 30;				/* milliseconds per frame */
static int g_duration = 12;				/* longest run in seconds before frames skip steps */
static bool g_eof = false;

static const char * SPINNER[] = { "·", "✢", "✳", "✶", "✻", "✽", "✻", "✶", "✳", "✢" };

/* stride so n positions play within g_duration seconds */
static int auto_stride(int n) {
	long frames = (long)g_duration * 1000 / (g_speed > 0 ? g_speed : 1);
	if (frames < 1) frames = 1;
	long s = (n + frames - 1) / frames;
	return s < 1 ? 1 : (int)s;
}

/* input box: a rounded border around one line of content */
static void box_lines(LINES * out, const char * content) {
	int w = g_cols - 1;
	if (w < 12) w = 12;
	SB b = {0};
	sb_printf(&b, "%s╭", fg(C_BORDER));
	sb_rep(&b, "─", w - 2);
	sb_puts(&b, "╮" RST);
	lines_take(out, &b);
	sb_printf(&b, "%s│" RST " ", fg(C_BORDER));
	int used = sb_put_clip(&b, content, w - 4);
	sb_puts(&b, RST);
	sb_pad(&b, w - 4 - used);
	sb_printf(&b, " %s│" RST, fg(C_BORDER));
	lines_take(out, &b);
	sb_printf(&b, "%s╰", fg(C_BORDER));
	sb_rep(&b, "─", w - 2);
	sb_puts(&b, "╯" RST);
	lines_take(out, &b);
	sb_free(&b);
}

/* left and right aligned hint line under the box */
static void hint_line(LINES * out, const char * left, const char * right) {
	SB b = {0};
	int w = g_cols - 1, lw = vis_width(left), rw = right ? vis_width(right) : 0;
	sb_puts(&b, "  ");
	sb_puts(&b, fg(C_DIM));
	sb_puts(&b, left);
	if (right && 2 + lw + 2 + rw <= w - 2) {
		sb_pad(&b, w - 2 - 2 - lw - rw);
		sb_puts(&b, fg(C_DIM));
		sb_puts(&b, right);
	}
	SB c = {0};
	sb_put_clip(&c, sb_str(&b), w);
	sb_puts(&c, RST);
	lines_add(out, sb_str(&c));
	sb_free(&c);
	sb_free(&b);
}

static void play_panel(LINES * out) {
	PLAYER * p = g_play;
	LINES fr = {0};
	int avail = g_rows - 1 - 7 - companion_rows();	/* blank, frame, blank, spinner, box (3), hints */
	p->frame(p, p->k, g_cols, avail, &fr);
	for (int i = fr.n > avail ? fr.n - avail : 0; i < fr.n; i++) lines_add(out, fr.v[i]);
	lines_free(&fr);
	lines_add(out, "");
	SB b = {0};
	bool at_end = p->k >= p->n - 1;
	if (p->paused && at_end)
		sb_printf(&b, "%s✓ Done%s · step %d/%d · enter to keep the result, ← to go back", fg(C_OK), fg(C_DIM), p->k, p->n - 1);
	else if (p->paused)
		sb_printf(&b, "%s‖ Paused%s · step %d/%d · space to resume, ←/→ to step", fg(C_WARN), fg(C_DIM), p->k, p->n - 1);
	else
		sb_printf(&b, "%s%s %s…%s (step %d/%d · %d ms/frame · %d step%s/frame · esc to stop)", fg(C_ACCENT),
		          SPINNER[(now_ms() / 120) % 10], p->verb, fg(C_DIM), p->k, p->n - 1, g_speed, p->stride,
		          p->stride == 1 ? "" : "s");
	SB c = {0};
	sb_put_clip(&c, sb_str(&b), g_cols - 1);
	sb_puts(&c, RST);
	companion_strip(out, sb_str(&c));
	sb_free(&c);
	sb_clear(&b);
	sb_printf(&b, "%s> %s(the keys below drive the animation)", fg(C_MUTED), fg(C_FAINT));
	box_lines(out, sb_str(&b));
	sb_free(&b);
	hint_line(out, "space pause · ←/→ step · pgup/pgdn jump · home/end · +/- speed · enter finish · esc stop", NULL);
}

/*---- SCREEN UPDATES ----------------------*/
/* The live region is never blanked before it is redrawn (that is what made
   it flicker on terminals without synchronized output). Each update moves to
   the top of the region and overwrites it line by line, ending every line
   with "erase to end of line"; lines that did not change are skipped. */
static LINES g_shown = {0};			/* the live region as it is on screen */

static void dyn_lines(LINES * L) {
	LINES all = {0};
	if (g_play) { lines_add(&all, ""); play_panel(&all); }
	else { companion_strip(&all, ""); idle_panel(&all); }
	for (int i = all.n > g_rows - 1 ? all.n - (g_rows - 1) : 0; i < all.n; i++) lines_add(L, all.v[i]);
	lines_free(&all);
}

/* cursor from the end of the live region to the start of its first line */
static void dyn_home(SB * o) {
	sb_puts(o, "\r");
	if (g_dyn > 1) sb_printf(o, "\x1b[%dA", g_dyn - 1);
}

/* write lines from the top of the live region; old marks what is on screen
   already (NULL: nothing usable, write everything) */
static void dyn_write(SB * o, const LINES * L, const LINES * old, int have) {
	for (int i = 0; i < L->n; i++) {
		if (i) sb_puts(o, i < have ? "\r\x1b[1B" : "\r\n");	/* move down, or open a new line */
		if (old && i < old->n && i < have && !strcmp(old->v[i], L->v[i])) continue;
		sb_puts(o, L->v[i]);
		sb_puts(o, RST "\x1b[K");
	}
	if (L->n < have) sb_puts(o, "\x1b[J");				/* the region got shorter */
}

static void shown_set(const LINES * L) {
	lines_clear(&g_shown);
	lines_append(&g_shown, L);
	g_dyn = L->n;
}

/* drop the live region (on exit) */
static void dyn_erase(SB * o) {
	if (g_dyn > 0) { dyn_home(o); sb_puts(o, "\x1b[J"); }
	g_dyn = 0;
	lines_clear(&g_shown);
}

/* write new transcript blocks, then update the live region */
static void ui_flush(void) {
	SB o = {0};
	LINES L = {0}, D = {0};
	for (; g_flushed < g_nblocks; g_flushed++) block_render(&g_blocks[g_flushed], g_cols, &L);
	if (!g_interactive) {						/* piped: plain lines, no cursor control */
		for (int i = 0; i < L.n; i++) { sb_puts(&o, L.v[i]); sb_puts(&o, RST "\n"); }
	} else {
		dyn_lines(&D);
		bool same = L.n == 0 && D.n == g_shown.n && g_dyn == g_shown.n;
		for (int i = 0; i < D.n && same; i++) same = !strcmp(D.v[i], g_shown.v[i]);
		if (!same) {
			sb_puts(&o, "\x1b[?2026h");			/* synchronized update where supported */
			dyn_home(&o);
			int have = g_dyn;					/* old lines on screen we may overwrite */
			if (L.n) {							/* new transcript lines go over the old region */
				LINES none = {0};
				dyn_write(&o, &L, &none, have);
				have = have > L.n ? have - L.n : 0;
				sb_puts(&o, have ? "\r\x1b[1B" : "\r\n");
				dyn_write(&o, &D, &none, have);	/* the region moved down: write all of it */
			} else {
				dyn_write(&o, &D, &g_shown, have);
			}
			shown_set(&D);
			sb_puts(&o, "\x1b[?2026l");
		}
	}
	if (o.len && !g_mute) term_write(o.s, o.len);
	lines_free(&L);
	lines_free(&D);
	sb_free(&o);
}

/* clear the screen and print the whole transcript again at the current width */
static void ui_repaint(void) {
	if (!g_interactive) return;
	term_write("\x1b[H\x1b[2J\x1b[3J", 11);
	g_dyn = 0;
	lines_clear(&g_shown);
	g_flushed = 0;
	ui_flush();
}

static void ui_check_resize(void) {
	if (!g_interactive) return;
	int c, r;
	term_size(&c, &r);
	if (c == g_cols && r == g_rows) return;
	g_cols = c;
	g_rows = r;
	ui_repaint();
}

static void ui_clear(void) {
	for (int i = 0; i < g_nblocks; i++) block_free(&g_blocks[i]);
	g_nblocks = 0;
	g_flushed = 0;
	if (g_interactive) {
		term_write("\x1b[H\x1b[2J\x1b[3J", 11);
		g_dyn = 0;
		lines_clear(&g_shown);
	}
}

/* Animate p; true when it ran to the end, false when stopped */
static bool ui_play(PLAYER * p, bool start_paused) {
	p->k = 0;
	p->paused = start_paused;
	if (p->stride < 1) p->stride = 1;
	if (p->n <= 0) return true;
	if (!g_interactive) { p->k = p->n - 1; return true; }
	companion_event(CE_START);
	g_play = p;
	long long next = now_ms() + g_speed;
	bool finished = false, stopped = false;
	while (!finished && !stopped) {
		ui_check_resize();
		ui_flush();
		bool at_end = p->k >= p->n - 1;
		long long now = now_ms();
		if (!p->paused && now >= next) {
			if (at_end) { finished = true; break; }
			p->k += p->stride;
			if (p->k > p->n - 1) p->k = p->n - 1;
			next = now + (p->k >= p->n - 1 ? 900 : g_speed);	/* hold the last frame */
			continue;
		}
		int wait = p->paused ? 120 : (int)(next - now);
		if (wait > 120) wait = 120;			/* keep the spinner turning */
		if (wait < 0) wait = 0;
		int key = term_key(wait);
		if (key != K_TIMEOUT) companion_event(CE_KEY);
		int jump = p->stride * 10 > (p->n - 1) / 20 ? p->stride * 10 : (p->n - 1) / 20;
		switch (key) {
		case K_TIMEOUT: case K_NONE: break;
		case ' ':
			p->paused = !p->paused;
			if (!p->paused && at_end) p->k = 0;	/* replay from the start */
			next = now_ms() + g_speed;
			break;
		case K_RIGHT: case 'n': case 'l': case '.':
			p->paused = true;
			if (p->k < p->n - 1) p->k++;
			break;
		case K_LEFT: case 'p': case 'h': case ',':
			p->paused = true;
			if (p->k > 0) p->k--;
			break;
		case K_PGDN: p->paused = true; p->k = p->k + jump < p->n - 1 ? p->k + jump : p->n - 1; break;
		case K_PGUP: p->paused = true; p->k = p->k > jump ? p->k - jump : 0; break;
		case K_HOME: p->paused = true; p->k = 0; break;
		case K_END:  p->paused = true; p->k = p->n - 1; break;
		case '+': case '=':
			if (g_speed > 15) g_speed = g_speed * 2 / 3;
			else if (p->stride < 1 << 20) p->stride *= 2;
			break;
		case '-': case '_':
			if (p->stride > 1) p->stride /= 2;
			else if (g_speed < 2000) g_speed = g_speed * 3 / 2 + 1;
			break;
		case K_ENTER: case '\n':
			p->k = p->n - 1;
			finished = true;
			break;
		case K_ESC: case 'q': case K_CTRL('c'):
			if (at_end) finished = true; else stopped = true;
			break;
		case K_EOF:
			g_eof = true;
			stopped = true;
			break;
		}
	}
	g_play = NULL;
	companion_event(finished ? CE_OK : CE_ERR);		/* a stopped run is a miss */
	return finished;
}

#endif
