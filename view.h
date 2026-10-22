/*
 *	VIEW.H
 *	------
 *	A view is one or more lanes (replayed traces) plus how to draw them:
 *	1D bars, a 2D heatmap (flat, snake, rows or columns), or a race of
 *	several algorithms stacked as compact bar charts
 *	Part of Sort-CLI, coded by Trinh D.D. Nguyen, MIT licensed
 */
#ifndef SORTCLI_VIEW_H
#define SORTCLI_VIEW_H

enum { M_1D, M_FLAT, M_SNAKE, M_ROWS, M_COLS, M_MODES };
static const char * MODE_NAMES[M_MODES] = { "1d", "flat", "snake", "rows", "cols" };
static const char * MODE_DESC[M_MODES] = {
	"one row of bars",
	"2D, sorted as one sequence in reading order",
	"2D, sorted as one sequence in snake order",
	"2D, every row sorted on its own, all at once",
	"2D, every column sorted on its own, all at once",
};

enum { V_BARS, V_HEAT, V_RACE };
#define MAX_LANES	64
#define MAX_RACE	8

typedef struct {
	int kind, map;			/* view kind, M_* layout for heatmaps */
	int w, h;				/* matrix size */
	int nl;
	TRACE T[MAX_LANES];
	LANE L[MAX_LANES];
	int alg[MAX_LANES];		/* algorithm per lane */
	int total;				/* longest trace */
	bool overflow;
} VIEW;

static void view_free(VIEW * v) {
	for (int i = 0; i < v->nl; i++) { lane_free(&v->L[i]); trace_free(&v->T[i]); }
	v->nl = 0;
}

/* matrix cell (x, y) of element i in lane l */
static void cell_of(const VIEW * v, int l, int i, int * x, int * y) {
	switch (v->map) {
	case M_SNAKE: *y = i / v->w; *x = (*y & 1) ? v->w - 1 - i % v->w : i % v->w; break;
	case M_ROWS:  *y = l; *x = i; break;
	case M_COLS:  *x = l; *y = i; break;
	default:      *y = i / v->w; *x = i % v->w;
	}
}

/* traces only (no lanes): used for playback and for /compare */
static bool view_run(VIEW * v, int kind, int map, int w, int h, const SAMPLES * s, const int * algs, int nalgs) {
	static EL seq[MAX_CELLS];
	memset(v, 0, sizeof *v);
	v->kind = kind;
	v->map = map;
	v->w = w;
	v->h = h;
	if (kind == V_HEAT && ALGS[algs[0]].only2d) v->map = map = M_FLAT;	/* shear sort works on the matrix itself */
	int lanes = kind == V_RACE ? nalgs : map == M_ROWS ? h : map == M_COLS ? w : 1;
	int len = map == M_ROWS ? w : map == M_COLS ? h : s->n;
	for (int l = 0; l < lanes; l++) {
		v->alg[l] = kind == V_RACE ? algs[l] : algs[0];
		v->nl = l + 1;
		for (int i = 0; i < len; i++) {
			int x = i, y = 0;
			if (kind == V_HEAT) cell_of(v, l, i, &x, &y);
			seq[i] = kind == V_HEAT ? s->a[y * w + x] : s->a[i];
			seq[i].id = i;
		}
		if (!sort_run(&ALGS[v->alg[l]], seq, len, w, h, &v->T[l])) v->overflow = true;
		if (v->T[l].n > v->total) v->total = v->T[l].n;
	}
	return !v->overflow;
}

static void view_lanes(VIEW * v) {
	for (int l = 0; l < v->nl; l++) lane_init(&v->L[l], &v->T[l], v->nl);
}

static void view_seek(VIEW * v, int k) {
	for (int l = 0; l < v->nl; l++) lane_seek(&v->L[l], k);
}

/*---- describing the latest event ---------*/
static void describe(SB * b, const LANE * L) {
	int i = L->pos - 1;
	while (i >= 0 && L->T->ev[i].op == OP_NOTE) i--;		/* the note has its own line */
	if (i < 0) { sb_puts(b, "Start"); return; }
	const EV * e = &L->T->ev[i];
	const EL * a = L->a;
	switch (e->op) {
	case OP_CMP:
		if (e->a != NIL && e->b != NIL) sb_printf(b, "compare a[%d] = %d with a[%d] = %d", e->a, a[e->a].v, e->b, a[e->b].v);
		else if (e->a != NIL) sb_printf(b, "compare a held value with a[%d] = %d", e->a, a[e->a].v);
		else sb_puts(b, "compare two values in the buffer");
		break;
	case OP_SWAP:  sb_printf(b, "swap a[%d] ↔ a[%d]  (now %d, %d)", e->a, e->b, a[e->a].v, a[e->b].v); break;
	case OP_WRITE: sb_printf(b, "write %d into a[%d]", e->v, e->a); break;
	case OP_DONE:
		if (e->a == e->b) sb_printf(b, "a[%d] = %d is in its final place", e->a, a[e->a].v);
		else sb_printf(b, "a[%d..%d] is in its final place", e->a, e->b);
		break;
	case OP_FOCUS:
		if (e->a == NIL) sb_puts(b, "focus cleared");
		else sb_printf(b, "work on a[%d..%d]", e->a, e->b);
		break;
	case OP_PIVOT:
		if (e->a == NIL) sb_puts(b, "pivot released");
		else sb_printf(b, "pivot a[%d] = %d", e->a, a[e->a].v);
		break;
	default: sb_puts(b, "…");
	}
}

