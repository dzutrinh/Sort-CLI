/*
 *	CMDS.H
 *	------
 *	Slash commands, plain-language requests, autocomplete, the line editor
 *	and the read-eval loop
 *	Part of Sort-CLI, coded by Trinh D.D. Nguyen, MIT licensed
 */
#ifndef SORTCLI_CMDS_H
#define SORTCLI_CMDS_H

#include <ctype.h>

/*---- APP STATE ---------------------------*/
static SAMPLES g_s;
static int g_kind = K_RANDOM;
static int g_custom[MAX_1D], g_ncustom = 0;
static int g_mode = M_1D;
static int g_n1 = 32;					/* 1D size */
static int g_w = 32, g_h = 12;			/* 2D size */
static int g_alg = 0;					/* current algorithm, set to TimSort at start */
static bool g_quit = false;

/* input line, menu, history */
#define INPUT_MAX	400
#define HIST_MAX	100
static char g_in[INPUT_MAX + 1];
static int g_len = 0, g_cur = 0, g_sel = 0;
static bool g_show_help = false;
static long long g_ctrlc_at = 0;
static char * g_hist[HIST_MAX];
static int g_nhist = 0, g_hpos = 0;
static char g_hsave[INPUT_MAX + 1];

/*---- LOOKUPS -----------------------------*/
static bool word_in(const char * list, const char * w) {
	char pat[48];
	if (strlen(w) > 40) return false;
	snprintf(pat, sizeof pat, " %s ", w);
	return strstr(list, pat) != NULL;
}

static bool has_prefix(const char * s, const char * pre) {
	while (*pre) if (tolower((unsigned char)*s++) != tolower((unsigned char)*pre++)) return false;
	return true;
}

static void lower(char * d, const char * s, size_t n) {
	size_t i = 0;
	for (; s[i] && i + 1 < n; i++) d[i] = (char)tolower((unsigned char)s[i]);
	d[i] = 0;
}

/* exact key, alias, then unique prefix of at least 3 letters */
static int find_alg(const char * s) {
	char w[48];
	lower(w, s, sizeof w);
	for (int i = 0; i < NALGS; i++)
		if (!strcmp(w, ALGS[i].key) || word_in(ALGS[i].aliases, w)) return i;
	int hit = -1;
	if (strlen(w) >= 3)
		for (int i = 0; i < NALGS; i++)
			if (has_prefix(ALGS[i].key, w)) { if (hit >= 0) return -1; hit = i; }
	return hit;
}

static int find_name(const char * s, const char * const * names, int n) {
	char w[48];
	lower(w, s, sizeof w);
	for (int i = 0; i < n; i++) if (!strcmp(w, names[i])) return i;
	int hit = -1;
	if (strlen(w) >= 2)
		for (int i = 0; i < n; i++)
			if (has_prefix(names[i], w)) { if (hit >= 0) return -1; hit = i; }
	return hit;
}

static bool parse_int(const char * s, int * v) {
	char * end;
	long x = strtol(s, &end, 10);
	if (end == s || *end || x < -9999 || x > 9999) return false;
	*v = (int)x;
	return true;
}

static bool parse_wxh(const char * s, int * w, int * h) {
	int a, b;
	char x, extra;
	if (sscanf(s, "%d%c%d%c", &a, &x, &b, &extra) == 3 && (x == 'x' || x == 'X')) { *w = a; *h = b; return true; }
	return false;
}

/*---- SIZES AND SAMPLES -------------------*/
static int frame_rows(void) { return g_interactive ? g_rows - 8 : 24; }
static int fit_1d(void) { int n = g_cols - 3; return n > MAX_1D ? MAX_1D : n < 2 ? 2 : n; }
static void fit_2d(int * w, int * h) {
	*w = (g_cols - 6) / 2;
	*h = (g_interactive ? g_rows : 40) - 13 - companion_rows();
	if (*w > 64) *w = 64;
	if (*h > 32) *h = 32;
	if (*w < 2) *w = 2;
	if (*h < 2) *h = 2;
}

static int lane_len(void) { return g_mode == M_1D ? g_n1 : g_mode == M_ROWS ? g_w : g_mode == M_COLS ? g_h : g_w * g_h; }

static void regen(void) {
	if (g_kind == K_CUSTOM && g_mode == M_1D && g_ncustom > 0) {
		g_s.n = g_ncustom;
		g_s.kind = K_CUSTOM;
		for (int i = 0; i < g_ncustom; i++) { g_s.a[i].v = g_custom[i]; g_s.a[i].id = i; }
		g_n1 = g_ncustom;
		return;
	}
	int kind = g_kind == K_CUSTOM ? K_RANDOM : g_kind;
	samples_generate(&g_s, kind, g_mode == M_1D ? g_n1 : g_w * g_h, g_seed);
}

static const char * mode_label(void) {
	static char buf[48];
	if (g_mode == M_1D) snprintf(buf, sizeof buf, "1d");
	else snprintf(buf, sizeof buf, "%d×%d %s", g_w, g_h, MODE_NAMES[g_mode]);
	return buf;
}

static void show_current(void) {
	LINES fr = {0};
	show_samples(&fr, &g_s, g_mode, g_w, g_h, g_cols, frame_rows());
	say_raw(&fr);
	lines_free(&fr);
}

/*---- RESULTS -----------------------------*/
static bool lane_sorted(const TRACE * T) {
	for (int i = 1; i < T->len; i++) if (T->final[i - 1].v > T->final[i].v) return false;
	return true;
}

static bool snake_sorted(const TRACE * T, int w, int h) {
	int prev = -(1 << 30);
	for (int y = 0; y < h; y++)
		for (int i = 0; i < w; i++) {
			int x = (y & 1) ? w - 1 - i : i, v = T->final[y * w + x].v;
			if (v < prev) return false;
			prev = v;
		}
	return true;
}

/* pairs of equal values that changed order (adjacent equal values in the output) */
static int unstable_pairs(const TRACE * T) {
	int bad = 0;
	for (int i = 1; i < T->len; i++)
		if (T->final[i - 1].v == T->final[i].v && T->final[i - 1].id > T->final[i].id) bad++;
	return bad;
}

static bool lane_has_dups(const TRACE * T) { return has_duplicates(T->start, T->len); }

/*---- RUNNING -----------------------------*/
static void frame_view(const PLAYER * p, int k, int cols, int maxrows, LINES * out) {
	view_render(out, p->data, k, cols, maxrows, true);
}

static bool check_alg(int ai, bool race) {
	const ALG * al = &ALGS[ai];
	if (al->only2d && (g_mode == M_1D || race)) {
		say(C_ERR, "%s sorts a matrix: switch to 2D first with `/mode 2d`.", al->name);
		return false;
	}
	int len = race ? g_n1 : lane_len();
	if (al->max_n && len > al->max_n) {
		say(C_ERR, "%s is limited to %d values because it is so slow, and this would sort %d. Try `/size %d`.",
		    al->name, al->max_n, len, al->max_n);
		return false;
	}
	return true;
}

static void too_long(const ALG * al) {
	say(C_ERR, "%s needs more than %d steps here, too many to animate. Try fewer values with `/size`%s.",
	    al->name, EV_CAP, g_mode == M_FLAT || g_mode == M_SNAKE ? " or `/mode rows`" : "");
}

