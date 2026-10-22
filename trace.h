/*
 *	TRACE.H
 *	-------
 *	Sorting algorithms never draw. They call the helpers below, which do the
 *	work on a private copy and record compact events. A LANE replays those
 *	events, with checkpoints so that any step, forwards or backwards, is cheap.
 *	Part of Sort-CLI, coded by Trinh D.D. Nguyen, MIT licensed
 */
#ifndef SORTCLI_TRACE_H
#define SORTCLI_TRACE_H

#include <stdint.h>

enum { OP_CMP = 1, OP_SWAP, OP_WRITE, OP_DONE, OP_FOCUS, OP_PIVOT, OP_NOTE };
#define NIL			0xFFFF
#define EV_CAP		4000000		/* events per trace; longer runs are refused */
#define NOTE_CAP	60000

typedef struct {
	uint8_t op;
	uint16_t a, b;				/* indices (NIL = none); NOTE: a is the note number */
	int16_t v, id;				/* the element a WRITE stores */
} EV;

typedef struct {
	EV * ev;
	int n, cap;
	bool overflow;
	char ** notes;
	int nnotes, cnotes;
	int len;					/* elements */
	EL * start, * final;
	long cmps, swaps, writes, reads;
} TRACE;

typedef struct {
	EL * a;
	int n;
	TRACE * T;
	int w, h;					/* matrix shape, for 2D-only algorithms */
	long cmps, swaps, writes, reads;
} SCTX;

static void trace_free(TRACE * T) {
	free(T->ev);
	for (int i = 0; i < T->nnotes; i++) free(T->notes[i]);
	free(T->notes);
	free(T->start);
	free(T->final);
	memset(T, 0, sizeof *T);
}

static void rec(SCTX * c, int op, int a, int b, EL e) {
	TRACE * T = c->T;
	if (T->overflow) return;
	if (T->n == T->cap) {
		if (T->cap >= EV_CAP) { T->overflow = true; return; }
		T->cap = T->cap ? T->cap * 2 : 1024;
		if (T->cap > EV_CAP) T->cap = EV_CAP;
		T->ev = xrealloc(T->ev, sizeof(EV) * (size_t)T->cap);
	}
	EV * x = &T->ev[T->n++];
	x->op = (uint8_t)op;
	x->a = (uint16_t)(a < 0 ? NIL : a);
	x->b = (uint16_t)(b < 0 ? NIL : b);
	x->v = (int16_t)e.v;
	x->id = (int16_t)e.id;
}

static const EL NOEL = { 0, 0 };

static int sgn(int x) { return (x > 0) - (x < 0); }

/*---- what algorithms may do --------------*/
/* compare a[i] with a[j]: <0, 0, >0 */
static int cmpi(SCTX * c, int i, int j) {
	rec(c, OP_CMP, i, j, NOEL);
	c->cmps++;
	c->reads += 2;
	return sgn(c->a[i].v - c->a[j].v);
}

/* compare a value held aside (key) with a[j] */
static int cmpk(SCTX * c, EL key, int j) {
	rec(c, OP_CMP, j, -1, NOEL);
	c->cmps++;
	c->reads++;
	return sgn(key.v - c->a[j].v);
}

/* compare two values held aside; hint is an index to light up, or -1 */
static int cmpe(SCTX * c, EL x, EL y, int hint) {
	rec(c, OP_CMP, hint, -1, NOEL);
	c->cmps++;
	return sgn(x.v - y.v);
}

static void swp(SCTX * c, int i, int j) {
	if (i == j) return;
	rec(c, OP_SWAP, i, j, NOEL);
	EL t = c->a[i]; c->a[i] = c->a[j]; c->a[j] = t;
	c->swaps++;
	c->reads += 2;			/* its two writes are counted as part of the swap */
}

static EL get(SCTX * c, int i) { c->reads++; return c->a[i]; }

static void put(SCTX * c, int i, EL e) {
	rec(c, OP_WRITE, i, -1, e);
	c->a[i] = e;
	c->writes++;
}

/* copy cnt elements from src to dst inside the array, overlap allowed */
static void move(SCTX * c, int dst, int src, int cnt) {
	if (dst < src) for (int i = 0; i < cnt; i++) put(c, dst + i, get(c, src + i));
	else for (int i = cnt - 1; i >= 0; i--) put(c, dst + i, get(c, src + i));
}

static void done(SCTX * c, int lo, int hi)  { if (lo <= hi) rec(c, OP_DONE, lo, hi, NOEL); }
static void focus(SCTX * c, int lo, int hi) { rec(c, OP_FOCUS, lo <= hi ? lo : -1, lo <= hi ? hi : -1, NOEL); }
static void pivot(SCTX * c, int i)          { rec(c, OP_PIVOT, i, -1, NOEL); }

static void note(SCTX * c, const char * fmt, ...) {
	TRACE * T = c->T;
	if (T->overflow || T->nnotes >= NOTE_CAP) return;
	if (T->nnotes == T->cnotes) {
		T->cnotes = T->cnotes ? T->cnotes * 2 : 32;
		T->notes = xrealloc(T->notes, sizeof(char *) * (size_t)T->cnotes);
	}
	SB b = {0};
	va_list ap;
	va_start(ap, fmt);
	sb_vprintf(&b, fmt, ap);
	va_end(ap);
	T->notes[T->nnotes] = xstrdup(sb_str(&b));
	sb_free(&b);
	rec(c, OP_NOTE, T->nnotes++, -1, NOEL);
}