/*---- 1D BARS -----------------------------*/
static const char * EIGHTHS[9] = { " ", "▁", "▂", "▃", "▄", "▅", "▆", "▇", "█" };

/* columns per element for n bars in avail columns; 0 when nothing fits */
static int bar_width(int n, int avail, int maxdigits, bool * values) {
	int vw = (maxdigits < 2 ? 2 : maxdigits) + 1;
	*values = false;
	if (n * vw <= avail) { *values = true; return vw; }
	if (n * 3 <= avail) return 3;
	if (n * 2 <= avail) return 2;
	return 1;
}

typedef struct { int c1, c2, w1, w2; } HL;

static void bar_rows(LINES * out, const LANE * L, int H, int cw, int mn, int mx, const HL * hl, SB * line, int maxw) {
	int n = L->T->len, levels = H * 8, glyphs = cw > 1 ? cw - 1 : 1;
	for (int r = 0; r < H; r++) {
		sb_puts(line, "  ");
		for (int i = 0; i < n; i++) {
			int v = L->a[i].v;
			int lvl = 1 + (int)((long)(v - mn) * (levels - 1) / (mx > mn ? mx - mn : 1));
			int e = lvl - (H - 1 - r) * 8;
			const char * col;
			if (i == hl->c1 || i == hl->c2 || i == hl->w1 || i == hl->w2) col = fg(231);
			else if (i == L->m.pivot) col = fg(201);
			else col = fg(shade(v - mn, mx - mn));
			sb_puts(line, col);
			sb_rep(line, EIGHTHS[e <= 0 ? 0 : e >= 8 ? 8 : e], glyphs);
			if (cw > 1) sb_putc(line, ' ');
		}
		SB c = {0};
		sb_put_clip(&c, sb_str(line), maxw);
		sb_puts(&c, RST);
		lines_add(out, sb_str(&c));
		sb_free(&c);
		sb_clear(line);
	}
}

static void add_line(LINES * out, SB * line, int maxw) {
	SB c = {0};
	sb_put_clip(&c, sb_str(line), maxw);
	sb_puts(&c, RST);
	lines_add(out, sb_str(&c));
	sb_free(&c);
	sb_clear(line);
}

/* markers under the bars: ▲ compare, ◆ swap/write, P pivot, ━ final */
static void marker_row(SB * line, const LANE * L, int cw, const HL * hl) {
	int n = L->T->len, glyphs = cw > 1 ? cw - 1 : 1;
	sb_puts(line, "  ");
	for (int i = 0; i < n; i++) {
		const char * mark = NULL;
		int col = 0;
		if (i == hl->w1 || i == hl->w2)      { mark = "◆"; col = C_ERR; }
		else if (i == hl->c1 || i == hl->c2) { mark = "▲"; col = C_PROBE; }
		else if (i == L->m.pivot)            { mark = "P"; col = 201; }
		if (mark) { sb_pad(line, glyphs - 1); sb_printf(line, "%s%s", fg(col), mark); }
		else if (L->dn[i]) { sb_puts(line, fg(C_FOUND)); sb_rep(line, "━", glyphs); }
		else sb_pad(line, glyphs);
		if (cw > 1) sb_putc(line, ' ');
	}
}

static void focus_row(SB * line, const LANE * L, int cw) {
	int n = L->T->len, lo = L->m.lo, hi = L->m.hi;
	sb_puts(line, "  ");
	if (lo < 0 || hi < lo || hi >= n) return;
	int start = lo * cw, end = hi * cw + (cw > 1 ? cw - 2 : 0);
	sb_pad(line, start);
	sb_puts(line, fg(C_ACCENT));
	if (start == end) sb_puts(line, "│");
	else { sb_puts(line, "╰"); sb_rep(line, "─", end - start - 1); sb_puts(line, "╯"); }
	sb_printf(line, "%s a[%d..%d]", fg(C_DIM), lo, hi);
}