static void run_alg(int ai, bool step) {
	const ALG * al = &ALGS[ai];
	g_alg = ai;
	if (!check_alg(ai, false)) return;
	VIEW * v = xrealloc(NULL, sizeof(VIEW));
	int list[1] = { ai };
	if (!view_run(v, g_mode == M_1D ? V_BARS : V_HEAT, g_mode, g_w, g_h, &g_s, list, 1)) {
		too_long(al);
		view_free(v); free(v);
		return;
	}
	view_lanes(v);
	if (g_mode == M_1D) say(C_TEXT, "**%s** on %d values (%s)", al->name, g_s.n, KIND_NAMES[g_s.kind]);
	else say(C_TEXT, "**%s** on a %d×%d matrix (%s values), %s", al->name, g_w, g_h, KIND_NAMES[g_s.kind],
	         v->map == M_ROWS ? "each row on its own" : v->map == M_COLS ? "each column on its own" :
	         al->only2d ? "as one matrix, ending in snake order" : v->map == M_FLAT ? "in reading order" : "in snake order");
	ui_flush();
	PLAYER p = { v->total + 1, 0, auto_stride(v->total + 1), false, al->verb, frame_view, v };
	bool fin = ui_play(&p, step);
	LINES fr = {0};
	view_render(&fr, v, p.k, g_cols, frame_rows(), true);
	say_raw(&fr);
	lines_free(&fr);
	if (!fin) {
		say(C_WARN, "Stopped at step %d of %d.", p.k, v->total);
	} else {
		long cm = 0, sw = 0, wr = 0, rd = 0;
		int bad = 0;
		bool sorted = true, dups = false;
		for (int l = 0; l < v->nl; l++) {
			TRACE * T = &v->T[l];
			cm += T->cmps; sw += T->swaps; wr += T->writes; rd += T->reads;
			sorted = sorted && (al->only2d ? snake_sorted(T, g_w, g_h) : lane_sorted(T));
			bad += unstable_pairs(T);
			dups = dups || lane_has_dups(T);
		}
		say(sorted ? C_OK : C_ERR, "%s %d values: %ld comparisons, %ld swaps, %ld writes, %ld array accesses.",
		    sorted ? "Sorted" : "Did NOT sort", g_s.n, cm, sw, wr, rd + wr + 2 * sw);
		if (!dups) say_more("All values differ, so stability cannot show here. `/data dups` has equal values.");
		else if (!bad) say_more("Stable here: equal values kept their original order%s.", al->stable ? "" : " (by luck, it is not stable in general)");
		else say_more("Not stable: %d neighbouring pairs of equal values swapped their original order.", bad);
		say_more("%s", al->hint);
	}
	view_free(v);
	free(v);
}

static const char * DEFAULT_RACE[] = { "insertion", "shell", "merge", "quick", "heap", "timsort" };

static void cmd_race(int argc, char ** argv) {
	int list[MAX_RACE], n = 0;
	if (g_mode != M_1D) { say(C_ERR, "Races run on 1D data: switch with `/mode 1d`."); return; }
	for (int i = 0; i < argc; i++) {
		int a = find_alg(argv[i]);
		if (a < 0) { say(C_ERR, "I don't know `%s`. `/algos` lists them.", argv[i]); return; }
		if (n < MAX_RACE) list[n++] = a;
	}
	if (n == 0) for (int i = 0; i < 6; i++) list[n++] = find_alg(DEFAULT_RACE[i]);
	if (n == 1) { run_alg(list[0], false); return; }
	for (int i = 0; i < n; i++) if (!check_alg(list[i], true)) return;
	VIEW * v = xrealloc(NULL, sizeof(VIEW));
	if (!view_run(v, V_RACE, M_1D, g_w, g_h, &g_s, list, n)) {
		say(C_ERR, "One of these needs more than %d steps here. Try fewer values with `/size`.", EV_CAP);
		view_free(v); free(v);
		return;
	}
	view_lanes(v);
	say(C_TEXT, "Race of %d algorithms on the same %d values (%s)", n, g_s.n, KIND_NAMES[g_s.kind]);
	ui_flush();
	PLAYER p = { v->total + 1, 0, auto_stride(v->total + 1), false, "Racing", frame_view, v };
	bool fin = ui_play(&p, false);
	LINES fr = {0};
	view_render(&fr, v, p.k, g_cols, frame_rows(), true);
	say_raw(&fr);
	lines_free(&fr);
	if (!fin) {
		say(C_WARN, "Stopped at step %d of %d.", p.k, v->total);
	} else {
		int order[MAX_RACE];
		for (int i = 0; i < n; i++) order[i] = i;
		for (int i = 1; i < n; i++)
			for (int j = i; j > 0 && v->T[order[j]].n < v->T[order[j - 1]].n; j--) { int t = order[j]; order[j] = order[j - 1]; order[j - 1] = t; }
		SB b = {0};
		for (int i = 0; i < n; i++)
			sb_printf(&b, "%s%d. **%s** (%d steps)", i ? " · " : "", i + 1, ALGS[v->alg[order[i]]].name, v->T[order[i]].n);
		say(C_OK, "Finish order: %s", sb_str(&b));
		say_more("A step is one comparison, swap, write or annotation, so fewer steps means less work.");
		sb_free(&b);
	}
	view_free(v);
	free(v);
}

/*---- COMMANDS ----------------------------*/
typedef void (*CMDFN)(int argc, char ** argv);
enum { COMP_NONE, COMP_ALGS, COMP_DATA, COMP_MODE, COMP_GRAD, COMP_COLOR, COMP_CAT, COMP_SEED, COMP_ONOFF };
typedef struct { const char * name, * args, * desc; CMDFN fn; int comp; } CMD;

static void cmd_help(int argc, char ** argv);
static void cmd_algos(int argc, char ** argv);
static void cmd_run(int argc, char ** argv);
static void cmd_step(int argc, char ** argv);
static void cmd_compare(int argc, char ** argv);
static void cmd_explain(int argc, char ** argv);
static void cmd_use(int argc, char ** argv);
static void cmd_data(int argc, char ** argv);
static void cmd_size(int argc, char ** argv);
static void cmd_mode(int argc, char ** argv);
static void cmd_show(int argc, char ** argv);
static void cmd_gradient(int argc, char ** argv);
static void cmd_color(int argc, char ** argv);
static void cmd_seed(int argc, char ** argv);
static void cmd_speed(int argc, char ** argv);
static void cmd_duration(int argc, char ** argv);
static void cmd_pet(int argc, char ** argv);
static void cmd_clear(int argc, char ** argv);
static void cmd_exit(int argc, char ** argv);

static const CMD CMDS[] = {
	{ "/run",      "[algo]",              "Run a sort and watch it",                   cmd_run,      COMP_ALGS },
	{ "/step",     "[algo]",              "Walk through a sort one step at a time",    cmd_step,     COMP_ALGS },
	{ "/race",     "[algo algo …]",       "Several sorts side by side on the same data", cmd_race,   COMP_ALGS },
	{ "/compare",  "[algo …]",            "Count the work every sort does, no animation", cmd_compare, COMP_ALGS },
	{ "/explain",  "[algo]",              "How a sort works, its cost, pseudocode",    cmd_explain,  COMP_ALGS },
	{ "/algos",    "[category]",          "List the algorithms",                       cmd_algos,    COMP_CAT },
	{ "/use",      "<algo>",              "Pick the current algorithm",                cmd_use,      COMP_ALGS },
	{ "/data",     "[kind] [n|WxH] | v1 v2 …", "Show or make new samples",             cmd_data,     COMP_DATA },
	{ "/size",     "<n> | <WxH>",         "Number of values (1D) or matrix size (2D)", cmd_size,     COMP_NONE },
	{ "/mode",     "1d | 2d [layout]",    "Bars, or a matrix: flat, snake, rows, cols", cmd_mode,    COMP_MODE },
	{ "/show",     "",                    "Show the current samples",                  cmd_show,     COMP_NONE },
	{ "/gradient", "[name|reverse|colors…]", "Color gradient, or your own stops",     cmd_gradient, COMP_GRAD },
	{ "/color",    "[256|truecolor]",     "Color output mode",                         cmd_color,    COMP_COLOR },
	{ "/seed",     "[n|random]",          "Random seed behind the samples",            cmd_seed,     COMP_SEED },
	{ "/speed",    "[ms]",                "Milliseconds per animation frame",          cmd_speed,    COMP_NONE },
	{ "/duration", "[s]",                 "Longest a run takes before frames skip steps", cmd_duration, COMP_NONE },
	{ "/pet",      "[on|off]",            "Show or hide Bitsy, the bar critter above the prompt", cmd_pet, COMP_ONOFF },
	{ "/clear",    "",                    "Clear the conversation",                    cmd_clear,    COMP_NONE },
	{ "/help",     "",                    "Show help",                                 cmd_help,     COMP_NONE },
	{ "/exit",     "",                    "Quit Sort-CLI",                              cmd_exit,     COMP_NONE },
};
#define NCMDS ((int)(sizeof CMDS / sizeof CMDS[0]))

