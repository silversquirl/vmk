#ifndef VMK_PREPROC_H
#define VMK_PREPROC_H

#include <stdio.h>
#include "vlib/vstring.h"

enum { TOKEN_MAX = 256 };

enum directive_type {
	DT_NONE, // Used internally
	DT_PRAGMA, // #pragma vmk
	DT_LINK, // #pragma link
	DT_DEP, // #pragma dep
	DT_INCLUDE, // #include
};

struct directive {
	enum directive_type type;
	vstring arg;
};

enum direc_status {
	DS_EOF = -1, // A directive is NOT returned in this case
	DS_NORMAL,
	DS_ERROR,
};

// NOT THREAD SAFE
enum direc_status read_directive(FILE *fp, struct directive *direc);

#endif
