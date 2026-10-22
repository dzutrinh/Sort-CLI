/*
 *	PET.H
 *	-----
 *	Bitsy, an orange bar critter that lives above the prompt, carrying five
 *	bars in the current gradient. 14x12 pixels drawn with half blocks (one
 *	cell = two stacked pixels), so 14 columns x 6 rows. Moods: idle (blinks),
 *	work (bubble-sorts its bars while a sort plays), sorted, miss, sleep
 *	(after a minute without keys). Same layout and timing as Search-CLI's Hoot.
 *	Part of Sort-CLI, coded by Trinh D.D. Nguyen, MIT licensed
 */
#ifndef SORTCLI_PET_H
#define SORTCLI_PET_H

#define PET_W		14
#define PET_H		12
#define PET_ROWS	(PET_H / 2)
#define PET_SLEEP_MS	60000

enum { PM_IDLE, PM_WORK, PM_FOUND, PM_MISS, PM_SLEEP };

/* the body sits under four rows of bars; the last pixel row stays empty */
static const char * BITSY[7] = {
	".OOOOOOOOOOOO.",
	".OWWOOOOOOWWO.",
	".OWKOOOOOOWKO.",
	".OOOOOOOOOOOO.",
	".OOOOOOOOOOOO.",
	"..OOOOOOOOOO..",
	"..KK......KK..",
};

static bool g_pet_on = true;
static int g_pet_mood = PM_IDLE;
static long long g_pet_until = 0;		/* when a sorted/miss mood ends */
static long long g_pet_last_key = 0;
static char g_pet_say[40] = "";
static long long g_pet_say_until = 0;
static char * g_pet_drawn = NULL;		/* last strip drawn, to skip idle redraws */
static int g_pet_bars[5] = { 5, 2, 4, 1, 3 };
static long long g_pet_step_f = -1;		/* last frame the bars took a sorting step */

static int pet_color(char c) {
	switch (c) {
		case 'O': return 173;			/* Claude orange */
		case 'W': return 231;
		case 'K': return 16;
	}
	if (c >= '1' && c <= '5') return shade(c - '1', 4);	/* bars follow /gradient */
	return -1;
}

static bool pet_fits(void) {
	return g_pet_on && g_interactive && g_cols >= 60 && g_rows >= 22;
}

/* extra rows the pet strip adds to the live region */
static int pet_extra_rows(void) { return pet_fits() ? PET_ROWS - 1 : 0; }

static void pet_say(const char * text, int ms) {
	snprintf(g_pet_say, sizeof g_pet_say, "%s", text);
	g_pet_say_until = now_ms() + ms;
}

static void pet_set(int mood, const char * text, int ms) {
	g_pet_mood = mood;
	g_pet_until = now_ms() + ms;
	if (text) pet_say(text, ms);
	else g_pet_say_until = 0;			/* the mood's own bubble takes over */
}

/* any key: note the activity and wake Bitsy up */
static void pet_wake(void) {
	g_pet_last_key = now_ms();
	if (g_pet_mood == PM_SLEEP) {
		g_pet_mood = PM_IDLE;
		pet_say("huh?", 1500);
	}
}

static int pet_mood(void) {
	long long t = now_ms();
	if ((g_pet_mood == PM_FOUND || g_pet_mood == PM_MISS) && t > g_pet_until) g_pet_mood = PM_IDLE;
	if (g_pet_mood == PM_IDLE && g_pet_last_key && t - g_pet_last_key > PET_SLEEP_MS) g_pet_mood = PM_SLEEP;
	return g_pet_mood;
}

static void put_px(char r[PET_H][PET_W + 1], int y, int x, const char * s) {
	for (; *s && x < PET_W; s++, x++) r[y][x] = *s;
}

/* one bubble-sort step every third frame, starting over once sorted */
static void pet_bars_step(long long f) {
	if (f % 3 || f == g_pet_step_f) return;
	g_pet_step_f = f;
	int i = (int)((f / 3) % 4), *b = g_pet_bars;
	if (b[i] > b[i + 1]) { int t = b[i]; b[i] = b[i + 1]; b[i + 1] = t; }
	bool sorted = true;
	for (int k = 0; k < 4; k++) if (b[k] > b[k + 1]) sorted = false;
	if (sorted && (f / 3) % 4 == 3) { b[0] = 5; b[1] = 2; b[2] = 4; b[3] = 1; b[4] = 3; }
}