static void stats_line(SB * line, const VIEW * v, int k) {
	long cm = 0, sw = 0, wr = 0;
	for (int l = 0; l < v->nl; l++) { cm += v->L[l].m.cmps; sw += v->L[l].m.swaps; wr += v->L[l].m.writes; }
	sb_printf(line, "  %sstep %d/%d · comparisons %s%ld%s · swaps %s%ld%s · writes %s%ld",
	          fg(C_DIM), k, v->total, fg(39), cm, fg(C_DIM), fg(C_ERR), sw, fg(C_DIM), fg(C_WARN), wr);
}

static void note_lines(LINES * out, SB * line, const LANE * L, int maxw) {
	sb_puts(line, "  ");
	SB d = {0};
	describe(&d, L);
	sb_printf(line, "%s%s", fg(C_TEXT), sb_str(&d));
	sb_free(&d);
	add_line(out, line, maxw);
	sb_puts(line, "  ");
	if (L->m.note >= 0) markup(line, L->T->notes[L->m.note], -1);
	add_line(out, line, maxw);
}

static void render_bars(LINES * out, VIEW * v, int k, int cols, int maxrows, bool narrate) {
	LANE * L = &v->L[0];
	int n = L->T->len, mn, mx, maxw = cols - 1;
	samples_minmax(L->T->start, n, &mn, &mx);
	bool values;
	int dg = digits(mn) > digits(mx) ? digits(mn) : digits(mx);
	int cw = bar_width(n, cols - 3, dg, &values);
	int H = maxrows - (values ? 1 : 0) - (narrate ? 5 : 0);
	if (H > 16) H = 16;
	if (H < 3) H = 3;
	HL hl = { -1, -1, -1, -1 };
	if (narrate && L->pos < L->T->n) lane_highlights(L, &hl.c1, &hl.c2, &hl.w1, &hl.w2);
	SB line = {0};
	bar_rows(out, L, H, cw, mn, mx, &hl, &line, maxw);
	if (values) {
		sb_puts(&line, "  ");
		for (int i = 0; i < n; i++) {
			bool hot = i == hl.c1 || i == hl.c2 || i == hl.w1 || i == hl.w2;
			if (hot) sb_printf(&line, BOLD "%s%*d" RST " ", fg(231), cw - 1, L->a[i].v);
			else sb_printf(&line, "%s%*d ", fg(L->dn[i] ? C_FOUND : 246), cw - 1, L->a[i].v);
		}
		add_line(out, &line, maxw);
	}
	if (narrate) {
		marker_row(&line, L, cw, &hl);
		add_line(out, &line, maxw);
		focus_row(&line, L, cw);
		add_line(out, &line, maxw);
		stats_line(&line, v, k);
		add_line(out, &line, maxw);
		note_lines(out, &line, L, maxw);
	}
	sb_free(&line);
}

/*---- RACE --------------------------------*/
static void render_race(LINES * out, VIEW * v, int k, int cols, int maxrows) {
	int m = v->nl, maxw = cols - 1;
	int H = (maxrows - 1) / m - 1;
	if (H > 6) H = 6;
	if (H < 1) H = 1;
	SB line = {0};
	for (int l = 0; l < m; l++) {
		LANE * L = &v->L[l];
		int n = L->T->len, mn, mx;
		samples_minmax(L->T->start, n, &mn, &mx);
		bool values;
		int cw = bar_width(n, cols - 3, 99, &values);	/* never room for values here */
		bool fin = L->pos >= L->T->n;
		sb_printf(&line, "  %s%s%-24s%s cmp %s%-7ld%s swp %s%-7ld%s wr %s%-7ld",
		          fin ? fg(C_OK) : fg(C_TEXT), BOLD, ALGS[v->alg[l]].name, NOBOLD,
		          fg(39), L->m.cmps, fg(C_DIM), fg(C_ERR), L->m.swaps, fg(C_DIM), fg(C_WARN), L->m.writes);
		if (fin) sb_printf(&line, "%s ✓ done in %d steps", fg(C_OK), L->T->n);
		add_line(out, &line, maxw);
		HL hl = { -1, -1, -1, -1 };
		if (!fin) lane_highlights(L, &hl.c1, &hl.c2, &hl.w1, &hl.w2);
		bar_rows(out, L, H, cw, mn, mx, &hl, &line, maxw);
	}
	sb_printf(&line, "  %sstep %d/%d", fg(C_DIM), k, v->total);
	add_line(out, &line, maxw);
	sb_free(&line);
}

