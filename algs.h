/*
 *	ALGS.H
 *	------
 *	The sorting algorithms, written against the helpers in trace.h
 *	(cmpi/cmpk/cmpe compare, swp/put/move change the array, get reads,
 *	done/focus/pivot/note annotate), and their registry
 *	Part of Sort-CLI, coded by Trinh D.D. Nguyen, MIT licensed
 */
#ifndef SORTCLI_ALGS_H
#define SORTCLI_ALGS_H

static EL * tmp_alloc(int n) { return xrealloc(NULL, sizeof(EL) * (size_t)(n > 0 ? n : 1)); }

/* compare-and-swap: put a[i], a[j] in ascending (up) or descending order */
static void cswap(SCTX * c, int i, int j, bool up) {
	int s = cmpi(c, i, j);
	if (up ? s > 0 : s < 0) swp(c, i, j);
}

/*==== EXCHANGE SORTS ===========================*/
static void a_bubble(SCTX * c) {
	for (int end = c->n - 1; end > 0; end--) {
		bool swapped = false;
		focus(c, 0, end);
		note(c, "Pass %d: carry the largest value of a[0..%d] to the end", c->n - end, end);
		for (int i = 0; i < end; i++)
			if (cmpi(c, i, i + 1) > 0) { swp(c, i, i + 1); swapped = true; }
		done(c, end, end);
		if (!swapped) { note(c, "No swaps in this pass: the rest is already sorted"); return; }
	}
}

static void a_cocktail(SCTX * c) {
	int lo = 0, hi = c->n - 1;
	while (lo < hi) {
		bool swapped = false;
		focus(c, lo, hi);
		note(c, "Forward pass over a[%d..%d]", lo, hi);
		for (int i = lo; i < hi; i++)
			if (cmpi(c, i, i + 1) > 0) { swp(c, i, i + 1); swapped = true; }
		done(c, hi, hi);
		hi--;
		if (!swapped) break;
		swapped = false;
		note(c, "Backward pass over a[%d..%d]", lo, hi);
		for (int i = hi; i > lo; i--)
			if (cmpi(c, i - 1, i) > 0) { swp(c, i - 1, i); swapped = true; }
		done(c, lo, lo);
		lo++;
		if (!swapped) break;
	}
}

static void a_oddeven(SCTX * c) {
	bool sorted = false;
	for (int pass = 1; !sorted; pass++) {
		sorted = true;
		note(c, "Round %d: compare odd pairs, then even pairs", pass);
		for (int start = 1; start >= 0; start--)
			for (int i = start; i + 1 < c->n; i += 2)
				if (cmpi(c, i, i + 1) > 0) { swp(c, i, i + 1); sorted = false; }
	}
}

static void a_comb(SCTX * c) {
	int gap = c->n;
	bool swapped = true;
	while (gap > 1 || swapped) {
		gap = gap * 10 / 13;
		if (gap < 1) gap = 1;
		swapped = false;
		note(c, "Gap %d", gap);
		for (int i = 0; i + gap < c->n; i++)
			if (cmpi(c, i, i + gap) > 0) { swp(c, i, i + gap); swapped = true; }
	}
}

static void a_gnome(SCTX * c) {
	int pos = 0;
	note(c, "Step forward while in order, swap back while not");
	while (pos < c->n) {
		if (pos == 0 || cmpi(c, pos - 1, pos) <= 0) pos++;
		else { swp(c, pos, pos - 1); pos--; }
	}
}

static void a_exchange(SCTX * c) {
	for (int i = 0; i < c->n - 1; i++) {
		focus(c, i, c->n - 1);
		note(c, "Swap every smaller value into a[%d]", i);
		for (int j = i + 1; j < c->n; j++)
			if (cmpi(c, i, j) > 0) swp(c, i, j);
		done(c, i, i);
	}
}

static bool circle_pass(SCTX * c, int lo, int hi) {
	if (lo >= hi) return false;
	bool swapped = false;
	int l = lo, h = hi;
	focus(c, lo, hi);
	while (l < h) {
		if (cmpi(c, l, h) > 0) { swp(c, l, h); swapped = true; }
		l++; h--;
	}
	if (l == h && h + 1 <= hi && cmpi(c, l, h + 1) > 0) { swp(c, l, h + 1); swapped = true; }
	int mid = (hi - lo) / 2;
	bool a = circle_pass(c, lo, lo + mid);
	bool b = circle_pass(c, lo + mid + 1, hi);
	return swapped || a || b;
}

static void a_circle(SCTX * c) {
	int round = 1;
	do note(c, "Round %d: compare mirrored pairs, then each half", round++);
	while (circle_pass(c, 0, c->n - 1));
}

/*==== QUICK SORTS ==============================*/
static int lomuto(SCTX * c, int lo, int hi) {
	pivot(c, hi);
	int i = lo;
	for (int j = lo; j < hi; j++)
		if (cmpi(c, j, hi) < 0) { swp(c, i, j); i++; }
	swp(c, i, hi);
	pivot(c, i);
	return i;
}

static void quick_lomuto(SCTX * c, int lo, int hi) {
	if (lo >= hi) { if (lo == hi) done(c, lo, lo); return; }
	focus(c, lo, hi);
	note(c, "Partition a[%d..%d] around the last value", lo, hi);
	int p = lomuto(c, lo, hi);
	done(c, p, p);
	quick_lomuto(c, lo, p - 1);
	quick_lomuto(c, p + 1, hi);
}

static void a_quick(SCTX * c) { quick_lomuto(c, 0, c->n - 1); pivot(c, -1); }

static void quick_hoare(SCTX * c, int lo, int hi) {
	if (lo >= hi) return;
	focus(c, lo, hi);
	int mid = lo + (hi - lo) / 2;
	EL pv = get(c, mid);
	pivot(c, mid);
	note(c, "Partition a[%d..%d] around the middle value %d", lo, hi, pv.v);
	int i = lo - 1, j = hi + 1;
	while (1) {
		do i++; while (cmpk(c, pv, i) > 0);
		do j--; while (cmpk(c, pv, j) < 0);
		if (i >= j) break;
		swp(c, i, j);
	}
	quick_hoare(c, lo, j);
	quick_hoare(c, j + 1, hi);
}

static void a_hoare(SCTX * c) { quick_hoare(c, 0, c->n - 1); pivot(c, -1); }

static void quick3(SCTX * c, int lo, int hi) {
	if (hi <= lo) { if (hi == lo) done(c, lo, lo); return; }
	focus(c, lo, hi);
	EL v = get(c, lo);
	note(c, "Split a[%d..%d] into < %d, = %d and > %d", lo, hi, v.v, v.v, v.v);
	int lt = lo, gt = hi, i = lo + 1;
	pivot(c, lo);
	while (i <= gt) {
		int s = cmpk(c, v, i);
		if (s > 0) swp(c, lt++, i++);
		else if (s < 0) swp(c, i, gt--);
		else i++;
	}
	done(c, lt, gt);
	quick3(c, lo, lt - 1);
	quick3(c, gt + 1, hi);
}

static void a_quick3(SCTX * c) { quick3(c, 0, c->n - 1); pivot(c, -1); }