static void cmd_help(int argc, char ** argv) {
	(void)argc; (void)argv;
	say(C_TEXT, "Sort-CLI animates %d sorting algorithms on 1D bars or 2D matrices. Type a slash command or just ask, e.g. `timsort`, `race quick merge heap`, `explain smooth`, `reversed 40`, `2d rows`.", NALGS);
	LINES L = {0};
	SB b = {0}, c = {0};
	for (int i = 0; i < NCMDS; i++) {
		sb_printf(&c, "%s %s", CMDS[i].name, CMDS[i].args);
		sb_printf(&b, "    %s", fg(C_CODE));
		sb_cell(&b, sb_str(&c), 34);
		sb_printf(&b, "%s%s", fg(C_MUTED), CMDS[i].desc);
		lines_take(&L, &b);
		sb_clear(&c);
	}
	lines_add(&L, "");
	sb_printf(&b, "    %sDuring playback: space pause · ←/→ step · pgup/pgdn jump · +/- speed · enter finish · esc stop", fg(C_DIM));
	lines_take(&L, &b);
	say_raw(&L);
	lines_free(&L);
	sb_free(&b);
	sb_free(&c);
}

static void cmd_algos(int argc, char ** argv) {
	int only = -1;
	if (argc && (only = find_name(argv[0], CATS, NCATS)) < 0) {
		say(C_ERR, "Unknown category `%s`. Try exchange, selection, insertion, merge, hybrid, distribution, network, slow or 2d.", argv[0]);
		return;
	}
	say(C_TEXT, "%d sorting algorithms%s. Current: **%s**. Costs are big-O in n values (k: value range, d: digits).",
	    NALGS, only >= 0 ? " (one category)" : "", ALGS[g_alg].name);
	/* the cells of every row, then each column as wide as its widest cell */
	enum { NCOL = 7 };
	static const char * HEAD[NCOL] = { "key", "name", "best", "average", "worst", "space", "stable" };
	static char cell[NALGS][NCOL][40];
	int width[NCOL];
	for (int c = 0; c < NCOL; c++) width[c] = vis_width(HEAD[c]);
	for (int i = 0; i < NALGS; i++) {
		const ALG * a = &ALGS[i];
		snprintf(cell[i][0], 40, "%s", a->key);
		snprintf(cell[i][1], 40, "%s", a->name);
		snprintf(cell[i][2], 40, "%s", a->best);
		snprintf(cell[i][3], 40, "%s", a->avg);
		snprintf(cell[i][4], 40, "%s", a->worst);
		snprintf(cell[i][5], 40, "%s", a->space);
		snprintf(cell[i][6], 40, "%s", a->stable ? "yes" : "no");
		if (only >= 0 && strcmp(a->cat, CATS[only])) continue;
		for (int c = 0; c < NCOL; c++) if (vis_width(cell[i][c]) > width[c]) width[c] = vis_width(cell[i][c]);
	}
	static const int COLOR[NCOL] = { C_CODE, C_TEXT, C_MUTED, C_MUTED, C_MUTED, C_MUTED, 0 };
	bool show[NCOL] = { true, true, true, true, true, true, true };
	static const int DROP[3] = { 5, 2, 6 };				/* in narrow windows: space, best, stable */
	for (int d = 0; d < 3; d++) {
		int total = 4;
		for (int c = 0; c < NCOL; c++) if (show[c]) total += width[c] + 2;
		if (total - 2 <= g_cols - 1) break;
		show[DROP[d]] = false;
	}
	LINES L = {0};
	SB b = {0};
	sb_printf(&b, "    %s", fg(C_DIM));
	for (int c = 0; c < NCOL; c++) if (show[c]) sb_cell(&b, HEAD[c], c < NCOL - 1 ? width[c] + 2 : 0);
	lines_take(&L, &b);
	for (int k = 0; k < NCATS; k++) {
		if (only >= 0 && k != only) continue;
		sb_printf(&b, "    %s%s", fg(C_ACCENT), CATS[k]);
		lines_take(&L, &b);
		for (int i = 0; i < NALGS; i++) {
			if (strcmp(ALGS[i].cat, CATS[k])) continue;
			sb_puts(&b, "    ");
			for (int c = 0; c < NCOL; c++) {
				if (!show[c]) continue;
				sb_puts(&b, fg(c == NCOL - 1 ? (ALGS[i].stable ? C_OK : C_DIM) : COLOR[c]));
				sb_cell(&b, cell[i][c], c < NCOL - 1 ? width[c] + 2 : 0);
			}
			lines_take(&L, &b);
		}
	}
	say_raw(&L);
	lines_free(&L);
	sb_free(&b);
}

static void cmd_run(int argc, char ** argv)  {
	int a = g_alg;
	if (argc > 1) { cmd_race(argc, argv); return; }
	if (argc && (a = find_alg(argv[0])) < 0) { say(C_ERR, "I don't know `%s`. `/algos` lists them.", argv[0]); return; }
	run_alg(a, false);
}

static void cmd_step(int argc, char ** argv) {
	int a = g_alg;
	if (argc && (a = find_alg(argv[0])) < 0) { say(C_ERR, "I don't know `%s`. `/algos` lists them.", argv[0]); return; }
	run_alg(a, true);
}

static void cmd_use(int argc, char ** argv) {
	int a;
	if (!argc) { say(C_TEXT, "The current algorithm is **%s**.", ALGS[g_alg].name); return; }
	if ((a = find_alg(argv[0])) < 0) { say(C_ERR, "I don't know `%s`. `/algos` lists them.", argv[0]); return; }
	g_alg = a;
	say(C_OK, "`/run` now uses **%s**.", ALGS[a].name);
}

typedef struct { int alg; long cm, sw, wr, acc; int steps; int bad; bool ok, skipped; char why[48]; } CMPROW;