/*---- 2D HEATMAP --------------------------*/
static void render_heat(LINES * out, VIEW * v, int k, int cols, int maxrows, bool narrate) {
	static short val[MAX_CELLS];
	static unsigned char hot[MAX_CELLS];		/* 1 compare, 2 swap/write, 3 pivot */
	int w = v->w, h = v->h, maxw = cols - 1, mn = 1 << 30, mx = -(1 << 30);
	(void)maxrows;
	memset(hot, 0, sizeof hot);
	int done_lanes = 0;
	for (int l = 0; l < v->nl; l++) {
		LANE * L = &v->L[l];
		HL hl = { -1, -1, -1, -1 };
		if (narrate && L->pos < L->T->n) lane_highlights(L, &hl.c1, &hl.c2, &hl.w1, &hl.w2);
		if (L->pos >= L->T->n) done_lanes++;
		for (int i = 0; i < L->T->len; i++) {
			int x, y;
			cell_of(v, l, i, &x, &y);
			int c = y * w + x;
			val[c] = (short)L->a[i].v;
			if (i == hl.w1 || i == hl.w2) hot[c] = 2;
			else if (i == hl.c1 || i == hl.c2) hot[c] = 1;
			else if (i == L->m.pivot) hot[c] = 3;
		}
	}
	for (int c = 0; c < w * h; c++) { if (val[c] < mn) mn = val[c]; if (val[c] > mx) mx = val[c]; }
	SB line = {0};
	sb_printf(&line, "  %s╭", fg(C_BORDER));
	sb_rep(&line, "─", w * 2);
	sb_puts(&line, "╮");
	add_line(out, &line, maxw);
	for (int y = 0; y < h; y++) {
		sb_printf(&line, "  %s│", fg(C_BORDER));
		for (int x = 0; x < w; x++) {
			int c = y * w + x;
			if (hot[c] == 1)      sb_printf(&line, "%s██", fg(231));
			else if (hot[c] == 2) sb_printf(&line, "%s▓▓", fg(231));
			else if (hot[c] == 3) sb_printf(&line, "%s██", fg(201));
			else sb_printf(&line, "%s██", fg(shade(val[c] - mn, mx - mn)));
		}
		sb_printf(&line, "%s│", fg(C_BORDER));
		add_line(out, &line, maxw);
	}
	sb_printf(&line, "  %s╰", fg(C_BORDER));
	sb_rep(&line, "─", w * 2);
	sb_puts(&line, "╯");
	if (narrate && (v->map == M_ROWS || v->map == M_COLS))
		sb_printf(&line, "%s  %d of %d %s sorted", fg(C_DIM), done_lanes, v->nl, v->map == M_ROWS ? "rows" : "columns");
	add_line(out, &line, maxw);
	if (narrate) {
		stats_line(&line, v, k);
		add_line(out, &line, maxw);
		note_lines(out, &line, &v->L[0], maxw);
	}
	sb_free(&line);
}

static void view_render(LINES * out, VIEW * v, int k, int cols, int maxrows, bool narrate) {
	view_seek(v, k);
	if (v->kind == V_HEAT) render_heat(out, v, k, cols, maxrows, narrate);
	else if (v->kind == V_RACE) render_race(out, v, k, cols, maxrows);
	else render_bars(out, v, k, cols, maxrows, narrate);
}

/* rest-state pictures of samples, without a trace */
static void show_samples(LINES * out, const SAMPLES * s, int mode, int w, int h, int cols, int maxrows) {
	VIEW * v = xrealloc(NULL, sizeof(VIEW));
	memset(v, 0, sizeof *v);
	v->kind = mode == M_1D ? V_BARS : V_HEAT;
	v->map = mode == M_1D ? M_FLAT : mode;
	v->w = w; v->h = h;
	int lanes = mode == M_ROWS ? h : mode == M_COLS ? w : 1;
	int len = mode == M_ROWS ? w : mode == M_COLS ? h : s->n;
	for (int l = 0; l < lanes; l++) {
		TRACE * T = &v->T[l];
		memset(T, 0, sizeof *T);
		T->len = len;
		T->start = tmp_alloc(len);
		for (int i = 0; i < len; i++) {
			int x = i, y = 0;
			if (v->kind == V_HEAT) cell_of(v, l, i, &x, &y);
			T->start[i] = v->kind == V_HEAT ? s->a[y * w + x] : s->a[i];
		}
		v->nl = l + 1;
		lane_init(&v->L[l], T, lanes);
	}
	if (v->kind == V_HEAT) render_heat(out, v, 0, cols, maxrows, false);
	else render_bars(out, v, 0, cols, maxrows, false);
	view_free(v);
	free(v);
}

#endif
