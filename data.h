/*
 *	DATA.H
 *	------
 *	Random numbers (seeded, reproducible) and sample generators
 *	Part of Sort-CLI, coded by Trinh D.D. Nguyen, MIT licensed
 */
#ifndef SORTCLI_DATA_H
#define SORTCLI_DATA_H

#include <math.h>

/*---- RANDOM NUMBERS (xorshift64) ---------*/
static unsigned long long g_rng = 88172645463325252ULL;
static unsigned long g_seed = 1;		/* seed behind the current samples */

static void rng_seed(unsigned long long s) { g_rng = s * 2654435761ULL + 0x9E3779B97F4A7C15ULL; if (!g_rng) g_rng = 1; }

static unsigned rng_next(void) {
	g_rng ^= g_rng << 13;
	g_rng ^= g_rng >> 7;
	g_rng ^= g_rng << 17;
	return (unsigned)(g_rng >> 32);
}

static int rng_int(int n) { return n <= 1 ? 0 : (int)(rng_next() % (unsigned)n); }	/* 0..n-1 */

/* a fresh seed for "new samples", short enough to type back */
static unsigned long new_seed(void) { return 1 + rng_next() % 99999; }

/*---- SAMPLES -----------------------------*/
typedef struct { int v, id; } EL;		/* value, and position before sorting */

#define MAX_CELLS	2048
#define MAX_1D		512

enum { K_RANDOM, K_SORTED, K_REVERSED, K_NEARLY, K_FEW, K_DUPS, K_RUNS, K_SAWTOOTH, K_ORGAN, K_CUSTOM, K_KINDS };
static const char * KIND_NAMES[K_KINDS] = {
	"random", "sorted", "reversed", "nearly", "few", "dups", "runs", "sawtooth", "organ", "custom" };
static const char * KIND_DESC[K_KINDS] = {
	"a shuffled permutation of 1..n",
	"already in order",
	"in reverse order",
	"sorted with a few elements out of place",
	"only a handful of distinct values",
	"random values with many duplicates",
	"a few ascending runs, TimSort's favourite",
	"repeated ascending ramps",
	"rises then falls, like organ pipes",
	"your own values",
};

typedef struct {
	EL a[MAX_CELLS];
	int n;
	int kind;
} SAMPLES;

static void shuffle_vals(int * v, int n) {
	for (int i = n - 1; i > 0; i--) {
		int j = rng_int(i + 1), t = v[i];
		v[i] = v[j]; v[j] = t;
	}
}

static int cmp_int(const void * x, const void * y) {
	int a = *(const int *)x, b = *(const int *)y;
	return (a > b) - (a < b);
}

/* fill s with n values of the given kind, from seed */
static void samples_generate(SAMPLES * s, int kind, int n, unsigned long seed) {
	static int v[MAX_CELLS];
	if (n < 1) n = 1;
	if (n > MAX_CELLS) n = MAX_CELLS;
	rng_seed(seed);
	for (int i = 0; i < n; i++) v[i] = i + 1;
	switch (kind) {
	case K_SORTED: break;
	case K_REVERSED: for (int i = 0; i < n; i++) v[i] = n - i; break;
	case K_NEARLY: {
		int k = n / 16 + 1;
		for (int t = 0; t < k; t++) {
			int i = rng_int(n), j = i + 1 + rng_int(3);
			if (j < n) { int x = v[i]; v[i] = v[j]; v[j] = x; }
		}
		break;
	}
	case K_FEW: {
		int k = 3 + rng_int(4);
		for (int i = 0; i < n; i++) v[i] = (rng_int(k) + 1) * n / k;
		break;
	}
	case K_DUPS: {
		int m = n / 3 > 2 ? n / 3 : 2;
		for (int i = 0; i < n; i++) v[i] = (rng_int(m) + 1) * n / m;
		break;
	}
	case K_RUNS: {
		shuffle_vals(v, n);
		int runs = 2 + rng_int(4), start = 0;
		for (int r = 0; r < runs && start < n; r++) {
			int len = r == runs - 1 ? n - start : (n - start) / (runs - r) + rng_int(3) - 1;
			if (len < 1) len = 1;
			if (start + len > n) len = n - start;
			qsort(v + start, (size_t)len, sizeof(int), cmp_int);
			start += len;
		}
		break;
	}
	case K_SAWTOOTH: {
		int m = n / 4 > 2 ? n / 4 : 2;
		for (int i = 0; i < n; i++) v[i] = (i % m + 1) * n / m;
		break;
	}
	case K_ORGAN:
		for (int i = 0; i < n; i++) v[i] = i < (n + 1) / 2 ? 2 * i + 1 : 2 * (n - i);
		break;
	default:
		kind = K_RANDOM;
		shuffle_vals(v, n);
	}
	s->n = n;
	s->kind = kind;
	for (int i = 0; i < n; i++) { s->a[i].v = v[i] < 1 ? 1 : v[i]; s->a[i].id = i; }
}

static void samples_minmax(const EL * a, int n, int * mn, int * mx) {
	*mn = *mx = n ? a[0].v : 0;
	for (int i = 1; i < n; i++) {
		if (a[i].v < *mn) *mn = a[i].v;
		if (a[i].v > *mx) *mx = a[i].v;
	}
}

static bool has_duplicates(const EL * a, int n) {
	static unsigned char seen[MAX_CELLS + 1];
	memset(seen, 0, sizeof seen);
	for (int i = 0; i < n; i++) {
		int v = a[i].v;
		if (v < 0 || v > MAX_CELLS) return true;		/* custom values: assume possible */
		if (seen[v]) return true;
		seen[v] = 1;
	}
	return false;
}

static int digits(int v) {
	int d = v < 0 ? 2 : 1;
	if (v < 0) v = -v;
	while (v >= 10) { v /= 10; d++; }
	return d;
}

#endif