static void cmd_compare(int argc, char ** argv) {
	static CMPROW rows[64];
	int list[64], n = 0;
	for (int i = 0; i < argc; i++) {
		int a = find_alg(argv[i]);
		if (!strcmp(argv[i], "all")) continue;
		if (a < 0) { say(C_ERR, "I don't know `%s`. `/algos` lists them.", argv[i]); return; }
		if (n < 64) list[n++] = a;
	}
	if (!n) for (int i = 0; i < NALGS; i++) list[n++] = i;
	int len = lane_len();
	bool dups = false;
	long top = 1;
	VIEW * v = xrealloc(NULL, sizeof(VIEW));
	for (int r = 0; r < n; r++) {
		const ALG * al = &ALGS[list[r]];
		CMPROW * row = &rows[r];
		memset(row, 0, sizeof *row);
		row->alg = list[r];
		if (al->only2d && g_mode == M_1D) { row->skipped = true; snprintf(row->why, sizeof row->why, "2D only"); continue; }
		if (al->max_n && len > al->max_n) { row->skipped = true; snprintf(row->why, sizeof row->why, "limited to %d values", al->max_n); continue; }
		int one[1] = { list[r] };
		bool fits = view_run(v, g_mode == M_1D ? V_BARS : V_HEAT, g_mode, g_w, g_h, &g_s, one, 1);
		if (!fits) { row->skipped = true; snprintf(row->why, sizeof row->why, "over %d steps", EV_CAP); view_free(v); continue; }
		row->ok = true;
		for (int l = 0; l < v->nl; l++) {
			TRACE * T = &v->T[l];
			row->cm += T->cmps; row->sw += T->swaps; row->wr += T->writes; row->acc += T->reads + T->writes + 2 * T->swaps;
			row->steps += T->n;
			row->bad += unstable_pairs(T);
			dups = dups || lane_has_dups(T);
		}
		if (row->acc > top) top = row->acc;
		view_free(v);
	}
	free(v);
	for (int i = 1; i < n; i++)							/* least work first */
		for (int j = i; j > 0; j--) {
			CMPROW * a = &rows[j - 1], * b = &rows[j];
			long ka = a->ok ? a->acc : (1L << 60), kb = b->ok ? b->acc : (1L << 60);
			if (kb >= ka) break;
			CMPROW t = *a; *a = *b; *b = t;
		}
	say(C_TEXT, "Work done by %d algorithms on %d values (%s, %s)", n, g_s.n, KIND_NAMES[g_s.kind], mode_label());
	LINES L = {0};
	SB b = {0};
	sb_printf(&b, "    %s", fg(C_DIM));
	sb_cell(&b, "algorithm", 28);
	sb_printf(&b, "%9s %9s %9s %10s  %-7s accesses", "compares", "swaps", "writes", "accesses", "stable");
	lines_take(&L, &b);
	for (int r = 0; r < n; r++) {
		CMPROW * row = &rows[r];
		sb_printf(&b, "    %s", fg(r == 0 && row->ok ? C_OK : C_TEXT));
		sb_cell(&b, ALGS[row->alg].name, 28);
		if (!row->ok) {
			sb_printf(&b, "%s(skipped: %s)", fg(C_DIM), row->why);
		} else {
			const char * st = !dups ? "—" : row->bad ? "no" : "yes";
			sb_printf(&b, "%s%9ld %9ld %9ld %10ld  %s", fg(C_MUTED), row->cm, row->sw, row->wr, row->acc,
			          !dups ? fg(C_DIM) : row->bad ? fg(C_ERR) : fg(C_OK));
			sb_cell(&b, st, 8);
			int len_bar = (int)((row->acc * 20 + top - 1) / top);
			sb_puts(&b, fg(r == 0 ? C_OK : C_ACCENT));
			sb_rep(&b, "█", len_bar < 1 ? 1 : len_bar);
		}
		lines_take(&L, &b);
	}
	say_raw(&L);
	lines_free(&L);
	sb_free(&b);
	say_more("Accesses count every read and write of the array (a swap is two of each). %s",
	         dups ? "Stable means equal values kept their original order on this input." : "All values differ, so stability shows as —; try `/data dups`.");
}

static void cmd_explain(int argc, char ** argv) {
	int a = g_alg;
	if (argc && (a = find_alg(argv[0])) < 0) { say(C_ERR, "I don't know `%s`. `/algos` lists them.", argv[0]); return; }
	const ALG * s = &ALGS[a];
	say(C_TEXT, "**%s** (%s). %s", s->name, s->cat, s->summary);
	say_more("Best O(%s) · average O(%s) · worst O(%s) · extra space O(%s) · %s · %s", s->best, s->avg, s->worst, s->space,
	         s->stable ? "stable" : "not stable", s->inplace ? "in place" : "uses a buffer");
	LINES L = {0};
	SB b = {0};
	for (int i = 0; i < 8 && s->pseudo[i]; i++) {
		sb_printf(&b, "       %s%s", fg(C_CODE), s->pseudo[i]);
		lines_take(&L, &b);
	}
	say_raw(&L);
	lines_free(&L);
	sb_free(&b);
	say_more("%s Watch it with `/step %s`.", s->hint, s->key);
}

static void announce_samples(const char * what) {
	if (g_mode == M_1D) say(C_OK, "%s %d values, **%s**: %s. Seed %lu.", what, g_s.n, KIND_NAMES[g_s.kind], KIND_DESC[g_s.kind], g_seed);
	else say(C_OK, "%s a %d×%d matrix (%s), **%s** values: %s. Seed %lu.", what, g_w, g_h, MODE_NAMES[g_mode],
	         KIND_NAMES[g_s.kind], KIND_DESC[g_s.kind], g_seed);
	show_current();
}

static bool set_size_1d(int n) {
	int fit = fit_1d();
	if (n < 2) { say(C_ERR, "Use at least 2 values."); return false; }
	if (n > fit) { say(C_WARN, "%d values do not fit the window: using %d.", n, fit); n = fit; }
	g_n1 = n;
	return true;
}

static bool set_size_2d(int w, int h) {
	int fw, fh;
	fit_2d(&fw, &fh);
	if (w < 2 || h < 2) { say(C_ERR, "A matrix needs at least 2×2 cells."); return false; }
	if (w > fw || h > fh) {
		say(C_WARN, "%d×%d does not fit the window: using %d×%d.", w, h, w < fw ? w : fw, h < fh ? h : fh);
		if (w > fw) w = fw;
		if (h > fh) h = fh;
	}
	g_w = w; g_h = h;
	return true;
}

static void cmd_data(int argc, char ** argv) {
	int v, w, h;
	if (!argc) {
		say(C_TEXT, "Current samples: %d %s values (%s), %s, seed %lu.", g_s.n, KIND_NAMES[g_s.kind], KIND_DESC[g_s.kind], mode_label(), g_seed);
		show_current();
		return;
	}
	bool all_ints = true;
	for (int i = 0; i < argc; i++) if (!parse_int(argv[i], &v)) all_ints = false;
	if (all_ints && argc >= 2) {						/* your own values */
		g_ncustom = 0;
		for (int i = 0; i < argc && g_ncustom < fit_1d(); i++) {
			parse_int(argv[i], &v);
			g_custom[g_ncustom++] = v < -999 ? -999 : v > 9999 ? 9999 : v;
		}
		g_kind = K_CUSTOM;
		g_mode = M_1D;
		regen();
		announce_samples("Loaded");
		return;
	}
	int kind = g_kind == K_CUSTOM ? K_RANDOM : g_kind;
	for (int i = 0; i < argc; i++) {
		int k;
		if (parse_int(argv[i], &v)) { if (!set_size_1d(v)) return; g_mode = M_1D; }
		else if (parse_wxh(argv[i], &w, &h)) { if (!set_size_2d(w, h)) return; if (g_mode == M_1D) g_mode = M_FLAT; }
		else if ((k = find_name(argv[i], KIND_NAMES, K_KINDS - 1)) >= 0) kind = k;
		else { say(C_ERR, "Unknown kind `%s`. Try random, sorted, reversed, nearly, few, dups, runs, sawtooth or organ.", argv[i]); return; }
	}
	g_kind = kind;
	g_seed = new_seed();
	regen();
	announce_samples("New samples:");
}

