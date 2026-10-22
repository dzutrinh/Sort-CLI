/*
 *	TEST.C
 *	------
 *	Test suite for Sort-CLI: runs every algorithm without the UI and checks
 *	the results, the traces, seeking, the 2D layouts, rendering widths,
 *	gradients and the commands.
 *	Usage: sort-cli-test [seed]
 *	Part of Sort-CLI, coded by Trinh D.D. Nguyen, MIT licensed
 */

#include "sort-cli.h"

static int g_checks = 0, g_failures = 0, g_mark = 0;
static const char * g_section = "";

#define CHECK(cond, ...) do { \
	g_checks++; \
	if (!(cond)) { \
		if (++g_failures <= 30) { \
			printf("\n  FAIL [%s] ", g_section); \
			printf(__VA_ARGS__); \
		} \
	} \
} while (0)

static void begin(const char * name) {
	g_section = name;
	g_mark = g_failures;
	printf("%-46s", name);
	fflush(stdout);
}

static void end(void) { printf("%s\n", g_failures == g_mark ? "ok" : "\nFAILED"); }

/*---- helpers -----------------------------*/
static bool fast_alg(const ALG * a) {
	if (a->max_n) return false;					/* the deliberately slow ones */
	return strstr(a->worst, "log") || strstr(a->worst, "d·n") || strstr(a->avg, "n + k");
}

/* replay every event onto a copy of the start; check it matches the result */
static void check_replay(const ALG * al, const TRACE * T) {
	EL * a = tmp_alloc(T->len);
	memcpy(a, T->start, sizeof(EL) * (size_t)T->len);
	long cm = 0, sw = 0, wr = 0;
	bool idx_ok = true;
	for (int i = 0; i < T->n; i++) {
		const EV * e = &T->ev[i];
		switch (e->op) {
		case OP_CMP:   cm++; if ((e->a != NIL && e->a >= T->len) || (e->b != NIL && e->b >= T->len)) idx_ok = false; break;
		case OP_SWAP:  sw++; { EL t = a[e->a]; a[e->a] = a[e->b]; a[e->b] = t; } break;
		case OP_WRITE: wr++; a[e->a].v = e->v; a[e->a].id = e->id; break;
		case OP_DONE:  if (e->a > e->b || e->b >= T->len) idx_ok = false; break;
		case OP_NOTE:  if (e->a >= T->nnotes) idx_ok = false; break;
		}
	}
	CHECK(idx_ok, "%s: an event points outside the array", al->key);
	CHECK(cm == T->cmps && sw == T->swaps && wr == T->writes,
	      "%s: counters %ld/%ld/%ld but events %ld/%ld/%ld", al->key, T->cmps, T->swaps, T->writes, cm, sw, wr);
	CHECK(!memcmp(a, T->final, sizeof(EL) * (size_t)T->len), "%s n=%d: replay differs from the result", al->key, T->len);
	free(a);
}

static void check_result(const ALG * al, const TRACE * T, const char * kind) {
	static unsigned char seen[MAX_CELLS];
	memset(seen, 0, (size_t)T->len);
	bool perm = true;
	for (int i = 0; i < T->len; i++) {
		int id = T->final[i].id;
		if (id < 0 || id >= T->len || seen[id] || T->start[id].v != T->final[i].v) perm = false;
		else seen[id] = 1;
	}
	CHECK(perm, "%s on %s n=%d: output is not a permutation of the input", al->key, kind, T->len);
	CHECK(lane_sorted(T), "%s on %s n=%d: not sorted", al->key, kind, T->len);
	if (al->stable) CHECK(unstable_pairs(T) == 0, "%s on %s n=%d: claims stable but %d pairs moved", al->key, kind, T->len, unstable_pairs(T));
	const EV * last = &T->ev[T->n - 1];
	CHECK(last->op == OP_DONE && last->a == 0 && last->b == T->len - 1, "%s: trace does not end with everything done", al->key);
}