static void dual_pivot(SCTX * c, int lo, int hi) {
	if (hi <= lo) { if (hi == lo) done(c, lo, lo); return; }
	focus(c, lo, hi);
	if (cmpi(c, lo, hi) > 0) swp(c, lo, hi);
	note(c, "Two pivots a[%d] = %d and a[%d] = %d make three parts", lo, c->a[lo].v, hi, c->a[hi].v);
	int l = lo + 1, g = hi - 1, k = l;
	while (k <= g) {
		if (cmpi(c, k, lo) < 0) { swp(c, k, l); l++; }
		else if (cmpi(c, k, hi) >= 0) {
			while (k < g && cmpi(c, g, hi) > 0) g--;
			swp(c, k, g);
			g--;
			if (cmpi(c, k, lo) < 0) { swp(c, k, l); l++; }
		}
		k++;
	}
	l--; g++;
	swp(c, lo, l);
	swp(c, hi, g);
	done(c, l, l);
	done(c, g, g);
	dual_pivot(c, lo, l - 1);
	dual_pivot(c, l + 1, g - 1);
	dual_pivot(c, g + 1, hi);
}

static void a_dualpivot(SCTX * c) { dual_pivot(c, 0, c->n - 1); }

/*==== SELECTION SORTS ==========================*/
static void a_selection(SCTX * c) {
	for (int i = 0; i < c->n - 1; i++) {
		int m = i;
		focus(c, i, c->n - 1);
		note(c, "Find the smallest of a[%d..%d]", i, c->n - 1);
		for (int j = i + 1; j < c->n; j++)
			if (cmpi(c, j, m) < 0) { m = j; pivot(c, m); }
		swp(c, i, m);
		pivot(c, -1);
		done(c, i, i);
	}
}

static void a_dselection(SCTX * c) {
	int lo = 0, hi = c->n - 1;
	while (lo < hi) {
		int mn = lo, mx = lo;
		focus(c, lo, hi);
		note(c, "Find both the smallest and the largest of a[%d..%d]", lo, hi);
		for (int i = lo + 1; i <= hi; i++) {
			if (cmpi(c, i, mn) < 0) mn = i;
			if (cmpi(c, i, mx) > 0) mx = i;
		}
		swp(c, lo, mn);
		if (mx == lo) mx = mn;
		swp(c, hi, mx);
		done(c, lo, lo);
		done(c, hi, hi);
		lo++; hi--;
	}
}

static void sift_down(SCTX * c, int base, int i, int size) {
	while (1) {
		int l = 2 * i + 1, r = l + 1, m = i;
		if (l < size && cmpi(c, base + l, base + m) > 0) m = l;
		if (r < size && cmpi(c, base + r, base + m) > 0) m = r;
		if (m == i) return;
		swp(c, base + i, base + m);
		i = m;
	}
}

static void heap_range(SCTX * c, int lo, int hi, bool mark) {
	int size = hi - lo + 1;
	note(c, "Build a max-heap in a[%d..%d]", lo, hi);
	for (int i = size / 2 - 1; i >= 0; i--) sift_down(c, lo, i, size);
	note(c, "Move the top of the heap to the end, then repair the heap");
	for (int end = size - 1; end > 0; end--) {
		focus(c, lo, lo + end);
		swp(c, lo, lo + end);
		if (mark) done(c, lo + end, lo + end);
		sift_down(c, lo, 0, end);
	}
}

static void a_heap(SCTX * c) { if (c->n > 1) heap_range(c, 0, c->n - 1, true); }

/* smoothsort: a forest of Leonardo heaps whose roots stay in ascending order */
static const int LEO[] = { 1, 1, 3, 5, 9, 15, 25, 41, 67, 109, 177, 287, 465, 753, 1219, 1973, 3193, 5167 };

static void leo_sift(SCTX * c, int root, int k) {
	while (k >= 2) {
		int r = root - 1, l = root - 1 - LEO[k - 2], ch, ck;
		if (cmpi(c, l, r) >= 0) { ch = l; ck = k - 1; } else { ch = r; ck = k - 2; }
		if (cmpi(c, root, ch) >= 0) return;
		swp(c, root, ch);
		root = ch;
		k = ck;
	}
}

static void leo_rectify(SCTX * c, const int * ord, const int * pos, int j) {
	while (j > 0) {
		int cur = pos[j], prev = pos[j - 1];
		if (cmpi(c, prev, cur) <= 0) break;
		if (ord[j] >= 2) {
			int r = cur - 1, l = cur - 1 - LEO[ord[j] - 2];
			if (cmpi(c, prev, l) <= 0 || cmpi(c, prev, r) <= 0) break;
		}
		swp(c, prev, cur);
		j--;
	}
	leo_sift(c, pos[j], ord[j]);
}

static void a_smooth(SCTX * c) {
	int ord[64], pos[64], s = 0;
	note(c, "Grow a forest of Leonardo heaps (sizes 1, 1, 3, 5, 9, 15, …)");
	for (int i = 0; i < c->n; i++) {
		if (s >= 2 && ord[s - 2] == ord[s - 1] + 1) { s--; ord[s - 1]++; pos[s - 1] = i; }
		else if (s >= 1 && ord[s - 1] == 1) { ord[s] = 0; pos[s] = i; s++; }
		else { ord[s] = 1; pos[s] = i; s++; }
		focus(c, 0, i);
		leo_rectify(c, ord, pos, s - 1);
	}
	note(c, "Take the largest root off the right end, re-balance the roots");
	for (int i = c->n - 1; i >= 0; i--) {
		int k = ord[--s];
		done(c, i, i);
		focus(c, 0, i - 1);
		if (k >= 2) {
			ord[s] = k - 1; pos[s] = i - 1 - LEO[k - 2]; s++;
			ord[s] = k - 2; pos[s] = i - 1; s++;
			leo_rectify(c, ord, pos, s - 2);
			leo_rectify(c, ord, pos, s - 1);
		}
	}
}

static void a_cycle(SCTX * c) {
	int n = c->n;
	for (int cs = 0; cs < n - 1; cs++) {
		EL item = get(c, cs);
		focus(c, cs, n - 1);
		int pos = cs;
		for (int i = cs + 1; i < n; i++) if (cmpk(c, item, i) > 0) pos++;
		if (pos == cs) { done(c, cs, cs); continue; }
		note(c, "Value %d belongs at a[%d]: write it there and carry the one it displaces", item.v, pos);
		while (cmpk(c, item, pos) == 0) pos++;
		EL t = get(c, pos); put(c, pos, item); item = t;
		done(c, pos, pos);
		while (pos != cs) {
			pos = cs;
			for (int i = cs + 1; i < n; i++) if (cmpk(c, item, i) > 0) pos++;
			while (pos != cs && cmpk(c, item, pos) == 0) pos++;
			t = get(c, pos); put(c, pos, item); item = t;
			done(c, pos, pos);
		}
	}
}

static void flip(SCTX * c, int hi) { for (int i = 0; i < hi; i++, hi--) swp(c, i, hi); }

static void a_pancake(SCTX * c) {
	for (int size = c->n; size > 1; size--) {
		int m = 0;
		focus(c, 0, size - 1);
		for (int i = 1; i < size; i++) if (cmpi(c, i, m) > 0) m = i;
		if (m != size - 1) {
			note(c, "Flip a[0..%d] to bring the largest to the front, then flip a[0..%d]", m, size - 1);
			if (m > 0) flip(c, m);
			flip(c, size - 1);
		}
		done(c, size - 1, size - 1);
	}
}