static void cmd_size(int argc, char ** argv) {
	int v, w, h;
	if (!argc) {
		if (g_mode == M_1D) say(C_TEXT, "Size: **%d** values. `/size 48` changes it, `/size 30x12` makes a matrix.", g_n1);
		else say(C_TEXT, "Size: **%d×%d** matrix. `/size 40x16` changes it, `/size 48` goes back to bars.", g_w, g_h);
		return;
	}
	if (parse_int(argv[0], &v)) { if (!set_size_1d(v)) return; g_mode = M_1D; }
	else if (parse_wxh(argv[0], &w, &h)) { if (!set_size_2d(w, h)) return; if (g_mode == M_1D) g_mode = M_FLAT; }
	else { say(C_ERR, "Usage: `/size 40` for bars or `/size 30x12` for a matrix."); return; }
	if (g_kind == K_CUSTOM) g_kind = K_RANDOM;
	regen();
	announce_samples("Resized:");
}

static void cmd_mode(int argc, char ** argv) {
	if (!argc) {
		say(C_TEXT, "Mode: **%s**, %s.", mode_label(), MODE_DESC[g_mode]);
		LINES L = {0};
		SB b = {0};
		for (int i = 0; i < M_MODES; i++) {
			sb_printf(&b, "    %s%-7s%s%s", fg(C_CODE), MODE_NAMES[i], fg(C_MUTED), MODE_DESC[i]);
			lines_take(&L, &b);
		}
		say_raw(&L);
		lines_free(&L);
		sb_free(&b);
		return;
	}
	int mode = -1, w = 0, h = 0;
	for (int i = 0; i < argc; i++) {
		int m;
		if (!strcmp(argv[i], "2d") || !strcmp(argv[i], "matrix")) { if (mode < 0 || mode == M_1D) mode = g_mode == M_1D ? M_FLAT : g_mode; }
		else if (!strcmp(argv[i], "columns")) mode = M_COLS;
		else if (parse_wxh(argv[i], &w, &h)) { if (mode < 0 || mode == M_1D) mode = g_mode == M_1D ? M_FLAT : g_mode; }
		else if ((m = find_name(argv[i], MODE_NAMES, M_MODES)) >= 0) mode = m;
		else { say(C_ERR, "Unknown mode `%s`. Try 1d, 2d, flat, snake, rows or cols.", argv[i]); return; }
	}
	if (w && !set_size_2d(w, h)) return;
	if (mode != M_1D) { int fw, fh; fit_2d(&fw, &fh); if (g_w > fw) g_w = fw; if (g_h > fh) g_h = fh; }
	else if (g_n1 > fit_1d()) g_n1 = fit_1d();
	g_mode = mode;
	if (g_kind == K_CUSTOM && g_mode != M_1D) g_kind = K_RANDOM;
	regen();
	announce_samples(g_mode == M_1D ? "Bars mode:" : "Matrix mode:");
}

static void cmd_show(int argc, char ** argv) { cmd_data(0, NULL); (void)argc; (void)argv; }

static void swatch(SB * b, int width) {
	for (int i = 0; i < width; i++) sb_printf(b, "%s█", fg(shade(i, width - 1)));
	sb_puts(b, RST);
}

static void grad_announce(void) {
	SB b = {0};
	swatch(&b, 24);
	say(C_OK, "Gradient is now **%s**%s %s", g_grad_name, g_grad_rev ? " (reversed)" : "", sb_str(&b));
	sb_free(&b);
	show_current();
}

/* /gradient: list, preset [reverse], reverse, or custom color stops */
static void cmd_gradient(int argc, char ** argv) {
	if (argc == 0) {
		say(C_TEXT, "Gradients color the bars and matrix cells. Current: **%s**%s, %s color.",
		    g_grad_name, g_grad_rev ? " (reversed)" : "", g_truecolor ? "24-bit" : "256");
		char keep_name[32];
		unsigned keep_rgb[GRAD_STOPS];
		int keep_n = g_grad_n;
		bool keep_rev = g_grad_rev;
		memcpy(keep_name, g_grad_name, sizeof keep_name);
		memcpy(keep_rgb, g_grad_rgb, sizeof keep_rgb);
		LINES L = {0};
		SB b = {0};
		for (int i = 0; i < NGPRESETS; i++) {
			grad_use_preset(i);
			bool cur = !strcmp(keep_name, GPRESETS[i].name);
			sb_printf(&b, "    %s%-10s ", fg(cur ? C_OK : C_CODE), GPRESETS[i].name);
			swatch(&b, 24);
			sb_printf(&b, "  %s%s", fg(C_DIM), GPRESETS[i].desc);
			lines_take(&L, &b);
		}
		memcpy(g_grad_name, keep_name, sizeof keep_name);
		memcpy(g_grad_rgb, keep_rgb, sizeof keep_rgb);
		g_grad_n = keep_n;
		g_grad_rev = keep_rev;
		grad_build();
		say_raw(&L);
		lines_free(&L);
		sb_free(&b);
		say_more("`/gradient magma`, `/gradient viridis reverse`, `/gradient reverse`, or your own stops: `/gradient #1e3a8a #f59e0b #fef3c7` (hex or xterm 0-255).");
		return;
	}
	if (argc == 1 && !strcmp(argv[0], "reverse")) {
		g_grad_rev = !g_grad_rev;
		grad_build();
		grad_announce();
		return;
	}
	int p = grad_find(argv[0]);
	if (p >= 0) {
		if (argc > 2 || (argc == 2 && strcmp(argv[1], "reverse"))) {
			say(C_ERR, "Usage: `/gradient %s` or `/gradient %s reverse`.", GPRESETS[p].name, GPRESETS[p].name);
			return;
		}
		grad_use_preset(p);
		if (argc == 2) { g_grad_rev = true; grad_build(); }
		grad_announce();
		return;
	}
	unsigned rgb[GRAD_STOPS];
	int n = 0;
	for (int i = 0; i < argc; i++) {
		if (n == GRAD_STOPS) { say(C_ERR, "A gradient takes at most %d colors.", GRAD_STOPS); return; }
		if (!grad_parse_color(argv[i], &rgb[n++])) {
			say(C_ERR, "`%s` is not a preset or a color. Use a preset name (`/gradient` lists them), hex like `#ff8800`, or an xterm color 0-255.", argv[i]);
			return;
		}
	}
	if (n < 2) {
		say(C_ERR, "A custom gradient needs at least two colors, e.g. `/gradient #000080 #ffd700`.");
		return;
	}
	memcpy(g_grad_rgb, rgb, sizeof rgb);
	g_grad_n = n;
	g_grad_rev = false;
	snprintf(g_grad_name, sizeof g_grad_name, "custom");
	grad_build();
	grad_announce();
}

/* /color: 24-bit or 256 colors */
static void cmd_color(int argc, char ** argv) {
	if (argc && (!strcmp(argv[0], "truecolor") || !strcmp(argv[0], "24bit") || !strcmp(argv[0], "256"))) {
		g_truecolor = strcmp(argv[0], "256") != 0;
		grad_build();
		SB b = {0};
		swatch(&b, 24);
		say(C_OK, "Using %s color %s", g_truecolor ? "24-bit" : "256", sb_str(&b));
		sb_free(&b);
		if (g_truecolor) say_more("If the swatch looks wrong, your terminal lacks 24-bit color: `/color 256` switches back.");
	} else if (argc) {
		say(C_ERR, "Usage: `/color truecolor` or `/color 256`.");
	} else {
		say(C_TEXT, "Gradients use **%s** color. `/color truecolor` gives smooth 24-bit gradients on terminals that support it; `/color 256` works everywhere.",
		    g_truecolor ? "24-bit" : "256");
	}
}