/* random seeks must equal a fresh replay */
static void check_seeks(const ALG * al, const TRACE * T) {
	LANE L, F;
	lane_init(&L, T, 1);
	lane_init(&F, T, 1);
	for (int t = 0; t < 12; t++) {
		int k = rng_int(T->n + 1);
		lane_seek(&L, k);
		lane_reset(&F);
		while (F.pos < k) lane_apply(&F, &T->ev[F.pos++]);
		bool same = !memcmp(L.a, F.a, sizeof(EL) * (size_t)T->len) && !memcmp(L.dn, F.dn, (size_t)T->len)
		         && L.m.cmps == F.m.cmps && L.m.swaps == F.m.swaps && L.m.writes == F.m.writes
		         && L.m.lo == F.m.lo && L.m.hi == F.m.hi && L.m.pivot == F.m.pivot && L.m.note == F.m.note;
		CHECK(same, "%s: seeking to %d of %d disagrees with a replay", al->key, k, T->n);
	}
	lane_free(&L);
	lane_free(&F);
}

/*---- tests -------------------------------*/
static void test_samples(void) {
	begin("sample generators and seeds");
	SAMPLES a, b;
	for (int k = 0; k < K_KINDS - 1; k++)
		for (int n = 1; n <= 300; n += 7) {
			samples_generate(&a, k, n, 1234 + n);
			samples_generate(&b, k, n, 1234 + n);
			CHECK(a.n == n && a.kind == k, "%s: size %d kind %d", KIND_NAMES[k], a.n, a.kind);
			CHECK(!memcmp(a.a, b.a, sizeof(EL) * (size_t)n), "%s n=%d: same seed gave different samples", KIND_NAMES[k], n);
			bool ok = true;
			for (int i = 0; i < n; i++) if (a.a[i].v < 1 || a.a[i].v > n || a.a[i].id != i) ok = false;
			CHECK(ok, "%s n=%d: values outside 1..n or ids out of order", KIND_NAMES[k], n);
			if (k == K_SORTED) CHECK(lane_sorted(&(TRACE){ .len = n, .final = a.a }), "sorted n=%d is not sorted", n);
		}
	samples_generate(&a, K_RANDOM, 64, 1);
	samples_generate(&b, K_RANDOM, 64, 2);
	CHECK(memcmp(a.a, b.a, sizeof(EL) * 64), "different seeds gave the same random samples");
	end();
}

static void test_algorithms(void) {
	begin("every algorithm on every kind and size");
	static const int SIZES[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 15, 16, 17, 20, 24, 31, 32, 33, 40, 63, 64, 65, 100, 257 };
	SAMPLES s;
	TRACE T;
	int runs = 0;
	for (int ai = 0; ai < NALGS; ai++) {
		const ALG * al = &ALGS[ai];
		if (al->only2d) continue;
		for (int k = 0; k < K_KINDS - 1; k++)
			for (size_t si = 0; si < sizeof SIZES / sizeof SIZES[0]; si++) {
				int n = SIZES[si];
				if (al->max_n && n > al->max_n) continue;
				samples_generate(&s, k, n, (unsigned long)(ai * 977 + k * 31 + n));
				bool fits = sort_run(al, s.a, n, n, 1, &T);
				CHECK(fits, "%s on %s n=%d: trace overflow", al->key, KIND_NAMES[k], n);
				if (fits) {
					check_result(al, &T, KIND_NAMES[k]);
					check_replay(al, &T);
					if (n == 33 || n == 100) check_seeks(al, &T);
				}
				trace_free(&T);
				runs++;
			}
		if (fast_alg(al))								/* big inputs for the n log n ones */
			for (int k = 0; k < K_KINDS - 1; k++) {
				samples_generate(&s, k, MAX_CELLS, (unsigned long)(ai + k));
				if (sort_run(al, s.a, MAX_CELLS, MAX_CELLS, 1, &T)) { check_result(al, &T, KIND_NAMES[k]); check_replay(al, &T); }
				else CHECK(false, "%s on %s n=%d: trace overflow", al->key, KIND_NAMES[k], MAX_CELLS);
				trace_free(&T);
				runs++;
			}
	}
	printf("(%d runs) ", runs);
	end();
}

static void test_custom_values(void) {
	begin("negative, large and repeated custom values");
	TRACE T;
	EL in[60];
	for (int round = 0; round < 40; round++) {
		int n = 2 + rng_int(58);
		for (int i = 0; i < n; i++) { in[i].v = rng_int(4) == 0 ? in[rng_int(i + 1)].v : rng_int(11000) - 999; in[i].id = i; }
		for (int ai = 0; ai < NALGS; ai++) {
			const ALG * al = &ALGS[ai];
			if (al->only2d || (al->max_n && n > al->max_n)) continue;
			if (sort_run(al, in, n, n, 1, &T)) { check_result(al, &T, "custom"); check_replay(al, &T); }
			trace_free(&T);
		}
	}
	end();
}