static void bitsy_pixels(char r[PET_H][PET_W + 1], int mood, long long f) {
	static const int SORTED[5] = { 1, 2, 3, 4, 5 }, MESSY[5] = { 3, 5, 1, 4, 2 };
	if (mood == PM_WORK) pet_bars_step(f);
	const int * b = mood == PM_FOUND || mood == PM_SLEEP ? SORTED : mood == PM_MISS ? MESSY : g_pet_bars;
	for (int y = 0; y < PET_H; y++) { memset(r[y], '.', PET_W); r[y][PET_W] = 0; }
	for (int i = 0; i < 5; i++) {						/* bars: rows 0..3, the tallest is cut at the top */
		char px[3] = { (char)('0' + b[i]), (char)('0' + b[i]), 0 };
		for (int k = 0; k < b[i]; k++) if (3 - k >= 0) put_px(r, 3 - k, 2 + 2 * i, px);
	}
	for (int y = 0; y < 7; y++) memcpy(r[4 + y], BITSY[y], PET_W);
	#define BODY_PX(y, x, s) put_px(r, 4 + (y), (x), (s))
	if (mood == PM_FOUND) {								/* ^‿^ */
		BODY_PX(1, 2, "OO"); BODY_PX(1, 10, "OO"); BODY_PX(2, 2, "KK"); BODY_PX(2, 10, "KK");
		BODY_PX(3, 5, "KOOK"); BODY_PX(4, 6, "KK");
	} else if (mood == PM_MISS) {
		BODY_PX(3, 6, "KK"); BODY_PX(4, 5, "KOOK");
	} else if (mood == PM_SLEEP || (mood == PM_IDLE && f % 16 == 15)) {	/* asleep, or a blink */
		BODY_PX(1, 2, "OO"); BODY_PX(1, 10, "OO"); BODY_PX(2, 2, "KK"); BODY_PX(2, 10, "KK");
		BODY_PX(4, 6, "KK");
	} else {
		if (mood == PM_WORK && f % 4 < 2) { BODY_PX(2, 2, "WK"); BODY_PX(2, 10, "WK"); }	/* glance */
		BODY_PX(3, 5, "KOOK"); BODY_PX(4, 6, "KK");
	}
	if (mood == PM_WORK && f % 2) { BODY_PX(6, 2, ".KK"); BODY_PX(6, 9, "KK."); }		/* step */
	#undef BODY_PX
}

/* sprite as PET_ROWS lines of half blocks */
static void pet_lines(char out[PET_ROWS][1024], int mood, long long f) {
	char r[PET_H][PET_W + 1];
	bitsy_pixels(r, mood, f);
	for (int y = 0; y < PET_ROWS; y++) {
		SB b = {0};
		for (int x = 0; x < PET_W; x++) {
			int t = pet_color(r[2 * y][x]), u = pet_color(r[2 * y + 1][x]);
			if (t < 0 && u < 0) sb_putc(&b, ' ');
			else if (t == u) sb_printf(&b, "%s█" RST, fg(t));
			else if (t < 0) sb_printf(&b, "%s▄" RST, fg(u));
			else if (u < 0) sb_printf(&b, "%s▀" RST, fg(t));
			else sb_printf(&b, "%s%s▀" RST, fg(t), bg(u));
		}
		snprintf(out[y], 1024, "%s", sb_str(&b));
		sb_free(&b);
	}
}

static const char * pet_bubble(int mood, long long f) {
	if (g_pet_say[0] && now_ms() < g_pet_say_until) return g_pet_say;
	switch (mood) {
		case PM_WORK:  return "sorting…";
		case PM_FOUND: return "sorted ✓";
		case PM_MISS:  return "oops";
		case PM_SLEEP: return f % 6 < 3 ? "z" : "z Z";
	}
	return NULL;
}

/* the strip above the prompt: speech bubble left of Bitsy, `bottom` on the last row */
static void pet_strip(LINES * out, const char * bottom) {
	long long f = now_ms() / 200;
	int mood = pet_mood(), lw = g_cols - 1 - PET_W - 1;
	char sprite[PET_ROWS][1024];
	pet_lines(sprite, mood, f);
	const char * say = pet_bubble(mood, f);
	int sw = say ? vis_width(say) : 0;
	SB line = {0};
	for (int y = 0; y < PET_ROWS; y++) {
		int used = 0;
		if (y == PET_ROWS - 1 && bottom) {
			used = sb_put_clip(&line, bottom, lw);
			sb_puts(&line, RST);
		} else if (say && y >= 1 && y <= 3 && sw + 4 <= lw) {
			sb_pad(&line, lw - sw - 4);
			if (y == 2) sb_printf(&line, "%s│ %s%s %s│", fg(C_FAINT), fg(C_TEXT), say, fg(C_FAINT));
			else {
				sb_printf(&line, "%s%s", fg(C_FAINT), y == 1 ? "╭" : "╰");
				sb_rep(&line, "─", sw + 2);
				sb_puts(&line, y == 1 ? "╮" : "╯");
			}
			sb_puts(&line, RST);
			used = lw;
		}
		sb_pad(&line, lw - used + 1);
		sb_puts(&line, sprite[y]);
		lines_take(out, &line);
	}
	sb_free(&line);
}

/* true when the strip looks different from the last time it was checked */
static bool pet_dirty(void) {
	if (!pet_fits()) return false;
	LINES L = {0};
	pet_strip(&L, NULL);
	SB all = {0};
	for (int i = 0; i < L.n; i++) sb_puts(&all, L.v[i]);
	bool changed = !g_pet_drawn || strcmp(g_pet_drawn, sb_str(&all)) != 0;
	free(g_pet_drawn);
	g_pet_drawn = xstrdup(sb_str(&all));
	sb_free(&all);
	lines_free(&L);
	return changed;
}

/*---- the companion hooks of ui.h ---------*/
static int companion_rows(void) { return pet_extra_rows(); }

static void companion_strip(LINES * out, const char * line) {
	if (pet_fits()) pet_strip(out, line);
	else lines_add(out, line);
}

static void companion_event(int what) {
	switch (what) {
		case CE_KEY:   pet_wake(); break;
		case CE_START: pet_set(PM_WORK, NULL, 0); break;
		case CE_OK:    pet_set(PM_FOUND, NULL, 4000); break;
		case CE_ERR:   if (g_pet_mood != PM_MISS) pet_set(PM_MISS, NULL, 2500); break;
	}
}

#endif
