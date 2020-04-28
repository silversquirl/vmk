#pragma vmk static
#define _POSIX_C_SOURCE 200809L

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "preproc.h"
#pragma link "preproc.o"

const char *USAGE[] = {
	"Usage: vmk FILE.c ...",
	NULL,
};

int usage(int code) {
	FILE *stream = code ? stderr : stdout;
	for (const char **line = USAGE; *line; line++) {
		fprintf(stream, "%s\n", *line);
	}
	return code;
}

struct stringset {
	size_t len, cap;
	vstring *strs;
};

struct stringset ss_new() {
	return (struct stringset){
		0, 16,
		malloc(16 * sizeof (vstring)),
	};
}

_Bool ss_add(struct stringset *ss, vstring str) {
	// TODO: Linear search is O(n). Optimize using either binary search or hash tables
	for (int i = 0; i < ss->len; i++) {
		if (!strcmp(ss->strs[i], str)) return 0;
	}

	if (ss->len >= ss->cap) ss->strs = realloc(ss->strs, sizeof *ss->strs * (ss->cap *= 2));
	ss->strs[ss->len++] = str;
	return 1;
}

int vmk(const char *fn) {
	// FIXME: not freed in error cases. Probably fine though, since the whole program will exit shortly after
	struct stringset libs = ss_new();
	struct stringset objects = ss_new();
	struct stringset deps = ss_new();

	ss_add(&deps, (vstring)fn); // CAREFUL! fn is not a vstring

	char *ext = strrchr(fn, '.');
	if (ext && !strcmp(ext, ".c")) {
		size_t len = strlen(fn);
		vstring o_file = vs_new_n(fn, len);
		o_file[len-1] = 'o';
		ss_add(&objects, o_file);
	}

	for (size_t i = 0; i < deps.len; i++) {
		printf("Resolving dependencies for %s\n", deps.strs[i]);

		FILE *fp = fopen(deps.strs[i], "r");
		if (!fp) {
			perror("fopen");
			return 1;
		}

		struct directive direc;
		enum direc_status status;
		while ((status = read_directive(fp, &direc)) == DS_NORMAL) {
			switch (direc.type) {
			case DT_PRAGMA:
				// TODO
				vs_free(direc.arg);
				break;

			case DT_LINK:
				ext = strrchr(direc.arg, '.');
				if (ext && !strcmp(ext, ".o")) {
					ss_add(&objects, direc.arg);

					size_t len = vs_len(direc.arg);
					vstring c_file = vs_new_n(direc.arg, len);
					c_file[len-1] = 'c';
					ss_add(&deps, c_file);
				} else {
					ss_add(&libs, direc.arg);
				}
				break;

			case DT_DEP:
			case DT_INCLUDE:
				ss_add(&deps, direc.arg);
				break;

			case DT_NONE:
				break;
			}
		}

		fclose(fp);

		if (status == DS_ERROR) return 1;
	}

	// Adjusted for index 0 being not a vstring
	for (size_t i = 1; i < deps.len; i++) vs_free(deps.strs[i]);

	// Create the command
	// cc -o <binary> <object>... -l<lib>... NULL
	vstring cmd[3 + objects.len + libs.len + 1];
	size_t i = 0;
	cmd[i++] = vs_new("cc"); cmd[i++] = vs_new("-o");

	// String extension
	ext = strrchr(fn, '.');
	if (ext) cmd[i++] = vs_new_n(fn, ext - fn);
	else cmd[i++] = vs_new(fn);

	for (size_t j = 0; j < objects.len; j++) {
		cmd[i++] = objects.strs[j];
	}

	for (size_t j = 0; j < libs.len; j++) {
		cmd[i] = vs_alloc(vs_len(libs.strs[j]) + 2);
		cmd[i][0] = '-'; cmd[i][1] = 'l';
		memcpy(cmd[i]+2, libs.strs[j], vs_len(libs.strs[j]));
		i++;

		vs_free(libs.strs[j]);
	}

	printf("$");
	for (size_t j = 0; j < i; j++) {
		printf(" %s", cmd[j]);
	}
	putchar('\n');

	cmd[i] = NULL;
	execvp(cmd[0], cmd);

	return 0;
}

int main(int argc, char *argv[]) {
	int opt;
	while ((opt = getopt(argc, argv, "h")) > 0) {
		switch (opt) {
		case 'h':
			return usage(0);

		default:
			return usage(1);
		}
	}

	if (optind >= argc) {
		return usage(1);
	}

	for (int i = optind; i < argc; i++) {
		if (vmk(argv[i])) return 1;
	}
	return 0;
}