static void test_timsort(void) {
	begin("TimSort runs, merges and galloping");
	SAMPLES s;
	TRACE T;
	int ti = find_alg("timsort"), gallops = 0, merges = 0;
	for (int round = 0; round < 60; round++) {
		int n = 16 + rng_int(MAX_CELLS - 16);
		samples_generate(&s, round % 3 == 0 ? K_RUNS : round % 3 == 1 ? K_SAWTOOTH : K_RANDOM, n, (unsigned long)round);
		sort_run(&ALGS[ti], s.a, n, n, 1, &T);
		check_result(&ALGS[ti], &T, "timsort");
		for (int i = 0; i < T.nnotes; i++) {
			if (strstr(T.notes[i], "gallop")) gallops++;
			if (strstr(T.notes[i], "Merge runs")) merges++;
		}
		trace_free(&T);
	}
	CHECK(merges > 0, "no merges recorded");
	CHECK(gallops > 0, "galloping never happened");
	samples_generate(&s, K_SORTED, 500, 1);					/* one run: no merges, n - 1 comparisons */
	sort_run(&ALGS[ti], s.a, 500, 500, 1, &T);
	CHECK(T.cmps == 499 && T.writes == 0, "sorted input took %ld comparisons and %ld writes", T.cmps, T.writes);
	trace_free(&T);
	samples_generate(&s, K_REVERSED, 500, 1);				/* one descending run: reversed in place */
	sort_run(&ALGS[ti], s.a, 500, 500, 1, &T);
	CHECK(T.cmps == 499 && T.swaps == 250 && T.writes == 0, "reversed input: %ld comparisons, %ld swaps", T.cmps, T.swaps);
	trace_free(&T);
	end();
}

static void test_shear(void) {
	begin("shear sort ends in snake order");
	SAMPLES s;
	TRACE T;
	int si = find_alg("shear");
	for (int round = 0; round < 200; round++) {
		int w = 2 + rng_int(20), h = 2 + rng_int(12);
		samples_generate(&s, round % (K_KINDS - 1), w * h, (unsigned long)round);
		sort_run(&ALGS[si], s.a, w * h, w, h, &T);
		CHECK(snake_sorted(&T, w, h), "%dx%d not in snake order", w, h);
		check_replay(&ALGS[si], &T);
		trace_free(&T);
	}
	end();
}

static void test_layouts(void) {
	begin("2D layouts and races");
	SAMPLES s;
	VIEW * v = xrealloc(NULL, sizeof(VIEW));
	int list[3] = { find_alg("insertion"), find_alg("heap"), find_alg("timsort") };
	for (int round = 0; round < 60; round++) {
		int w = 2 + rng_int(30), h = 2 + rng_int(12), map = M_FLAT + round % 4;
		samples_generate(&s, round % (K_KINDS - 1), w * h, (unsigned long)round);
		view_run(v, V_HEAT, map, w, h, &s, list + round % 3, 1);
		int lanes = map == M_ROWS ? h : map == M_COLS ? w : 1;
		CHECK(v->nl == lanes, "%s: %d lanes, expected %d", MODE_NAMES[map], v->nl, lanes);
		static int grid[MAX_CELLS];
		static unsigned char seen[MAX_CELLS];
		memset(seen, 0, sizeof seen);
		bool cover = true;
		for (int l = 0; l < v->nl; l++) {
			CHECK(lane_sorted(&v->T[l]), "%s lane %d not sorted", MODE_NAMES[map], l);
			for (int i = 0; i < v->T[l].len; i++) {
				int x, y;
				cell_of(v, l, i, &x, &y);
				if (x < 0 || y < 0 || x >= w || y >= h || seen[y * w + x]) cover = false;
				else { seen[y * w + x] = 1; grid[y * w + x] = v->T[l].final[i].v; }
			}
		}
		CHECK(cover, "%s %dx%d: lanes do not cover every cell exactly once", MODE_NAMES[map], w, h);
		if (map == M_SNAKE) {
			bool ok = true;
			for (int i = 1; i < w * h; i++) {
				int y0 = (i - 1) / w, x0 = (y0 & 1) ? w - 1 - (i - 1) % w : (i - 1) % w;
				int y1 = i / w, x1 = (y1 & 1) ? w - 1 - i % w : i % w;
				if (grid[y0 * w + x0] > grid[y1 * w + x1]) ok = false;
			}
			CHECK(ok, "snake %dx%d: the matrix is not in snake order", w, h);
		}
		view_free(v);
	}
	samples_generate(&s, K_RANDOM, 40, 3);
	CHECK(view_run(v, V_RACE, M_1D, 0, 0, &s, list, 3) && v->nl == 3, "race of 3 did not make 3 lanes");
	for (int l = 0; l < v->nl; l++) CHECK(lane_sorted(&v->T[l]), "race lane %d not sorted", l);
	view_free(v);
	free(v);
	end();
}

