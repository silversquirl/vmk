#include <errno.h>
#include <stdio.h>

#define VSTRING_IMPL
#include "vlib/vstring.h"

#include "preproc.h"

enum tok_status {
	TS_ERROR = -1,
	TS_EOT, // End of token
	TS_EOL, // End of line
	TS_EOF, // End of file
	TS_INVALID, // Invalid token, skip this directive
};

static enum tok_status read_tok(vstring *str, FILE *fp) {
	*str = vs_alloc(256);

	_Bool quoted = 0;
	size_t i = 0;

	int ch;
	while ((ch = fgetc(fp)) != EOF) {
		if (i == 0) {
			if (ch == '"' && !quoted) {
				quoted = 1;
				continue;
			}

			if (ch == ' ' || ch == '\t') continue;

			if (ch == '\n') {
				vs_free(*str);
				return TS_INVALID;
			}
		}

		if (ch == '\n' || (!quoted && (ch == ' ' || ch == '\t')) || ch == '"') {
			if (ch == '"' && !quoted) ungetc(ch, fp);

			*str = vs_resize(*str, i);

			if (ch == '\n') return TS_EOL;
			return TS_EOT;
		}

		if (!quoted && (ch == '<' || ch == '>')) {
			vs_free(*str);
			return TS_INVALID;
		}

		size_t len = vs_len(*str);
		if (i >= len) {
			*str = vs_resize(*str, len*2);
		}
		(*str)[i++] = ch;
	}

	if (ferror(fp)) {
		int err_no = errno;
		vs_free(*str);
		errno = err_no;
		return TS_ERROR;
	}

	*str = vs_resize(*str, i);
	return TS_EOF;
}

static enum direc_status find_next_directive(FILE *fp, _Bool start_of_line) {
	int ch;
	while ((ch = fgetc(fp)) != EOF) {
		// FIXME: breaks on multiline strings, godo string parser
		if (ch == '\n') {
			start_of_line = 1;
			continue;
		}

		if (!start_of_line) continue;
		if (ch == ' ' || ch == '\t') continue;
		if (ch == '#') return DS_NORMAL;
	}

	if (ferror(fp)) return DS_ERROR;
	return DS_EOF;
}

int read_directive(FILE *fp, struct directive *direc) {
	static enum directive_type partial_type = DT_NONE;

	if (feof(fp)) {
		return DS_EOF;
	}

	_Bool start_of_line = 1;

	if (partial_type) {
		direc->type = partial_type;
		partial_type = DT_NONE;

		switch (read_tok(&direc->arg, fp)) {
		case TS_ERROR:
			perror("fgetc");
			return DS_ERROR;

		case TS_EOT:
			partial_type = direc->type;
			// fallthrough
		case TS_EOL:
		case TS_EOF:
			return DS_NORMAL;

		case TS_INVALID:
			// Continue parsing
			start_of_line = 0;
			break;
		}
	}

	vstring tok = NULL;
	while (1) {
		if (tok) {
			vs_free(tok);
			tok = NULL;
		}

		switch (find_next_directive(fp, start_of_line)) {
		case DS_NORMAL:
			break;

		case DS_ERROR:
			perror("fgetc");
			return DS_ERROR;

		case DS_EOF:
			return DS_EOF;
		}

		switch (read_tok(&tok, fp)) {
		case TS_ERROR:
			perror("fgetc");
			return DS_ERROR;

		case TS_EOT:
			break;

		case TS_EOL:
			// Skip to the next directive
			start_of_line = 1;
			continue;

		case TS_INVALID:
			// Skip to the next directive
			start_of_line = 0;
			continue;

		case TS_EOF:
			vs_free(tok);
			return DS_EOF;
		}

		if (!strcasecmp(tok, "include")) {
			direc->type = DT_INCLUDE;
			switch (read_tok(&direc->arg, fp)) {
			case TS_ERROR:
				perror("fgetc");
				return DS_ERROR;

			case TS_EOT:
			case TS_EOL:
			case TS_EOF:
				return DS_NORMAL;

			case TS_INVALID:
				// Skip to the next directive
				start_of_line = 0;
				continue;
			}
		}

		if (!strcasecmp(tok, "pragma")) {
			// We need a second token to identify pragma directives
			vs_free(tok);
			switch (read_tok(&tok, fp)) {
			case TS_ERROR:
				perror("fgetc");
				return DS_ERROR;

			case TS_EOT:
				break;
			
			case TS_EOL:
				// Skip to the next directive
				start_of_line = 1;
				continue;

			case TS_INVALID:
				// Skip to the next directive
				start_of_line = 0;
				continue;

			case TS_EOF:
				vs_free(tok);
				return DS_EOF;
			}

			if (!strcasecmp(tok, "vmk")) {
				direc->type = DT_PRAGMA;
			} else if (!strcasecmp(tok, "link")) {
				direc->type = DT_LINK;
			} else if (!strcasecmp(tok, "dep")) {
				direc->type = DT_DEP;
			}

			switch (read_tok(&direc->arg, fp)) {
			case TS_ERROR:
				perror("fgetc");
				return DS_ERROR;

			case TS_EOT:
				partial_type = direc->type;
				// fallthrough
			case TS_EOL:
			case TS_EOF:
				return DS_NORMAL;

			case TS_INVALID:
				// Skip to the next directive
				start_of_line = 0;
				continue;
			}
		}
	}
}