/*==== INSERTION SORTS ==========================*/
static void insertion_range(SCTX * c, int lo, int hi) {
	for (int i = lo + 1; i <= hi; i++) {
		EL key = get(c, i);
		int j = i - 1;
		while (j >= lo && cmpk(c, key, j) < 0) { put(c, j + 1, get(c, j)); j--; }
		if (j + 1 != i) put(c, j + 1, key);
	}
}

static void a_insertion(SCTX * c) {
	note(c, "Take each value and slide it left into the sorted prefix");
	for (int i = 1; i < c->n; i++) {
		focus(c, 0, i);
		EL key = get(c, i);
		int j = i - 1;
		while (j >= 0 && cmpk(c, key, j) < 0) { put(c, j + 1, get(c, j)); j--; }
		if (j + 1 != i) put(c, j + 1, key);
	}
}

static void binary_insert(SCTX * c, int lo, int hi, int start) {
	for (int i = start; i < hi; i++) {
		EL key = get(c, i);
		int l = lo, r = i;
		while (l < r) {
			int m = l + (r - l) / 2;
			if (cmpk(c, key, m) < 0) r = m; else l = m + 1;
		}
		if (l != i) { move(c, l + 1, l, i - l); put(c, l, key); }
	}
}

static void a_binsertion(SCTX * c) {
	note(c, "Binary-search where each value goes, then shift the rest right");
	for (int i = 1; i < c->n; i++) {
		focus(c, 0, i);
		binary_insert(c, 0, i + 1, i);
	}
}

static void a_shell(SCTX * c) {
	int gaps[32], ng = 0;
	static const int CIURA[] = { 1, 4, 10, 23, 57, 132, 301, 701 };
	for (int i = 0; i < 8 && CIURA[i] < c->n; i++) gaps[ng++] = CIURA[i];
	while (ng && ng < 32 && gaps[ng - 1] * 9 / 4 < c->n) { gaps[ng] = gaps[ng - 1] * 9 / 4; ng++; }
	if (!ng) gaps[ng++] = 1;
	for (int g = ng - 1; g >= 0; g--) {
		int gap = gaps[g];
		note(c, "Insertion sort on every %d%s element", gap, gap == 1 ? "st" : "th");
		for (int i = gap; i < c->n; i++) {
			EL key = get(c, i);
			int j = i;
			while (j >= gap && cmpk(c, key, j - gap) < 0) { put(c, j, get(c, j - gap)); j -= gap; }
			if (j != i) put(c, j, key);
		}
	}
}

/*==== MERGE SORTS ==============================*/
static void merge_runs(SCTX * c, EL * tmp, int lo, int mid, int hi) {
	focus(c, lo, hi);
	note(c, "Merge a[%d..%d] with a[%d..%d]", lo, mid, mid + 1, hi);
	for (int i = lo; i <= hi; i++) tmp[i] = get(c, i);
	int i = lo, j = mid + 1, k = lo;
	while (i <= mid && j <= hi) {
		if (cmpe(c, tmp[j], tmp[i], k) < 0) put(c, k++, tmp[j++]);
		else put(c, k++, tmp[i++]);
	}
	while (i <= mid) put(c, k++, tmp[i++]);
	while (j <= hi) put(c, k++, tmp[j++]);
}

static void merge_td(SCTX * c, EL * tmp, int lo, int hi) {
	if (lo >= hi) return;
	int mid = lo + (hi - lo) / 2;
	merge_td(c, tmp, lo, mid);
	merge_td(c, tmp, mid + 1, hi);
	merge_runs(c, tmp, lo, mid, hi);
}

static void a_merge(SCTX * c) {
	EL * tmp = tmp_alloc(c->n);
	merge_td(c, tmp, 0, c->n - 1);
	free(tmp);
}

static void a_bmerge(SCTX * c) {
	EL * tmp = tmp_alloc(c->n);
	for (int w = 1; w < c->n; w *= 2)
		for (int lo = 0; lo + w < c->n; lo += 2 * w) {
			int hi = lo + 2 * w - 1 < c->n - 1 ? lo + 2 * w - 1 : c->n - 1;
			merge_runs(c, tmp, lo, lo + w - 1, hi);
		}
	free(tmp);
}

/*---- TIMSORT (a port of Java's java.util.TimSort) ----*/
#define TIM_MIN_MERGE	16		/* Java uses 32 and CPython 64: smaller shows runs on small arrays */
#define TIM_MIN_GALLOP	7

typedef struct {
	SCTX * c;
	EL * tmp;
	int min_gallop;
	int base[64], len[64], size;
} TIM;

/* key against arr[i], or against the main array when arr is NULL */
static int tim_cmp(TIM * t, EL key, const EL * arr, int i) {
	return arr ? cmpe(t->c, key, arr[i], -1) : cmpk(t->c, key, i);
}

/* first k in [0, len] with arr[base + k - 1] < key <= arr[base + k] */
static int gallop_left(TIM * t, EL key, const EL * arr, int base, int len, int hint) {
	int last = 0, ofs = 1;
	if (tim_cmp(t, key, arr, base + hint) > 0) {
		int max = len - hint;
		while (ofs < max && tim_cmp(t, key, arr, base + hint + ofs) > 0) { last = ofs; ofs = ofs * 2 + 1; }
		if (ofs > max) ofs = max;
		last += hint; ofs += hint;
	} else {
		int max = hint + 1;
		while (ofs < max && tim_cmp(t, key, arr, base + hint - ofs) <= 0) { last = ofs; ofs = ofs * 2 + 1; }
		if (ofs > max) ofs = max;
		int x = last; last = hint - ofs; ofs = hint - x;
	}
	last++;
	while (last < ofs) {
		int m = last + (ofs - last) / 2;
		if (tim_cmp(t, key, arr, base + m) > 0) last = m + 1; else ofs = m;
	}
	return ofs;
}

/* first k in [0, len] with arr[base + k - 1] <= key < arr[base + k] */
static int gallop_right(TIM * t, EL key, const EL * arr, int base, int len, int hint) {
	int last = 0, ofs = 1;
	if (tim_cmp(t, key, arr, base + hint) < 0) {
		int max = hint + 1;
		while (ofs < max && tim_cmp(t, key, arr, base + hint - ofs) < 0) { last = ofs; ofs = ofs * 2 + 1; }
		if (ofs > max) ofs = max;
		int x = last; last = hint - ofs; ofs = hint - x;
	} else {
		int max = len - hint;
		while (ofs < max && tim_cmp(t, key, arr, base + hint + ofs) >= 0) { last = ofs; ofs = ofs * 2 + 1; }
		if (ofs > max) ofs = max;
		last += hint; ofs += hint;
	}
	last++;
	while (last < ofs) {
		int m = last + (ofs - last) / 2;
		if (tim_cmp(t, key, arr, base + m) < 0) ofs = m; else last = m + 1;
	}
	return ofs;
}