static void check_widths(const LINES * L, int cols, const char * what) {
	for (int i = 0; i < L->n; i++)
		CHECK(vis_width(L->v[i]) <= cols - 1, "%s: line %d is %d columns in a %d-column window", what, i, vis_width(L->v[i]), cols);
}

static void test_rendering(void) {
	begin("frames fit the window");
	static const int COLS[] = { 50, 80, 120, 200 };
	SAMPLES s;
	LINES out = {0};
	VIEW * v = xrealloc(NULL, sizeof(VIEW));
	int algs[6] = { find_alg("timsort"), find_alg("quick"), find_alg("radix"), find_alg("heap"), find_alg("smooth"), find_alg("shear") };
	for (int c = 0; c < 4; c++) {
		int cols = COLS[c];
		g_cols = cols;
		int sizes[3] = { (cols - 3) / 3, (cols - 3) / 2, cols - 3 };
		for (int z = 0; z < 3; z++) {
			samples_generate(&s, K_RANDOM, sizes[z], 9);
			for (int a = 0; a < 5; a++) {
				view_run(v, V_BARS, M_1D, 0, 0, &s, algs + a, 1);
				view_lanes(v);
				for (int k = 0; k <= v->total; k += v->total / 25 + 1) {
					view_render(&out, v, k, cols, 20, true);
					CHECK(out.n <= 20, "bars: %d lines for 20 rows", out.n);
					check_widths(&out, cols, ALGS[algs[a]].key);
					lines_clear(&out);
				}
				view_free(v);
			}
			view_run(v, V_RACE, M_1D, 0, 0, &s, algs, 5);
			view_lanes(v);
			view_render(&out, v, v->total / 2, cols, 24, true);
			CHECK(out.n <= 24, "race: %d lines for 24 rows", out.n);
			check_widths(&out, cols, "race");
			lines_clear(&out);
			view_free(v);
		}
		int w = (cols - 6) / 2 < 64 ? (cols - 6) / 2 : 64, h = 8;
		samples_generate(&s, K_RANDOM, w * h, 4);
		for (int map = M_FLAT; map < M_MODES; map++) {
			view_run(v, V_HEAT, map, w, h, &s, algs + (map == M_FLAT ? 5 : 0), 1);
			view_lanes(v);
			view_render(&out, v, v->total / 3, cols, 30, true);
			CHECK(out.n == h + 5, "heat: %d lines for height %d", out.n, h);
			check_widths(&out, cols, MODE_NAMES[map]);
			lines_clear(&out);
			view_free(v);
		}
	}
	free(v);
	lines_free(&out);
	g_cols = 100;
	end();
}