static void cmd_seed(int argc, char ** argv) {
	int v;
	if (!argc) { say(C_TEXT, "Seed **%lu**. `/seed 42` makes the samples reproducible, `/seed random` picks a new one.", g_seed); return; }
	if (!strcmp(argv[0], "random")) g_seed = new_seed() ^ (unsigned long)now_ms() % 99991;
	else if (parse_int(argv[0], &v) && v >= 0) g_seed = (unsigned long)v;
	else { say(C_ERR, "Usage: `/seed 42` or `/seed random`."); return; }
	if (g_kind == K_CUSTOM) g_kind = K_RANDOM;
	regen();
	announce_samples("Reseeded:");
}

static void cmd_speed(int argc, char ** argv) {
	int v;
	if (!argc) { say(C_TEXT, "Each frame shows for **%d ms**; `+`/`-` change it while playing.", g_speed); return; }
	if (!parse_int(argv[0], &v) || v < 5 || v > 2000) { say(C_ERR, "Speed is milliseconds per frame, 5 to 2000."); return; }
	g_speed = v;
	say(C_OK, "Each frame now shows for **%d ms**.", v);
}

static void cmd_duration(int argc, char ** argv) {
	int v;
	if (!argc) { say(C_TEXT, "Runs last at most **%d s**; longer ones show several steps per frame.", g_duration); return; }
	if (!parse_int(argv[0], &v) || v < 1 || v > 600) { say(C_ERR, "Duration is in seconds, 1 to 600."); return; }
	g_duration = v;
	say(C_OK, "Runs now last at most **%d s**.", v);
}

static void cmd_pet(int argc, char ** argv) {
	if (argc && (!strcmp(argv[0], "on") || !strcmp(argv[0], "off"))) {
		g_pet_on = !strcmp(argv[0], "on");
		say(C_OK, g_pet_on ? "Bitsy is back above the prompt." : "Bitsy is hidden. `/pet on` brings it back.");
		if (g_pet_on) pet_say("hi!", 3000);
	} else if (argc) {
		say(C_ERR, "Usage: `/pet on` or `/pet off`.");
	} else {
		say(C_TEXT, "Bitsy the bar critter is **%s**%s. It sorts its bars while you sort yours, cheers when a run finishes and dozes off after a minute without keys.",
		    g_pet_on ? "on" : "off",
		    g_pet_on && !pet_fits() ? ", but hidden because the window is smaller than 60×22" : "");
	}
}

static void cmd_clear(int argc, char ** argv) {
	(void)argc; (void)argv;
	ui_clear();
	block_add(B_BANNER, 0, NULL, NULL);
}

static void cmd_exit(int argc, char ** argv) {
	(void)argc; (void)argv;
	g_quit = true;
}

/*---- PLAIN-LANGUAGE REQUESTS -------------*/
static const char * FILLER = " sort sorting sorts sorted? with on the a an of values value numbers run show me please using in and to ";

static void natural(int argc, char ** argv) {
	int algs[MAX_RACE], na = 0, kind = -1, num = -1, mode = -1, grad = -1, w = 0, h = 0;
	bool race = false, compare = false, explain = false, step = false, help = false, quit = false, unknown = false;
	char unk[48] = "";
	for (int i = 0; i < argc; i++) {
		char t[48];
		int v, k;
		lower(t, argv[i], sizeof t);
		size_t n = strlen(t);
		while (n && strchr(",.?!:;", t[n - 1])) t[--n] = 0;
		if (!n) continue;
		if (parse_int(t, &v)) num = v;
		else if (parse_wxh(t, &w, &h)) { if (mode < 0) mode = M_FLAT; }
		else if (!strcmp(t, "race") || !strcmp(t, "vs") || !strcmp(t, "versus")) race = true;
		else if (!strcmp(t, "compare")) compare = true;
		else if (!strcmp(t, "explain") || !strcmp(t, "how") || !strcmp(t, "what") || !strcmp(t, "why") || !strcmp(t, "describe")) explain = true;
		else if (!strcmp(t, "step") || !strcmp(t, "slowly")) step = true;
		else if (!strcmp(t, "help") || !strcmp(t, "commands")) help = true;
		else if (!strcmp(t, "exit") || !strcmp(t, "quit") || !strcmp(t, "bye")) quit = true;
		else if (!strcmp(t, "2d") || !strcmp(t, "matrix") || !strcmp(t, "grid")) { if (mode < 0) mode = M_FLAT; }
		else if (!strcmp(t, "1d") || !strcmp(t, "bars")) mode = M_1D;
		else if (!strcmp(t, "rows") || !strcmp(t, "row")) mode = M_ROWS;
		else if (!strcmp(t, "cols") || !strcmp(t, "columns") || !strcmp(t, "column")) mode = M_COLS;
		else if (!strcmp(t, "snake")) mode = M_SNAKE;
		else if (!strcmp(t, "flat")) mode = M_FLAT;
		else if ((k = find_alg(t)) >= 0) { if (na < MAX_RACE) algs[na++] = k; }
		else if ((k = find_name(t, KIND_NAMES, K_KINDS - 1)) >= 0 && n >= 3) kind = k;
		else if ((k = grad_find(t)) >= 0) grad = k;
		else if (!word_in(FILLER, t)) { unknown = true; snprintf(unk, sizeof unk, "%s", t); }
	}
	if (quit) { g_quit = true; return; }
	if (help) { cmd_help(0, NULL); return; }
	char * av[MAX_RACE];
	for (int i = 0; i < na; i++) av[i] = (char *)ALGS[algs[i]].key;
	if (race || (na >= 2 && !compare)) { cmd_race(na, av); return; }
	if (compare) { cmd_compare(na, av); return; }
	if (explain && na) { cmd_explain(1, av); return; }
	bool changed = false;
	if (mode >= 0 && mode != g_mode) {
		char wh[24], * mv[2] = { (char *)MODE_NAMES[mode], wh };
		snprintf(wh, sizeof wh, "%dx%d", w ? w : g_w, h ? h : g_h);
		cmd_mode(w ? 2 : 1, mv);
		changed = true;
	}
	if (kind >= 0 || (num > 0 && !changed)) {
		char nb[16], * dv[2];
		int dn = 0;
		if (kind >= 0) dv[dn++] = (char *)KIND_NAMES[kind];
		if (num > 0 && g_mode == M_1D) { snprintf(nb, sizeof nb, "%d", num); dv[dn++] = nb; }
		cmd_data(dn, dv);
		changed = true;
	}
	if (grad >= 0) { char * gv[1] = { (char *)GPRESETS[grad].name }; cmd_gradient(1, gv); changed = true; }
	if (na == 1) { run_alg(algs[0], step); return; }
	if (step && !na) { run_alg(g_alg, true); return; }
	if (changed) return;
	if (unknown) say(C_TEXT, "I don't know `%s`. Try `timsort`, `race quick merge heap`, `explain smooth`, `reversed 40`, `2d rows` or `/help`.", unk);
	else say(C_TEXT, "I only speak sorting. Try `timsort`, `race quick merge heap`, `explain smooth`, `reversed 40`, `2d rows` or `/help`.");
}

/*---- DISPATCH ----------------------------*/
#define MAX_ARGS	80

static void execute(const char * line) {
	char buf[INPUT_MAX + 1], * argv[MAX_ARGS];
	int argc = 0;
	snprintf(buf, sizeof buf, "%s", line);
	for (char * tok = strtok(buf, " \t"); tok && argc < MAX_ARGS; tok = strtok(NULL, " \t")) argv[argc++] = tok;
	if (!argc) return;
	if (argv[0][0] != '/') { natural(argc, argv); return; }
	char name[48];
	lower(name, argv[0], sizeof name);
	if (!strcmp(name, "/quit") || !strcmp(name, "/q")) { g_quit = true; return; }
	if (!strcmp(name, "/?")) { cmd_help(0, NULL); return; }
	if (!strcmp(name, "/dim")) { cmd_mode(argc - 1, argv + 1); return; }
	if (!strcmp(name, "/theme")) { cmd_gradient(argc - 1, argv + 1); return; }
	for (int i = 0; i < NCMDS; i++)
		if (!strcmp(name, CMDS[i].name)) { CMDS[i].fn(argc - 1, argv + 1); return; }
	int hit = -1, hits = 0;
	for (int i = 0; i < NCMDS; i++) if (has_prefix(CMDS[i].name, name)) { hit = i; hits++; }
	if (hits == 1) { CMDS[hit].fn(argc - 1, argv + 1); return; }
	say(C_ERR, "Unknown command `%s`. Type `/help` for the list.", argv[0]);
}