static void tim_merge_lo(TIM * t, int base1, int len1, int base2, int len2) {
	SCTX * c = t->c;
	EL * tmp = t->tmp;
	for (int i = 0; i < len1; i++) tmp[i] = get(c, base1 + i);
	int c1 = 0, c2 = base2, dest = base1;
	put(c, dest++, get(c, c2++));
	if (--len2 == 0) { for (int i = 0; i < len1; i++) put(c, dest + i, tmp[c1 + i]); return; }
	if (len1 == 1) { move(c, dest, c2, len2); put(c, dest + len2, tmp[c1]); return; }
	int mg = t->min_gallop;
	while (1) {
		int n1 = 0, n2 = 0;			/* wins in a row */
		do {
			if (cmpk(c, tmp[c1], c2) > 0) {
				put(c, dest++, get(c, c2++)); n2++; n1 = 0;
				if (--len2 == 0) goto out;
			} else {
				put(c, dest++, tmp[c1++]); n1++; n2 = 0;
				if (--len1 == 1) goto out;
			}
		} while ((n1 | n2) < mg);
		note(c, "One run keeps winning: gallop (min_gallop %d)", mg);
		do {
			n1 = gallop_right(t, get(c, c2), tmp, c1, len1, 0);
			if (n1) {
				for (int i = 0; i < n1; i++) put(c, dest + i, tmp[c1 + i]);
				dest += n1; c1 += n1; len1 -= n1;
				if (len1 <= 1) goto out;
			}
			put(c, dest++, get(c, c2++));
			if (--len2 == 0) goto out;
			n2 = gallop_left(t, tmp[c1], NULL, c2, len2, 0);
			if (n2) {
				move(c, dest, c2, n2);
				dest += n2; c2 += n2; len2 -= n2;
				if (len2 == 0) goto out;
			}
			put(c, dest++, tmp[c1++]);
			if (--len1 == 1) goto out;
			mg--;
		} while (n1 >= TIM_MIN_GALLOP || n2 >= TIM_MIN_GALLOP);
		if (mg < 0) mg = 0;
		mg += 2;
	}
out:
	t->min_gallop = mg < 1 ? 1 : mg;
	if (len1 == 1) { move(c, dest, c2, len2); put(c, dest + len2, tmp[c1]); }
	else for (int i = 0; i < len1; i++) put(c, dest + i, tmp[c1 + i]);
}

static void tim_merge_hi(TIM * t, int base1, int len1, int base2, int len2) {
	SCTX * c = t->c;
	EL * tmp = t->tmp;
	for (int i = 0; i < len2; i++) tmp[i] = get(c, base2 + i);
	int c1 = base1 + len1 - 1, c2 = len2 - 1, dest = base2 + len2 - 1;
	put(c, dest--, get(c, c1--));
	if (--len1 == 0) { for (int i = 0; i < len2; i++) put(c, dest - (len2 - 1) + i, tmp[i]); return; }
	if (len2 == 1) { dest -= len1; c1 -= len1; move(c, dest + 1, c1 + 1, len1); put(c, dest, tmp[c2]); return; }
	int mg = t->min_gallop;
	while (1) {
		int n1 = 0, n2 = 0;
		do {
			if (cmpk(c, tmp[c2], c1) < 0) {
				put(c, dest--, get(c, c1--)); n1++; n2 = 0;
				if (--len1 == 0) goto out;
			} else {
				put(c, dest--, tmp[c2--]); n2++; n1 = 0;
				if (--len2 == 1) goto out;
			}
		} while ((n1 | n2) < mg);
		note(c, "One run keeps winning: gallop (min_gallop %d)", mg);
		do {
			n1 = len1 - gallop_right(t, tmp[c2], NULL, base1, len1, len1 - 1);
			if (n1) {
				dest -= n1; c1 -= n1; len1 -= n1;
				move(c, dest + 1, c1 + 1, n1);
				if (len1 == 0) goto out;
			}
			put(c, dest--, tmp[c2--]);
			if (--len2 == 1) goto out;
			n2 = len2 - gallop_left(t, get(c, c1), tmp, 0, len2, len2 - 1);
			if (n2) {
				dest -= n2; c2 -= n2; len2 -= n2;
				for (int i = 0; i < n2; i++) put(c, dest + 1 + i, tmp[c2 + 1 + i]);
				if (len2 <= 1) goto out;
			}
			put(c, dest--, get(c, c1--));
			if (--len1 == 0) goto out;
			mg--;
		} while (n1 >= TIM_MIN_GALLOP || n2 >= TIM_MIN_GALLOP);
		if (mg < 0) mg = 0;
		mg += 2;
	}
out:
	t->min_gallop = mg < 1 ? 1 : mg;
	if (len2 == 1) { dest -= len1; c1 -= len1; move(c, dest + 1, c1 + 1, len1); put(c, dest, tmp[c2]); }
	else for (int i = 0; i < len2; i++) put(c, dest - (len2 - 1) + i, tmp[i]);
}

static void tim_merge_at(TIM * t, int i) {
	SCTX * c = t->c;
	int base1 = t->base[i], len1 = t->len[i], base2 = t->base[i + 1], len2 = t->len[i + 1];
	t->len[i] = len1 + len2;
	if (i == t->size - 3) { t->base[i + 1] = t->base[i + 2]; t->len[i + 1] = t->len[i + 2]; }
	t->size--;
	focus(c, base1, base2 + len2 - 1);
	note(c, "Merge runs a[%d..%d] and a[%d..%d]", base1, base1 + len1 - 1, base2, base2 + len2 - 1);
	int k = gallop_right(t, get(c, base2), NULL, base1, len1, 0);
	base1 += k; len1 -= k;
	if (len1 == 0) return;
	len2 = gallop_left(t, get(c, base1 + len1 - 1), NULL, base2, len2, len2 - 1);
	if (len2 == 0) return;
	if (len1 <= len2) tim_merge_lo(t, base1, len1, base2, len2);
	else tim_merge_hi(t, base1, len1, base2, len2);
}

/* length of the run at lo, reversing it when it is strictly descending */
static int tim_count_run(SCTX * c, int lo, int hi) {
	int r = lo + 1;
	if (r == hi) return 1;
	if (cmpi(c, r++, lo) < 0) {
		while (r < hi && cmpi(c, r, r - 1) < 0) r++;
		for (int i = lo, j = r - 1; i < j; i++, j--) swp(c, i, j);
	} else {
		while (r < hi && cmpi(c, r, r - 1) >= 0) r++;
	}
	return r - lo;
}

static int tim_min_run(int n) {
	int r = 0;
	while (n >= TIM_MIN_MERGE) { r |= n & 1; n >>= 1; }
	return n + r;
}

static void a_timsort(SCTX * c) {
	int n = c->n, lo = 0, left = n;
	if (n < 2) return;
	TIM t = { c, tmp_alloc(n), TIM_MIN_GALLOP, {0}, {0}, 0 };
	int minrun = n < TIM_MIN_MERGE ? n : tim_min_run(n);
	note(c, "minrun = %d: find natural runs, extend short ones to minrun", minrun);
	do {
		int run = tim_count_run(c, lo, n);
		focus(c, lo, lo + run - 1);
		note(c, "Natural run a[%d..%d], %d long", lo, lo + run - 1, run);
		if (run < minrun) {
			int force = left <= minrun ? left : minrun;
			focus(c, lo, lo + force - 1);
			note(c, "Extend it to a[%d..%d] with binary insertion", lo, lo + force - 1);
			binary_insert(c, lo, lo + force, lo + run);
			run = force;
		}
		t.base[t.size] = lo; t.len[t.size] = run; t.size++;
		while (t.size > 1) {						/* keep the run stack balanced */
			int k = t.size - 2;
			if ((k > 0 && t.len[k - 1] <= t.len[k] + t.len[k + 1]) || (k > 1 && t.len[k - 2] <= t.len[k] + t.len[k - 1])) {
				if (t.len[k - 1] < t.len[k + 1]) k--;
			} else if (t.len[k] > t.len[k + 1]) break;
			tim_merge_at(&t, k);
		}
		lo += run;
		left -= run;
	} while (left);
	while (t.size > 1) {
		int k = t.size - 2;
		if (k > 0 && t.len[k - 1] < t.len[k + 1]) k--;
		tim_merge_at(&t, k);
	}
	free(t.tmp);
}