static void test_gradients(void) {
	begin("gradients: presets, stops, 256 colors");
	for (int n = 16; n < 256; n++) CHECK(nearest256(xterm_rgb(n)) == n || xterm_rgb(nearest256(xterm_rgb(n))) == xterm_rgb(n),
	                                     "xterm %d maps to %d", n, nearest256(xterm_rgb(n)));
	unsigned c;
	CHECK(grad_parse_color("#ff8800", &c) && c == 0xff8800, "#ff8800 parsed as %06x", c);
	CHECK(grad_parse_color("ff8800", &c) && c == 0xff8800, "ff8800 parsed as %06x", c);
	CHECK(grad_parse_color("#f80", &c) && c == 0xff8800, "#f80 parsed as %06x", c);
	CHECK(grad_parse_color("196", &c) && c == 0xff0000, "196 parsed as %06x", c);
	CHECK(!grad_parse_color("256", &c) && !grad_parse_color("#12345", &c) && !grad_parse_color("blue", &c), "bad colors accepted");
	bool tc = g_truecolor;
	for (int t = 0; t < 2; t++) {
		g_truecolor = t;
		for (int p = 0; p < NGPRESETS; p++) {
			grad_use_preset(p);
			CHECK(!strcmp(g_grad_name, GPRESETS[p].name) && grad_find(GPRESETS[p].name) == p, "preset %s", GPRESETS[p].name);
			unsigned first = GPRESETS[p].xterm ? xterm_rgb((int)GPRESETS[p].stops[0]) : GPRESETS[p].stops[0];
			CHECK(grad_at(0) == first, "%s starts at %06x, not %06x", GPRESETS[p].name, grad_at(0), first);
			g_grad_rev = true;
			CHECK(grad_at(1) == first, "%s reversed does not end at its first color", GPRESETS[p].name);
			g_grad_rev = false;
			for (int i = 0; i <= 20; i++) {
				int col = shade(i, 20);
				CHECK(t ? (col & TRUECOLOR) != 0 : (col >= 16 && col < 256), "%s shade %d = %x", GPRESETS[p].name, i, col);
				CHECK(strlen(fg(col)) < 24 && strlen(bg(col)) < 24, "escape too long");
			}
		}
	}
	g_truecolor = tc;
	grad_use_preset(0);
	end();
}

static void test_pet(void) {
	begin("Bitsy: moods, strip, hiding");
	bool ia = g_interactive;
	int cols = g_cols, rows = g_rows;
	g_interactive = true;
	g_cols = 100; g_rows = 30;
	char out[PET_ROWS][1024];
	for (int m = PM_IDLE; m <= PM_SLEEP; m++)
		for (long long f = 0; f < 32; f++) {
			pet_lines(out, m, f);
			for (int y = 0; y < PET_ROWS; y++) CHECK(vis_width(out[y]) == PET_W, "mood %d row %d is %d wide", m, y, vis_width(out[y]));
		}
	g_pet_bars[0] = 5; g_pet_bars[1] = 2; g_pet_bars[2] = 4; g_pet_bars[3] = 1; g_pet_bars[4] = 3;
	g_pet_step_f = -1;
	bool reached = false;
	for (long long f = 0; f < 200 && !reached; f++) {
		pet_bars_step(f);
		reached = g_pet_bars[0] == 1 && g_pet_bars[1] == 2 && g_pet_bars[2] == 3 && g_pet_bars[3] == 4 && g_pet_bars[4] == 5;
	}
	CHECK(reached, "the work mood never sorts its bars");
	pet_say("sorted ✓", 3000);
	LINES L = {0};
	pet_strip(&L, "✻ Sorting… (step 3/9)");
	CHECK(L.n == PET_ROWS, "strip has %d rows", L.n);
	for (int i = 0; i < L.n; i++) CHECK(vis_width(L.v[i]) <= g_cols - 1, "strip row %d is %d wide", i, vis_width(L.v[i]));
	CHECK(strstr(L.v[PET_ROWS - 1], "Sorting") && strstr(L.v[2], "sorted"), "spinner or bubble missing from the strip");
	lines_free(&L);
	CHECK(pet_fits() && companion_rows() == PET_ROWS - 1, "pet should show at 100x30");
	g_cols = 59;  CHECK(!pet_fits(), "pet shown in 59 columns");
	g_cols = 100; g_rows = 21; CHECK(!pet_fits(), "pet shown in 21 rows");
	g_rows = 30; g_interactive = false; CHECK(!pet_fits() && companion_rows() == 0, "pet shown when piped");
	g_interactive = true; g_pet_on = false; CHECK(!pet_fits(), "pet shown after /pet off");
	g_pet_on = true;
	g_pet_mood = PM_IDLE; g_pet_last_key = now_ms() - PET_SLEEP_MS - 1;
	CHECK(pet_mood() == PM_SLEEP, "no sleep after a minute");
	pet_wake();
	CHECK(pet_mood() == PM_IDLE, "a key did not wake it");
	companion_event(CE_START); CHECK(g_pet_mood == PM_WORK, "no work mood on start");
	companion_event(CE_OK);    CHECK(g_pet_mood == PM_FOUND && !strcmp(pet_bubble(PM_FOUND, 0), "sorted ✓"), "no success mood");
	companion_event(CE_ERR);   CHECK(g_pet_mood == PM_MISS && !strcmp(pet_bubble(PM_MISS, 0), "oops"), "no miss mood");
	g_pet_mood = PM_IDLE;
	g_interactive = ia; g_cols = cols; g_rows = rows;
	end();
}