/*---- AUTOCOMPLETE ------------------------*/
typedef struct { char name[40]; const char * desc; } SUG;
#define MAX_SUG		48

static void sug_add(SUG * s, int * n, const char * name, const char * desc, const char * part) {
	if (*n >= MAX_SUG || !has_prefix(name, part)) return;
	snprintf(s[*n].name, sizeof s[*n].name, "%s", name);
	s[*n].desc = desc;
	(*n)++;
}

/* suggestions for the word being typed; *from is where that word starts */
static int suggest(SUG * s, int * from) {
	int n = 0;
	if (g_len == 0 || g_in[0] != '/') return 0;
	char * sp = strchr(g_in, ' ');
	if (!sp) {
		*from = 0;
		for (int i = 0; i < NCMDS; i++) sug_add(s, &n, CMDS[i].name, CMDS[i].desc, g_in);
	} else {
		const char * part = strrchr(g_in, ' ') + 1;
		char name[48];
		size_t len = (size_t)(sp - g_in);
		if (len >= sizeof name) return 0;
		memcpy(name, g_in, len);
		name[len] = 0;
		int c = -1;
		for (int i = 0; i < NCMDS; i++) if (!strcmp(name, CMDS[i].name)) c = i;
		if (c < 0) return 0;
		bool first = part == sp + 1;
		*from = (int)(part - g_in);
		switch (CMDS[c].comp) {
		case COMP_ALGS:
			if (!first && CMDS[c].fn != cmd_race && CMDS[c].fn != cmd_compare && CMDS[c].fn != cmd_run) break;
			for (int i = 0; i < NALGS; i++) sug_add(s, &n, ALGS[i].key, ALGS[i].name, part);
			break;
		case COMP_DATA:
			for (int i = 0; i < K_KINDS - 1; i++) sug_add(s, &n, KIND_NAMES[i], KIND_DESC[i], part);
			break;
		case COMP_MODE:
			sug_add(s, &n, "2d", "a matrix (keeps the current layout)", part);
			for (int i = 0; i < M_MODES; i++) sug_add(s, &n, MODE_NAMES[i], MODE_DESC[i], part);
			break;
		case COMP_GRAD:
			if (!first) break;
			for (int i = 0; i < NGPRESETS; i++) sug_add(s, &n, GPRESETS[i].name, GPRESETS[i].desc, part);
			sug_add(s, &n, "reverse", "flip the current gradient", part);
			break;
		case COMP_COLOR:
			sug_add(s, &n, "256", "xterm 256 colors", part);
			sug_add(s, &n, "truecolor", "24-bit color", part);
			break;
		case COMP_CAT:
			for (int i = 0; i < NCATS; i++) sug_add(s, &n, CATS[i], "category", part);
			break;
		case COMP_SEED:
			sug_add(s, &n, "random", "pick a new seed", part);
			break;
		case COMP_ONOFF:
			sug_add(s, &n, "on", "show Bitsy", part);
			sug_add(s, &n, "off", "hide Bitsy", part);
			break;
		}
	}
	if (n == 1 && !strcmp(s[0].name, g_in + *from)) return 0;	/* already typed in full */
	return n;
}

/*---- IDLE PANEL: input box and what sits under it ----*/
static void idle_panel(LINES * out) {
	SB b = {0};
	int avail = g_cols - 1 - 4 - 2;
	sb_printf(&b, "%s>" RST " ", fg(C_MUTED));
	if (g_len == 0) {
		const char * ph = "Try \"timsort\", \"race quick merge heap\" or /help";
		sb_printf(&b, INV "%c" RST "%s%s", ph[0], fg(C_FAINT), ph + 1);
	} else {
		int start = g_cur >= avail ? g_cur - avail + 1 : 0;
		for (int i = start; i <= g_len && i - start < avail; i++) {
			char ch = i < g_len ? g_in[i] : ' ';
			if (i == g_cur) sb_printf(&b, INV "%c" RST, ch);
			else sb_putc(&b, ch);
		}
	}
	box_lines(out, sb_str(&b));
	sb_clear(&b);

	SUG s[MAX_SUG];
	int from = 0, n = suggest(s, &from);
	if (n > 0) {
		if (g_sel >= n) g_sel = n - 1;
		int shown = n < 7 ? n : 7, first = g_sel - shown / 2;
		if (first < 0) first = 0;
		if (first > n - shown) first = n - shown;
		for (int i = first; i < first + shown; i++) {
			bool on = i == g_sel;
			sb_printf(&b, "  %s%-16s %s%s", fg(on ? C_CODE : C_MUTED), s[i].name, fg(on ? C_CODE : C_DIM), s[i].desc);
			SB c = {0};
			sb_put_clip(&c, sb_str(&b), g_cols - 1);
			sb_puts(&c, RST);
			lines_add(out, sb_str(&c));
			sb_free(&c);
			sb_clear(&b);
		}
		if (n > shown) hint_line(out, "↑/↓ to choose · tab to complete · enter to run", NULL);
	} else if (g_show_help) {
		hint_line(out, "/ for commands          ↑/↓ for history       tab to complete", NULL);
		hint_line(out, "esc to clear input      ctrl+l to redraw      ctrl+c twice to exit", NULL);
		hint_line(out, "playback: space pause · ←/→ step · pgup/pgdn jump · +/- speed · enter finish · esc stop", NULL);
	} else {
		char right[200];
		snprintf(right, sizeof right, "%s · %d %s · %s · seed %lu · %s", ALGS[g_alg].key, g_s.n, KIND_NAMES[g_s.kind],
		         mode_label(), g_seed, g_grad_name);
		if (g_ctrlc_at) hint_line(out, "Press Ctrl-C again to exit", right);
		else hint_line(out, "? for shortcuts", right);
	}
	sb_free(&b);
}

static void banner_lines(LINES * out, int cols) {
	int w = cols - 1 < 60 ? cols - 1 : 60;
	SB b = {0};
	char info[96];
	snprintf(info, sizeof info, "%d sort algorithms · bars, matrices, races · v%s", NALGS, APP_VERSION);
	const char * rows[5] = { NULL, "", "/help for help, /algos for the algorithm list", "", info };
	sb_printf(&b, "%s╭", fg(C_ACCENT));
	sb_rep(&b, "─", w - 2);
	sb_puts(&b, "╮" RST);
	lines_take(out, &b);
	for (int i = 0; i < 5; i++) {
		SB c = {0};
		if (i == 0) {
			sb_printf(&c, "%s✻" RST " Welcome to " BOLD "Sort-CLI" NOBOLD "!  ", fg(C_ACCENT));
			swatch(&c, 18);
		} else sb_printf(&c, "  %s%s", fg(C_MUTED), rows[i]);
		sb_printf(&b, "%s│" RST " ", fg(C_ACCENT));
		int used = sb_put_clip(&b, sb_str(&c), w - 4);
		sb_puts(&b, RST);
		sb_pad(&b, w - 4 - used);
		sb_printf(&b, " %s│" RST, fg(C_ACCENT));
		lines_take(out, &b);
		sb_free(&c);
	}
	sb_printf(&b, "%s╰", fg(C_ACCENT));
	sb_rep(&b, "─", w - 2);
	sb_puts(&b, "╯" RST);
	lines_take(out, &b);
	sb_free(&b);
}