/*==== HYBRID ===================================*/
static void intro(SCTX * c, int lo, int hi, int depth) {
	while (hi - lo + 1 > 16) {
		if (depth == 0) {
			note(c, "Too deep: switch to heapsort for a[%d..%d]", lo, hi);
			heap_range(c, lo, hi, false);
			return;
		}
		depth--;
		focus(c, lo, hi);
		int mid = lo + (hi - lo) / 2;
		if (cmpi(c, mid, lo) < 0) swp(c, mid, lo);
		if (cmpi(c, hi, lo) < 0) swp(c, hi, lo);
		if (cmpi(c, hi, mid) < 0) swp(c, hi, mid);
		swp(c, mid, hi);
		note(c, "Median-of-three quicksort step on a[%d..%d]", lo, hi);
		int p = lomuto(c, lo, hi);
		intro(c, p + 1, hi, depth);
		hi = p - 1;
	}
}

static void a_intro(SCTX * c) {
	int depth = 0;
	for (int n = c->n; n > 1; n >>= 1) depth += 2;
	intro(c, 0, c->n - 1, depth);
	pivot(c, -1);
	focus(c, 0, c->n - 1);
	note(c, "Finish the small pieces with one insertion sort");
	insertion_range(c, 0, c->n - 1);
}

/*==== DISTRIBUTION SORTS =======================*/
static void a_counting(SCTX * c) {
	int n = c->n, mn, mx;
	samples_minmax(c->a, n, &mn, &mx);
	c->reads += n;
	int k = mx - mn + 1;
	int * cnt = xrealloc(NULL, sizeof(int) * (size_t)(k + 1));
	memset(cnt, 0, sizeof(int) * (size_t)(k + 1));
	EL * out = tmp_alloc(n);
	note(c, "Count each value %d..%d, then place them using running totals", mn, mx);
	for (int i = 0; i < n; i++) cnt[get(c, i).v - mn + 1]++;
	for (int i = 1; i <= k; i++) cnt[i] += cnt[i - 1];
	for (int i = 0; i < n; i++) { EL e = get(c, i); out[cnt[e.v - mn]++] = e; }
	note(c, "Write the values back in order");
	for (int i = 0; i < n; i++) put(c, i, out[i]);
	free(out);
	free(cnt);
}

static void radix_pass(SCTX * c, EL * out, int lo, int hi, int exp, int mn, int * bounds) {
	int cnt[11] = {0};
	for (int i = lo; i <= hi; i++) cnt[(get(c, i).v - mn) / exp % 10 + 1]++;
	for (int d = 1; d <= 10; d++) cnt[d] += cnt[d - 1];
	if (bounds) for (int d = 0; d <= 10; d++) bounds[d] = lo + cnt[d];
	for (int i = lo; i <= hi; i++) { EL e = get(c, i); out[lo + cnt[(e.v - mn) / exp % 10]++] = e; }
	for (int i = lo; i <= hi; i++) put(c, i, out[i]);
}

static void a_radix(SCTX * c) {
	int mn, mx;
	samples_minmax(c->a, c->n, &mn, &mx);
	EL * out = tmp_alloc(c->n);
	int exp = 1, d = 1;
	do {
		note(c, "Stable pass on digit %d (the %ds)", d++, exp);
		radix_pass(c, out, 0, c->n - 1, exp, mn, NULL);
		exp *= 10;
	} while ((mx - mn) / exp > 0);
	free(out);
}

static void msd(SCTX * c, EL * out, int lo, int hi, int exp, int mn) {
	if (hi <= lo || exp == 0) return;
	int b[11];
	focus(c, lo, hi);
	note(c, "Split a[%d..%d] by the %ds digit", lo, hi, exp);
	radix_pass(c, out, lo, hi, exp, mn, b);
	for (int d = 0; d < 10; d++) msd(c, out, b[d], b[d + 1] - 1, exp / 10, mn);
}

static void a_msd(SCTX * c) {
	int mn, mx, exp = 1;
	samples_minmax(c->a, c->n, &mn, &mx);
	while ((mx - mn) / exp >= 10) exp *= 10;
	EL * out = tmp_alloc(c->n);
	msd(c, out, 0, c->n - 1, exp, mn);
	free(out);
}

static void a_bucket(SCTX * c) {
	int n = c->n, mn, mx;
	samples_minmax(c->a, n, &mn, &mx);
	int k = (int)sqrt((double)n);
	if (k < 1) k = 1;
	int * start = xrealloc(NULL, sizeof(int) * (size_t)(k + 1));
	memset(start, 0, sizeof(int) * (size_t)(k + 1));
	EL * out = tmp_alloc(n);
	long range = (long)mx - mn + 1;
	note(c, "Drop the values into %d buckets by size", k);
	for (int i = 0; i < n; i++) start[(long)(get(c, i).v - mn) * k / range + 1]++;
	for (int i = 1; i <= k; i++) start[i] += start[i - 1];
	int * fill = xrealloc(NULL, sizeof(int) * (size_t)k);
	memcpy(fill, start, sizeof(int) * (size_t)k);
	for (int i = 0; i < n; i++) { EL e = get(c, i); out[fill[(long)(e.v - mn) * k / range]++] = e; }
	for (int i = 0; i < n; i++) put(c, i, out[i]);
	for (int b = 0; b < k; b++) {
		if (start[b + 1] - start[b] < 2) continue;
		focus(c, start[b], start[b + 1] - 1);
		note(c, "Insertion sort bucket %d: a[%d..%d]", b + 1, start[b], start[b + 1] - 1);
		insertion_range(c, start[b], start[b + 1] - 1);
	}
	free(fill); free(out); free(start);
}

/*==== SORTING NETWORKS =========================*/
static void bit_merge(SCTX * c, int lo, int n, bool up) {
	if (n <= 1) return;
	int m = 1;
	while (m < n) m <<= 1;
	m >>= 1;
	for (int i = lo; i < lo + n - m; i++) cswap(c, i, i + m, up);
	bit_merge(c, lo, m, up);
	bit_merge(c, lo + m, n - m, up);
}

static void bit_sort(SCTX * c, int lo, int n, bool up) {
	if (n <= 1) return;
	int m = n / 2;
	bit_sort(c, lo, m, !up);
	bit_sort(c, lo + m, n - m, up);
	focus(c, lo, lo + n - 1);
	note(c, "Bitonic merge a[%d..%d] %s", lo, lo + n - 1, up ? "upwards" : "downwards");
	bit_merge(c, lo, n, up);
}

static void a_bitonic(SCTX * c) { bit_sort(c, 0, c->n, true); }

static void a_batcher(SCTX * c) {
	int n = c->n;
	for (int p = 1; p < n; p <<= 1)
		for (int k = p; k >= 1; k >>= 1) {
			note(c, "Merge blocks of %d, comparators %d apart", p, k);
			for (int j = k % p; j + k < n; j += 2 * k)
				for (int i = 0; i < k && i + j + k < n; i++)
					if ((i + j) / (p * 2) == (i + j + k) / (p * 2)) cswap(c, i + j, i + j + k, true);
		}
}