static bool last_bullet_is(int color) {
	for (int i = g_nblocks - 1; i >= 0; i--)
		if (g_blocks[i].kind == B_BULLET) return g_blocks[i].color == color;
	return false;
}

static bool transcript_has(const char * text) {
	for (int i = 0; i < g_nblocks; i++) if (strstr(g_blocks[i].text, text)) return true;
	return false;
}

static void run_cmd(const char * line) { ui_clear(); execute(line); }

static void test_commands(void) {
	begin("commands and plain requests");
	static const char * OK[] = {
		"/help", "/algos", "/algos merge", "/run", "/run bubble", "/step heap", "/race", "/race quick merge heap",
		"/compare", "/compare timsort merge", "/explain", "/explain smooth", "/use quick3", "/data", "/data runs",
		"/data dups 20", "/data 5 3 9 1", "/size 24", "/size 12x5", "/mode", "/mode 1d", "/mode 2d rows", "/mode cols",
		"/mode snake", "/mode 1d", "/show", "/gradient", "/gradient magma", "/gradient reverse", "/gradient #102030 #f0e0d0",
		"/gradient 21 51 226", "/color", "/color truecolor", "/color 256", "/seed", "/seed 42", "/seed random",
		"/speed 50", "/duration 5", "/expl tim", "timsort", "race quick merge", "explain timsort", "reversed 20",
		"2d rows", "1d", "compare heap merge", "step insertion", "help", "/pet", "/pet off", "/pet on",
		"/gradient viridis reverse", "/gradient cividis",
	};
	for (size_t i = 0; i < sizeof OK / sizeof OK[0]; i++) {
		run_cmd(OK[i]);
		CHECK(g_nblocks > 0, "`%s` said nothing", OK[i]);
		CHECK(!last_bullet_is(C_ERR), "`%s` answered with an error", OK[i]);
	}
	static const char * BAD[] = { "/frob", "/run nosuch", "/data weird", "/size 1", "/mode cube", "/gradient nope",
	                              "/gradient #ff0000", "/color 16", "/seed x", "/speed 1", "/duration 0", "/pet maybe" };
	execute("/mode 1d");
	for (size_t i = 0; i < sizeof BAD / sizeof BAD[0]; i++) {
		run_cmd(BAD[i]);
		CHECK(last_bullet_is(C_ERR), "`%s` should be an error", BAD[i]);
	}
	run_cmd("/run shear");
	CHECK(last_bullet_is(C_ERR) && transcript_has("matrix"), "shear sort ran in 1D");
	run_cmd("/size 30");
	run_cmd("/run bogo");
	CHECK(last_bullet_is(C_ERR) && transcript_has("limited to 7"), "bogosort ran on 30 values");
	run_cmd("/seed 42");
	SAMPLES a = g_s;
	run_cmd("/seed 42");
	CHECK(!memcmp(a.a, g_s.a, sizeof(EL) * (size_t)g_s.n), "the same seed gave different samples");
	run_cmd("/data 5 1 4 1");
	CHECK(g_s.n == 4 && g_s.kind == K_CUSTOM && g_s.a[0].v == 5, "custom values not loaded");
	run_cmd("/run merge");
	CHECK(last_bullet_is(C_OK) && transcript_has("Stable here"), "merge on custom duplicates");
	run_cmd("/exit");
	CHECK(g_quit, "/exit did not quit");
	ui_clear();
	end();
}

int main(int argc, char ** argv) {
	unsigned long long seed = argc > 1 ? strtoull(argv[1], NULL, 10) : (unsigned long long)time(NULL) % 100000;
	setvbuf(stdout, NULL, _IONBF, 0);
	(void)term_start;					/* the interactive parts are not tested here */
	(void)repl;
	g_color = false;
	g_interactive = false;
	g_mute = true;
	g_cols = 100;
	g_rows = 32;
	app_setup();
	printf("Sort-CLI test suite, seed %llu (rerun with: sort-cli-test %llu)\n\n", seed, seed);
	rng_seed(seed);
	test_samples();
	test_algorithms();
	test_custom_values();
	test_timsort();
	test_shear();
	test_layouts();
	test_rendering();
	test_gradients();
	test_pet();
	test_commands();
	printf("\n%d checks, %d failures\n", g_checks, g_failures);
	return g_failures ? 1 : 0;
}