static void welcome(void) {
	block_add(B_BANNER, 0, NULL, NULL);
	block_add(B_BLANK, 0, NULL, NULL);
	say_text("%sTips for getting started:", fg(C_MUTED));
	block_add(B_BLANK, 0, NULL, NULL);
	say_text("1. Type `timsort` or `/run` to watch a sort, `/step heap` to go one step at a time");
	say_text("2. `/race quick merge heap` runs several on the same data");
	say_text("3. `/data runs`, `/data dups`, `/seed 42`: change and reproduce the samples");
	say_text("4. `/mode 2d rows` sorts every row of a matrix at once");
	say_text("5. `/gradient magma`, or your own: `/gradient #1e3a8a #f59e0b`");
}

/*---- LINE EDITOR -------------------------*/
static void input_set(const char * s) {
	snprintf(g_in, sizeof g_in, "%s", s);
	g_len = (int)strlen(g_in);
	g_cur = g_len;
	g_sel = 0;
}

static void history_add(const char * s) {
	if (g_nhist && !strcmp(g_hist[g_nhist - 1], s)) return;
	if (g_nhist == HIST_MAX) {
		free(g_hist[0]);
		memmove(g_hist, g_hist + 1, sizeof(char *) * (HIST_MAX - 1));
		g_nhist--;
	}
	g_hist[g_nhist++] = xstrdup(s);
}

static void submit(void) {
	char line[INPUT_MAX + 1];
	snprintf(line, sizeof line, "%s", g_in);
	input_set("");
	g_show_help = false;
	char * p = line;
	while (*p == ' ') p++;
	size_t n = strlen(p);
	while (n && p[n - 1] == ' ') p[--n] = 0;
	if (!n) { g_hpos = g_nhist; return; }
	history_add(p);
	g_hpos = g_nhist;
	say_user(p);
	execute(p);
}

/* apply the selected suggestion; true when there was one */
static bool complete(bool add_space) {
	SUG s[MAX_SUG];
	int from = 0, n = suggest(s, &from);
	if (n <= 0) return false;
	char buf[INPUT_MAX + 1];
	snprintf(buf, sizeof buf, "%.*s%s%s", from, g_in, s[g_sel < n ? g_sel : 0].name, add_space ? " " : "");
	input_set(buf);
	return true;
}

static void handle_key(int k) {
	SUG s[MAX_SUG];
	int from, nsug = suggest(s, &from);
	companion_event(CE_KEY);
	if (k != K_CTRL('c')) g_ctrlc_at = 0;
	if (k != '?') g_show_help = false;
	switch (k) {
	case K_ENTER: case '\n':
		if (nsug > 0) complete(false);
		submit();
		break;
	case K_TAB:
		complete(true);
		break;
	case K_UP:
		if (nsug > 0) { g_sel = (g_sel + nsug - 1) % nsug; break; }
		if (g_hpos > 0) {
			if (g_hpos == g_nhist) snprintf(g_hsave, sizeof g_hsave, "%s", g_in);
			input_set(g_hist[--g_hpos]);
		}
		break;
	case K_DOWN: case K_SHIFT_TAB:
		if (nsug > 0) { g_sel = (g_sel + 1) % nsug; break; }
		if (g_hpos < g_nhist) {
			g_hpos++;
			input_set(g_hpos == g_nhist ? g_hsave : g_hist[g_hpos]);
		}
		break;
	case K_LEFT:  if (g_cur > 0) g_cur--; break;
	case K_RIGHT: if (g_cur < g_len) g_cur++; break;
	case K_HOME: case K_CTRL('a'): g_cur = 0; break;
	case K_END:  case K_CTRL('e'): g_cur = g_len; break;
	case K_BACKSPACE: case 8:
		if (g_cur > 0) {
			memmove(g_in + g_cur - 1, g_in + g_cur, (size_t)(g_len - g_cur + 1));
			g_cur--; g_len--; g_sel = 0;
		}
		break;
	case K_DEL:
		if (g_cur < g_len) {
			memmove(g_in + g_cur, g_in + g_cur + 1, (size_t)(g_len - g_cur));
			g_len--; g_sel = 0;
		}
		break;
	case K_CTRL('u'):
		memmove(g_in, g_in + g_cur, (size_t)(g_len - g_cur + 1));
		g_len -= g_cur; g_cur = 0; g_sel = 0;
		break;
	case K_CTRL('k'):
		g_in[g_cur] = 0; g_len = g_cur; g_sel = 0;
		break;
	case K_CTRL('w'): {
		int i = g_cur;
		while (i > 0 && g_in[i - 1] == ' ') i--;
		while (i > 0 && g_in[i - 1] != ' ') i--;
		memmove(g_in + i, g_in + g_cur, (size_t)(g_len - g_cur + 1));
		g_len -= g_cur - i; g_cur = i; g_sel = 0;
		break;
	}
	case K_CTRL('l'):
		ui_repaint();
		break;
	case K_ESC:
		input_set("");
		break;
	case K_CTRL('c'):
		if (g_len) input_set("");
		else if (g_ctrlc_at && now_ms() - g_ctrlc_at < 2000) g_quit = true;
		else g_ctrlc_at = now_ms();
		break;
	case K_CTRL('d'):
		if (!g_len) g_quit = true;
		break;
	default:
		if (k == '?' && g_len == 0) { g_show_help = !g_show_help; break; }
		if (k >= 32 && k < 127 && g_len < INPUT_MAX) {
			memmove(g_in + g_cur + 1, g_in + g_cur, (size_t)(g_len - g_cur + 1));
			g_in[g_cur++] = (char)k;
			g_len++;
			g_sel = 0;
		}
	}
}

/*---- MAIN LOOP ---------------------------*/
static void app_setup(void) {
	term_size(&g_cols, &g_rows);
	if (!g_grad_n) grad_init();
	g_alg = find_alg("timsort");
	g_n1 = (g_cols - 3) / 3;
	if (g_n1 > 32) g_n1 = 32;
	if (g_n1 < 8) g_n1 = 8;
	fit_2d(&g_w, &g_h);
	if (g_w > 32) g_w = 32;
	if (g_h > 12) g_h = 12;
	regen();
}

static void repl(void) {
	welcome();
	if (!g_interactive) {						/* piped input: one request per line */
		char line[INPUT_MAX + 2];
		ui_flush();
		while (!g_quit && fgets(line, sizeof line, stdin)) {
			line[strcspn(line, "\r\n")] = 0;
			input_set(line);
			submit();
			ui_flush();
		}
		return;
	}
	g_pet_last_key = now_ms();
	pet_say("hi!", 3000);
	bool redraw = true;
	while (!g_quit && !g_eof) {
		ui_check_resize();
		if (redraw) { ui_flush(); pet_dirty(); }
		int k = term_key(pet_fits() ? 200 : g_ctrlc_at ? 250 : 1000);	/* ticks only to animate Bitsy */
		if (k == K_TIMEOUT) {
			redraw = pet_dirty();
			if (g_ctrlc_at && now_ms() - g_ctrlc_at > 2000) { g_ctrlc_at = 0; redraw = true; }
			continue;
		}
		if (k == K_EOF) break;
		handle_key(k);
		redraw = true;
	}
	SB o = {0};									/* leave the transcript, drop the live region */
	dyn_erase(&o);
	term_write(sb_str(&o), o.len);
	sb_free(&o);
}

#endif