/*==== SLOW AND SILLY ===========================*/
static void stooge(SCTX * c, int i, int j) {
	if (cmpi(c, i, j) > 0) swp(c, i, j);
	if (j - i + 1 > 2) {
		int t = (j - i + 1) / 3;
		focus(c, i, j);
		stooge(c, i, j - t);
		stooge(c, i + t, j);
		stooge(c, i, j - t);
	}
}

static void a_stooge(SCTX * c) { if (c->n > 1) { note(c, "Sort the first 2/3, the last 2/3, then the first 2/3 again"); stooge(c, 0, c->n - 1); } }

static void slow(SCTX * c, int i, int j) {
	if (i >= j) return;
	int m = (i + j) / 2;
	slow(c, i, m);
	slow(c, m + 1, j);
	if (cmpi(c, m, j) > 0) swp(c, m, j);
	slow(c, i, j - 1);
}

static void a_slowsort(SCTX * c) { note(c, "Multiply and surrender: find the maximum the slowest way"); slow(c, 0, c->n - 1); }

static void a_bogo(SCTX * c) {
	for (long round = 1;; round++) {
		bool sorted = true;
		for (int i = 0; i + 1 < c->n && sorted; i++) if (cmpi(c, i, i + 1) > 0) sorted = false;
		if (sorted) return;
		note(c, "Not sorted: shuffle (attempt %ld)", round);
		for (int i = c->n - 1; i > 0; i--) swp(c, i, rng_int(i + 1));
	}
}

/*==== 2D ======================================*/
static void transposition(SCTX * c, int start, int stride, int count, bool up) {
	for (int pass = 0; pass < count; pass++)
		for (int i = pass & 1; i + 1 < count; i += 2)
			cswap(c, start + i * stride, start + (i + 1) * stride, up);
}

static void a_shear(SCTX * c) {
	int w = c->w, h = c->h;
	if (w * h != c->n || h < 1) { w = c->n; h = 1; }
	int phases = 0;
	while ((1 << phases) < h) phases++;
	for (int ph = 0; ph <= phases; ph++) {
		note(c, "Phase %d: sort rows, alternating direction (snake order)", ph + 1);
		for (int y = 0; y < h; y++) { focus(c, y * w, y * w + w - 1); transposition(c, y * w, 1, w, !(y & 1)); }
		if (ph == phases || h == 1) break;
		note(c, "Phase %d: sort every column top to bottom", ph + 1);
		focus(c, 0, c->n - 1);
		for (int x = 0; x < w; x++) transposition(c, x, w, h, true);
	}
}

/*==== REGISTRY =================================*/
typedef struct {
	const char * key, * aliases, * name, * cat;
	void (*fn)(SCTX *);
	const char * best, * avg, * worst, * space;
	bool stable, inplace, only2d;
	int max_n;					/* 0: no limit */
	const char * verb, * summary, * hint;
	const char * pseudo[8];
} ALG;

static const char * CATS[] = { "exchange", "selection", "insertion", "merge", "hybrid", "distribution", "network", "slow", "2d" };
#define NCATS ((int)(sizeof CATS / sizeof CATS[0]))