/*---- LANE: replaying a trace -------------*/
typedef struct {
	int lo, hi, pivot, note;
	long cmps, swaps, writes;
} META;

typedef struct {
	const TRACE * T;
	EL * a;
	unsigned char * dn;			/* elements known to be in their final place */
	int pos;					/* events applied */
	META m;
	int every, nck;				/* checkpoint spacing and count */
	EL * ck_a;
	unsigned char * ck_d;
	META * ck_m;
} LANE;

static void lane_apply(LANE * L, const EV * e) {
	switch (e->op) {
	case OP_CMP:   L->m.cmps++; break;
	case OP_SWAP: {
		EL t = L->a[e->a]; L->a[e->a] = L->a[e->b]; L->a[e->b] = t;
		L->m.swaps++;
		break;
	}
	case OP_WRITE: L->a[e->a].v = e->v; L->a[e->a].id = e->id; L->m.writes++; break;
	case OP_DONE:  memset(L->dn + e->a, 1, (size_t)(e->b - e->a + 1)); break;
	case OP_FOCUS: L->m.lo = e->a == NIL ? -1 : e->a; L->m.hi = e->b == NIL ? -1 : e->b; break;
	case OP_PIVOT: L->m.pivot = e->a == NIL ? -1 : e->a; break;
	case OP_NOTE:  L->m.note = e->a; break;
	}
}

static void lane_reset(LANE * L) {
	int n = L->T->len;
	memcpy(L->a, L->T->start, sizeof(EL) * (size_t)n);
	memset(L->dn, 0, (size_t)n);
	L->m = (META){ -1, -1, -1, -1, 0, 0, 0 };
	L->pos = 0;
}

static void lane_save(LANE * L, int k) {
	int n = L->T->len;
	memcpy(L->ck_a + (size_t)k * n, L->a, sizeof(EL) * (size_t)n);
	memcpy(L->ck_d + (size_t)k * n, L->dn, (size_t)n);
	L->ck_m[k] = L->m;
}

static void lane_load(LANE * L, int k) {
	int n = L->T->len;
	memcpy(L->a, L->ck_a + (size_t)k * n, sizeof(EL) * (size_t)n);
	memcpy(L->dn, L->ck_d + (size_t)k * n, (size_t)n);
	L->m = L->ck_m[k];
	L->pos = k * L->every;
}

/* budget: checkpoints of all lanes together stay near 24 MB */
static void lane_init(LANE * L, const TRACE * T, int lanes) {
	memset(L, 0, sizeof *L);
	L->T = T;
	int n = T->len > 0 ? T->len : 1;
	L->a = xrealloc(NULL, sizeof(EL) * (size_t)n);
	L->dn = xrealloc(NULL, (size_t)n);
	double budget = 24.0 * 1024 * 1024 / (lanes > 0 ? lanes : 1);
	double per = (double)n * (sizeof(EL) + 1) + sizeof(META);
	long every = (long)ceil((double)T->n * per / budget);
	L->every = every < 256 ? 256 : (int)every;
	L->nck = T->n / L->every + 1;
	L->ck_a = xrealloc(NULL, sizeof(EL) * (size_t)n * (size_t)L->nck);
	L->ck_d = xrealloc(NULL, (size_t)n * (size_t)L->nck);
	L->ck_m = xrealloc(NULL, sizeof(META) * (size_t)L->nck);
	lane_reset(L);
	lane_save(L, 0);
	for (int i = 0; i < T->n; i++) {
		lane_apply(L, &T->ev[i]);
		L->pos = i + 1;
		if (L->pos % L->every == 0) lane_save(L, L->pos / L->every);
	}
	lane_load(L, 0);
}

static void lane_free(LANE * L) {
	free(L->a); free(L->dn); free(L->ck_a); free(L->ck_d); free(L->ck_m);
	memset(L, 0, sizeof *L);
}

/* state after the first k events */
static void lane_seek(LANE * L, int k) {
	if (k < 0) k = 0;
	if (k > L->T->n) k = L->T->n;
	if (k < L->pos || k - L->pos > L->every) lane_load(L, k / L->every);
	while (L->pos < k) lane_apply(L, &L->T->ev[L->pos++]);
}

/* the latest compare and the latest swap/write among the last few events */
static void lane_highlights(const LANE * L, int * c1, int * c2, int * w1, int * w2) {
	*c1 = *c2 = *w1 = *w2 = -1;
	bool gotc = false, gotw = false;
	for (int i = L->pos - 1; i >= 0 && i >= L->pos - 6 && !(gotc && gotw); i--) {
		const EV * e = &L->T->ev[i];
		if (e->op == OP_CMP && !gotc) {
			gotc = true;
			*c1 = e->a == NIL ? -1 : e->a;
			*c2 = e->b == NIL ? -1 : e->b;
		} else if ((e->op == OP_SWAP || e->op == OP_WRITE) && !gotw) {
			gotw = true;
			*w1 = e->a;
			*w2 = e->op == OP_SWAP ? e->b : -1;
		}
	}
}

#endif
