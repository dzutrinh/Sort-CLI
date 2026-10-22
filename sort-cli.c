/*
 *	SORT-CLI.C
 *	----------
 *	Sort algorithm visualizations in a chat-style terminal UI
 *	Version 1.0.0
 *	Coded by Trinh D.D. Nguyen
 *	MIT licensed
 *
 *	Compile:
 *		make
 *	or:
 *		cc -O2 -std=c99 sort-cli.c -o sort-cli -lm
 *	or:
 *		mingw32-make
 *
 *	1.0.0:
 *	- Claude-CLI-style interface: transcript, prompt with slash commands
 *	  and autocomplete, plain-language requests, spinner and hints
 *	- 35 algorithms, among them TimSort, smoothsort, introsort, dual-pivot
 *	  quicksort, radix, bucket, bitonic and Batcher networks, shear sort
 *	- Step forwards and backwards through any run; races of several
 *	  algorithms on the same data; work counts with /compare
 *	- 1D bars or 2D matrices (flat, snake, per row, per column)
 *	- Gradients shared with Search-CLI: 17 presets, custom stops, reverse,
 *	  in truecolor or xterm-256
 *	- Bitsy the bar critter above the prompt (/pet on|off)
 *	- Sample kinds, sizes and a reproducible seed
 *	0.6.0 and earlier: menu-driven SortVis with 13 algorithms
 */

#include "sort-cli.h"

static void app_version(void) {
	printf("Sort-CLI %s (%s)\nCoded by Trinh D.D. Nguyen\n", APP_VERSION, APP_PLATFORM);
	exit(0);
}

static void app_help(void) {
	printf("Sort-CLI - Sort Algorithm Visualizations\n");
	printf("========================================\n\n");
	printf("USAGE:\n");
	printf("  sort-cli [OPTIONS]\n\n");
	printf("OPTIONS:\n");
	printf("  -v, --version          Display program version information\n");
	printf("  -h, --help             Display this help message\n");
	printf("  -s, --speed <ms>       Milliseconds per animation frame (default: 30, range 5-2000)\n");
	printf("      --seed <n>         Seed for the samples, for reproducible runs\n");
	printf("  -g, --gradient <name>  Color gradient preset (rainbow, viridis, plasma, magma, turbo, ...)\n");
	printf("      --truecolor        Smooth 24-bit gradients (default when COLORTERM=truecolor)\n");
	printf("      --256              Use the 256-color palette\n");
	printf("      --no-color         Plain output (also when NO_COLOR is set)\n\n");
	printf("Inside the app type /help. Requests can also be piped in, one per line:\n");
	printf("  printf '/data dups 24\\n/compare\\n' | sort-cli\n\n");
	printf("REQUIREMENTS:\n");
	printf("  macOS or Linux with a UTF-8, 256-color terminal\n");
	printf("  Windows 10 (build 10586) or later for VT support\n");
	exit(0);
}

static long number_arg(int argc, char ** argv, int i, long lo, long hi) {
	char * end;
	if (i + 1 >= argc) {
		fprintf(stderr, "Error: %s requires a value\nUse --help for usage information\n", argv[i]);
		exit(1);
	}
	long v = strtol(argv[i + 1], &end, 10);
	if (end == argv[i + 1] || *end || v < lo || v > hi) {
		fprintf(stderr, "Error: invalid value '%s' for %s (allowed %ld-%ld)\n", argv[i + 1], argv[i], lo, hi);
		exit(1);
	}
	return v;
}

static void app_params(int argc, char ** argv) {
	const char * nc = getenv("NO_COLOR");
	if (nc && *nc) g_color = false;
	rng_seed((unsigned long long)time(NULL));
	g_seed = new_seed();
	grad_init();
	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-v") || !strcmp(argv[i], "--version")) app_version();
		else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) app_help();
		else if (!strcmp(argv[i], "-s") || !strcmp(argv[i], "--speed")) g_speed = (int)number_arg(argc, argv, i++, 5, 2000);
		else if (!strcmp(argv[i], "--seed")) g_seed = (unsigned long)number_arg(argc, argv, i++, 0, 2147483647L);
		else if (!strcmp(argv[i], "--no-color")) g_color = false;
		else if (!strcmp(argv[i], "--truecolor")) { g_truecolor = true; grad_build(); }
		else if (!strcmp(argv[i], "--256")) { g_truecolor = false; grad_build(); }
		else if (!strcmp(argv[i], "-g") || !strcmp(argv[i], "--gradient")) {
			int p = i + 1 < argc ? grad_find(argv[i + 1]) : -1;
			if (p < 0) {
				fprintf(stderr, "Error: --gradient needs a preset name:");
				for (int k = 0; k < NGPRESETS; k++) fprintf(stderr, " %s", GPRESETS[k].name);
				fprintf(stderr, "\n");
				exit(1);
			}
			bool tc = g_truecolor;
			grad_use_preset(p);
			g_truecolor = tc;
			grad_build();
			i++;
		}
		else {
			fprintf(stderr, "Error: unknown option '%s'\nUse --help for usage information\n", argv[i]);
			exit(1);
		}
	}
}

int main(int argc, char ** argv) {
	app_params(argc, argv);
	if (!term_start()) {
		fprintf(stderr, "Sort-CLI needs a terminal with VT/ANSI support (Windows 10 1511 or later).\n");
		return 1;
	}
	app_setup();
	repl();
	term_restore();
	return 0;
}