static const ALG ALGS[] = {
	{ "bubble", " bubblesort ", "Bubble sort", "exchange", a_bubble, "n", "n²", "n²", "1", true, true, false, 0, "Bubbling",
	  "Sweeps left to right swapping neighbours that are out of order, so the largest value bubbles to the end each pass. Stops early when a pass makes no swaps.",
	  "Swaps equal the number of inversions; try `/data nearly` to see the early exit.",
	  { "repeat:", "  for i in 0 .. end-1:", "    if a[i] > a[i+1]: swap", "  end ← end − 1", "until no swaps" } },
	{ "cocktail", " shaker cocktailshaker bidirectional ", "Cocktail shaker sort", "exchange", a_cocktail, "n", "n²", "n²", "1", true, true, false, 0, "Shaking",
	  "Bubble sort in both directions: small values that sit near the end (turtles) move fast on the backward pass.",
	  "Compare with bubble sort on `/data reversed`.",
	  { "while lo < hi:", "  bubble forward lo..hi, hi ← hi − 1", "  bubble backward hi..lo, lo ← lo + 1" } },
	{ "oddeven", " brick odd-even transposition ", "Odd-even transposition sort", "exchange", a_oddeven, "n", "n²", "n²", "1", true, true, false, 0, "Bricklaying",
	  "Alternates comparing all odd pairs and all even pairs. Every comparison in a round is independent, so it maps onto parallel hardware.",
	  "It is also the row and column step of shear sort.",
	  { "repeat:", "  compare-swap (1,2), (3,4), …", "  compare-swap (0,1), (2,3), …", "until nothing moves" } },
	{ "comb", " combsort ", "Comb sort", "exchange", a_comb, "n log n", "n²/2ᵖ", "n²", "1", false, true, false, 0, "Combing",
	  "Bubble sort with a gap that shrinks by 1.3 each pass, so turtles jump far early on.",
	  "The gap sequence is n/1.3, n/1.3², … down to 1.",
	  { "gap ← n", "repeat:", "  gap ← max(1, gap / 1.3)", "  compare-swap a[i], a[i+gap] for all i", "until gap = 1 and no swaps" } },
	{ "gnome", " stupid ", "Gnome sort", "exchange", a_gnome, "n", "n²", "n²", "1", true, true, false, 0, "Gnoming",
	  "A garden gnome sorting flower pots: step forward while the pots are in order, swap and step back when they are not.",
	  "It does the same swaps as insertion sort, one at a time.",
	  { "pos ← 0", "while pos < n:", "  if pos = 0 or a[pos−1] ≤ a[pos]: pos ← pos + 1", "  else: swap a[pos], a[pos−1]; pos ← pos − 1" } },
	{ "exchange", " interchange ", "Exchange sort", "exchange", a_exchange, "n²", "n²", "n²", "1", false, true, false, 0, "Exchanging",
	  "Compares a[i] with every later element and swaps whenever the later one is smaller. The classic Sort-CLI \"interchange sort\".",
	  "Always n(n−1)/2 comparisons, whatever the input.",
	  { "for i in 0 .. n−2:", "  for j in i+1 .. n−1:", "    if a[i] > a[j]: swap" } },
	{ "circle", " circlesort ", "Circle sort", "exchange", a_circle, "n log n", "n log n log n", "n log n log n", "log n", false, true, false, 0, "Circling",
	  "Compares the first and last element, the second and second-last, and so on, then recurses on each half. Repeats until a round swaps nothing.",
	  "Added to SortVis in 0.6.",
	  { "circle(lo, hi):", "  compare-swap mirrored pairs", "  circle(left half), circle(right half)", "repeat while anything moved" } },
	{ "quick", " quicksort lomuto ", "Quicksort (Lomuto)", "exchange", a_quick, "n log n", "n log n", "n²", "log n", false, true, false, 0, "Partitioning",
	  "Uses the last element as the pivot, moves everything smaller to its left, then sorts both sides.",
	  "Sorted or reversed input is its worst case: try `/data sorted`.",
	  { "quick(lo, hi):", "  p ← a[hi], i ← lo", "  for j in lo .. hi−1: if a[j] < p: swap a[i], a[j]; i++", "  swap a[i], a[hi]", "  quick(lo, i−1), quick(i+1, hi)" } },
	{ "hoare", " hoarequick quickhoare ", "Quicksort (Hoare)", "exchange", a_hoare, "n log n", "n log n", "n²", "log n", false, true, false, 0, "Partitioning",
	  "Hoare's original partition: two cursors move inwards from both ends and swap pairs that are on the wrong side of the middle pivot.",
	  "Fewer swaps than Lomuto, and fine on sorted input.",
	  { "p ← a[mid], i ← lo−1, j ← hi+1", "loop:", "  do i++ while a[i] < p", "  do j−− while a[j] > p", "  if i ≥ j: break", "  swap a[i], a[j]", "quick(lo, j), quick(j+1, hi)" } },
	{ "quick3", " 3way three-way dutch dijkstra ", "3-way quicksort", "exchange", a_quick3, "n", "n log n", "n²", "log n", false, true, false, 0, "Partitioning",
	  "Dijkstra's Dutch national flag partition: values below, equal to and above the pivot. Equal values are finished in one go.",
	  "Shines on `/data few` and `/data dups`.",
	  { "v ← a[lo], lt ← lo, gt ← hi, i ← lo+1", "while i ≤ gt:", "  a[i] < v: swap a[lt++], a[i++]", "  a[i] > v: swap a[i], a[gt−−]", "  else: i++", "sort a[lo..lt−1] and a[gt+1..hi]" } },
	{ "dualpivot", " dual yaroslavskiy dual-pivot ", "Dual-pivot quicksort", "exchange", a_dualpivot, "n log n", "n log n", "n²", "log n", false, true, false, 0, "Partitioning",
	  "Yaroslavskiy's quicksort with two pivots and three parts. Java uses it for primitive arrays.",
	  "Fewer element moves per level than single-pivot quicksort.",
	  { "p ← a[lo], q ← a[hi] (p ≤ q)", "split into < p, p..q, > q", "put the pivots in place", "sort the three parts" } },
	{ "selection", " select ", "Selection sort", "selection", a_selection, "n²", "n²", "n²", "1", false, true, false, 0, "Selecting",
	  "Finds the smallest remaining value and swaps it into place. Never more than n−1 swaps.",
	  "Always n(n−1)/2 comparisons, but very few writes.",
	  { "for i in 0 .. n−2:", "  m ← index of min a[i..n−1]", "  swap a[i], a[m]" } },
	{ "dselection", " double doubleselection minmax ", "Double selection sort", "selection", a_dselection, "n²", "n²", "n²", "1", false, true, false, 0, "Selecting",
	  "Selection sort that finds the minimum and the maximum in the same pass, filling the array from both ends.",
	  "Half as many passes as selection sort, the same number of comparisons.",
	  { "while lo < hi:", "  find min and max of a[lo..hi]", "  swap min to lo, max to hi", "  lo++, hi−−" } },
	{ "heap", " heapsort ", "Heapsort", "selection", a_heap, "n log n", "n log n", "n log n", "1", false, true, false, 0, "Heapifying",
	  "Arranges the array as a binary max-heap, then repeatedly swaps the top to the end and repairs the heap.",
	  "Guaranteed n log n with no extra memory, but jumps around a lot.",
	  { "build a max-heap", "for end ← n−1 down to 1:", "  swap a[0], a[end]", "  sift a[0] down in a[0..end−1]" } },
	{ "smooth", " smoothsort leonardo ", "Smoothsort", "selection", a_smooth, "n", "n log n", "n log n", "1", false, true, false, 0, "Smoothing",
	  "Dijkstra's heapsort over a forest of Leonardo heaps. Nearly sorted input costs close to O(n).",
	  "Compare with heapsort on `/data nearly`.",
	  { "for each element: add it to the Leonardo forest", "  merge the two last heaps when their orders are k+1, k", "  restore heap order of the roots", "for i ← n−1 down to 0:", "  remove root, expose its two child heaps", "  restore the roots" } },
	{ "cycle", " cyclesort ", "Cycle sort", "selection", a_cycle, "n²", "n²", "n²", "1", false, true, false, 0, "Cycling",
	  "Counts how many values are smaller to find where each value belongs and rotates cycles into place. Writes each element at most once.",
	  "Minimal writes: useful when writing is expensive (flash memory).",
	  { "for each start:", "  item ← a[start]", "  pos ← start + #(smaller values after it)", "  write item at pos, carry the displaced value", "  repeat until the cycle returns to start" } },
	{ "pancake", " flip ", "Pancake sort", "selection", a_pancake, "n", "n²", "n²", "1", false, true, false, 0, "Flipping",
	  "Only allowed move: reverse a prefix (flip the top of a pancake stack). Brings the largest pancake up, then flips it to the bottom.",
	  "At most 2n − 3 flips.",
	  { "for size ← n down to 2:", "  m ← index of max a[0..size−1]", "  flip(0..m), flip(0..size−1)" } },
	{ "insertion", " insert insertsort ", "Insertion sort", "insertion", a_insertion, "n", "n²", "n²", "1", true, true, false, 0, "Inserting",
	  "Takes values one at a time and slides each left into the already sorted prefix. Very fast on small or nearly sorted data.",
	  "Moves equal the number of inversions. TimSort uses it for short runs.",
	  { "for i in 1 .. n−1:", "  key ← a[i], j ← i−1", "  while j ≥ 0 and a[j] > key: a[j+1] ← a[j]; j−−", "  a[j+1] ← key" } },
	{ "binsertion", " binary binaryinsertion ", "Binary insertion sort", "insertion", a_binsertion, "n log n", "n²", "n²", "1", true, true, false, 0, "Inserting",
	  "Insertion sort that finds the spot by binary search: log n comparisons per value, but the same shifting.",
	  "Fewer comparisons than insertion sort, the same writes.",
	  { "for i in 1 .. n−1:", "  key ← a[i]", "  p ← first position in a[0..i−1] with a[p] > key", "  shift a[p..i−1] right, a[p] ← key" } },
	{ "shell", " shellsort ", "Shellsort", "insertion", a_shell, "n log n", "n^1.25", "n^1.5", "1", false, true, false, 0, "Shelling",
	  "Insertion sort over elements a gap apart, with the gap shrinking (Ciura's gaps 701, 301, 132, 57, 23, 10, 4, 1).",
	  "The early long-distance moves leave little work for the final gap-1 pass.",
	  { "for gap in 701, 301, 132, 57, 23, 10, 4, 1:", "  insertion sort a[i], a[i+gap], a[i+2gap], …" } },
	{ "merge", " mergesort topdown ", "Merge sort", "merge", a_merge, "n log n", "n log n", "n log n", "n", true, false, false, 0, "Merging",
	  "Splits the array in half, sorts each half, then merges them through a buffer. Stable and always n log n.",
	  "Writes n values per level, log n levels.",
	  { "sort(lo, hi):", "  if lo < hi:", "    mid ← (lo + hi) / 2", "    sort(lo, mid), sort(mid+1, hi)", "    merge the halves through a buffer" } },
	{ "bmerge", " bottomup bottom-up ", "Bottom-up merge sort", "merge", a_bmerge, "n log n", "n log n", "n log n", "n", true, false, false, 0, "Merging",
	  "Merge sort without recursion: merges pairs of 1s, then 2s, 4s, 8s … across the whole array.",
	  "Watch the merged blocks double in size.",
	  { "for width ← 1, 2, 4, … < n:", "  for each pair of blocks of that width:", "    merge them" } },
	{ "timsort", " tim tim-sort ", "TimSort", "merge", a_timsort, "n", "n log n", "n log n", "n", true, false, false, 0, "Galloping",
	  "Python's and Java's sort. Finds natural runs (reversing descending ones), extends short runs to minrun with binary insertion, and merges runs from a stack that keeps their lengths balanced. Merges switch to galloping (exponential search) when one run keeps winning.",
	  "This port uses MIN_MERGE 16 instead of Java's 32 or CPython's 64, so runs and merges show on small arrays. Try `/data runs`.",
	  { "minrun ← from n", "for each natural run (reverse if descending):", "  extend to minrun with binary insertion", "  push it; merge while the stack breaks", "    |Z| > |Y| + |X| and |Y| > |X|", "merge what is left", "merges gallop after 7 wins in a row" } },
	{ "intro", " introsort introspective ", "Introsort", "hybrid", a_intro, "n log n", "n log n", "n log n", "log n", false, true, false, 0, "Introspecting",
	  "Quicksort with a median-of-three pivot that switches to heapsort when recursion gets too deep, leaving small pieces for one final insertion sort. C++'s std::sort.",
	  "The depth limit (2 log₂ n) protects against quicksort's worst case.",
	  { "intro(lo, hi, depth):", "  while size > 16:", "    if depth = 0: heapsort; return", "    partition around median of three", "    intro(right part), continue with the left", "insertion sort everything" } },
	{ "counting", " count countsort ", "Counting sort", "distribution", a_counting, "n + k", "n + k", "n + k", "n + k", true, false, false, 0, "Counting",
	  "Never compares: counts how often each value occurs and uses running totals to place every value directly.",
	  "Zero comparisons. k is the value range, so it suits small integers.",
	  { "count[v]++ for every value", "turn counts into running totals", "place each value at its total (right to left keeps it stable)" } },
	{ "radix", " lsd radixsort ", "Radix sort (LSD)", "distribution", a_radix, "d·n", "d·n", "d·n", "n", true, false, false, 0, "Distributing",
	  "Sorts by the last decimal digit, then the next, with a stable counting pass each time.",
	  "d passes for d-digit values, no comparisons at all.",
	  { "for each digit, least significant first:", "  stable counting sort by that digit" } },
	{ "msd", " msdradix radixmsd ", "Radix sort (MSD)", "distribution", a_msd, "d·n", "d·n", "d·n", "n", true, false, false, 0, "Distributing",
	  "Splits by the most significant digit into ten buckets, then sorts each bucket by the next digit.",
	  "Like sorting words by first letter, then second letter.",
	  { "msd(lo, hi, digit):", "  distribute by that digit", "  for each bucket: msd(bucket, next digit)" } },
	{ "bucket", " bucketsort bin ", "Bucket sort", "distribution", a_bucket, "n + k", "n + k", "n²", "n", true, false, false, 0, "Bucketing",
	  "Drops values into √n buckets by size, writes the buckets back in order, then insertion-sorts each bucket.",
	  "Fast when the values are spread evenly; one crowded bucket makes it slow.",
	  { "k ← √n buckets over [min, max]", "append each value to its bucket", "write the buckets back in order", "insertion sort each bucket" } },
	{ "bitonic", " bitonicsort ", "Bitonic sort", "network", a_bitonic, "n log² n", "n log² n", "n log² n", "1", false, true, false, 0, "Networking",
	  "Batcher's sorting network: builds bitonic sequences (up then down) and merges them with fixed compare-swaps. This is the version for any n.",
	  "The comparisons do not depend on the data, which is why GPUs like it.",
	  { "sort(lo, n, up):", "  sort first half descending, second half ascending", "  merge(lo, n, up)", "merge: compare-swap i with i+m (m = largest power of 2 < n)", "  then merge both parts" } },
	{ "batcher", " oddevenmerge odd-even-merge ", "Odd-even merge sort", "network", a_batcher, "n log² n", "n log² n", "n log² n", "1", false, true, false, 0, "Networking",
	  "Batcher's other sorting network: merges sorted blocks of 1, 2, 4 … with a fixed pattern of compare-swaps.",
	  "Fewer comparators than bitonic sort.",
	  { "for p ← 1, 2, 4, … < n:", "  for k ← p, p/2, … 1:", "    compare-swap i+j with i+j+k inside the same 2p block" } },
	{ "stooge", " stoogesort ", "Stooge sort", "slow", a_stooge, "n^2.71", "n^2.71", "n^2.71", "log n", false, true, false, 64, "Stooging",
	  "Sorts the first two thirds, the last two thirds, then the first two thirds again. Correct, and famously slow.",
	  "Limited to 64 values.",
	  { "stooge(i, j):", "  if a[i] > a[j]: swap", "  if j − i + 1 > 2:", "    t ← (j − i + 1) / 3", "    stooge(i, j−t), stooge(i+t, j), stooge(i, j−t)" } },
	{ "slowsort", " slow ", "Slowsort", "slow", a_slowsort, "n^(log n)", "n^(log n)", "n^(log n)", "n", false, true, false, 32, "Procrastinating",
	  "\"Multiply and surrender\": finds the maximum by recursively sorting both halves, moves it to the end, then sorts everything else again.",
	  "Limited to 32 values.",
	  { "slow(i, j):", "  slow(i, m), slow(m+1, j)", "  if a[m] > a[j]: swap", "  slow(i, j−1)" } },
	{ "bogo", " bogosort monkey stupid random ", "Bogosort", "slow", a_bogo, "n", "n·n!", "∞", "1", false, true, false, 7, "Shuffling",
	  "Shuffles until the array happens to be sorted.",
	  "Limited to 7 values, which already needs about 5,000 shuffles.",
	  { "while not sorted:", "  shuffle" } },
	{ "shear", " shearsort snake ", "Shear sort", "2d", a_shear, "n√n log n", "n√n log n", "n√n log n", "1", false, true, true, 0, "Shearing",
	  "A 2D sort for mesh computers: sorts rows in alternating directions, then columns, ⌈log₂ h⌉ + 1 times. The result is in snake order.",
	  "Only in 2D mode (`/mode 2d`). Rows and columns use odd-even transposition.",
	  { "repeat ⌈log₂ rows⌉ + 1 times:", "  sort even rows left to right, odd rows right to left", "  sort every column top to bottom (not after the last)" } },
};
#define NALGS ((int)(sizeof ALGS / sizeof ALGS[0]))

/* run alg on in[0..n) into T; false when the trace grew past the cap */
static bool sort_run(const ALG * al, const EL * in, int n, int w, int h, TRACE * T) {
	unsigned long long saved = g_rng;
	memset(T, 0, sizeof *T);
	T->len = n;
	T->start = tmp_alloc(n);
	memcpy(T->start, in, sizeof(EL) * (size_t)n);
	EL * a = tmp_alloc(n);
	memcpy(a, in, sizeof(EL) * (size_t)n);
	SCTX c = { a, n, T, w, h, 0, 0, 0, 0 };
	rng_seed(g_seed * 31 + 7);						/* bogosort shuffles reproducibly */
	if (n > 1) al->fn(&c);
	pivot(&c, -1);
	focus(&c, 1, 0);
	note(&c, "Sorted ✓");
	done(&c, 0, n - 1);
	g_rng = saved;
	T->final = a;
	T->cmps = c.cmps;
	T->swaps = c.swaps;
	T->writes = c.writes;
	T->reads = c.reads;
	return !T->overflow;
}

#endif
