#include <assert.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <libgen.h>
#include <limits.h>
#include <regex.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <elfutils/libdwfl.h>

enum {
	SU_ATTR_STATIC = 0,
	SU_ATTR_DYNAMIC,
	SU_ATTR_BOUNDED,
};

typedef struct {
	size_t size;
	bool attrs[3];
} stackuse;

/* Regex for parsing the gcc -fstack-usage file */
#define REGEXPR ".+:[0-9]+:[0-9]+:(.+)	([0-9]+)	([a-z,]+)"
#define REGSUBS 4 /* 3 subexpressions +1 for entire expression */

/* reg_t for parsing the regex from above */
static regex_t sureg;

/* Command used for src2su */
static char *cmd;

/* Taken from elfutils example code. */
static char *debuginfo_path;
static const Dwfl_Callbacks offline_callbacks = {
	.find_debuginfo = dwfl_standard_find_debuginfo,
	.debuginfo_path = &debuginfo_path,

	.section_address = dwfl_offline_section_address,
	.find_elf = dwfl_build_id_find_elf,
};

static void
usage(char *prog)
{
	fprintf(stderr, "USAGE: %s ELF CMD\n", basename(prog));
	exit(EXIT_FAILURE);
}

static char *
src2su(const char *src)
{
	static char dest[PATH_MAX + 2]; /* +2 for null byte and newline */
	char *newline;
	FILE *out;
	pid_t pid;
	int p[2];

	if (pipe(p))
		err(EXIT_FAILURE, "pipe failed");

	/* XXX: Instead of forking a new process for each invocation of
	 * this function it would be more efficient to continously run
	 * the script in the background, pass source files paths on
	 * standard input and receive stack-usage paths on standard
	 * output. Implementing this will result in better performance. */
	switch ((pid = fork())) {
	case 0:
		close(p[0]); /* close unused read-end */
		close(STDOUT_FILENO);
		dup(p[1]);
		close(p[1]);

		execl(cmd, cmd, src, (char*)NULL);
		err(EXIT_FAILURE, "execlp failed");
	case -1:
		err(EXIT_FAILURE, "fork failed");
	default:
		close(p[1]); /* close unused write-end */
		if (!(out = fdopen(p[0], "r")))
			err(EXIT_FAILURE, "fdopen failed on pipe");

		if (!fgets(dest, sizeof(dest), out))
			errx(EXIT_FAILURE, "fgets returned no data");
		if ((newline = strchr(dest, '\n')))
			*newline = '\0';

		if (fclose(out))
			err(EXIT_FAILURE, "fclose failed");
	}

	return dest;
}

static char *
getsufp(Dwfl *dwfl, GElf_Addr addr)
{
	Dwfl_Line *line;
	const char *srcfp;

	line = dwfl_getsrc(dwfl, addr);
	if (!(srcfp = dwfl_lineinfo(line, NULL, NULL, NULL, NULL, NULL)))
		return NULL;

	return src2su(srcfp);
}

static char *
parsesu(stackuse *out, char *line)
{
	char *attrs, *name, *p;
	regmatch_t matches[REGSUBS];
	unsigned long long s;

	if (regexec(&sureg, line, REGSUBS, matches, 0))
		return NULL;

	errno = 0; /* reset errno for error check */
	s = strtoull(&line[matches[2].rm_so], NULL, 10);
	if (errno != 0)
		return NULL;
	assert(s <= SIZE_MAX);
	out->size = (size_t)s;

	attrs = &line[matches[3].rm_so];
	out->attrs[SU_ATTR_STATIC] = strstr(attrs, "static");
	out->attrs[SU_ATTR_DYNAMIC] = strstr(attrs, "dynamic");
	out->attrs[SU_ATTR_BOUNDED] = strstr(attrs, "bounded");

	name = &line[matches[1].rm_so];
	if ((p = strchr(name, '\t')))
		*p = '\0';

	/* XXX: This is a pointer to an element in `line`. */
	return name;
}

/* XXX: This is somewhat inefficient as the same file will likely be
 * parsed multiple times. Caching parsing results would be desirable. */
static stackuse *
getsu(stackuse *out, const char *fp, const char *fn)
{
	FILE *in;
	ssize_t n;
	size_t llen;
	char *line, *name;
	unsigned int lnum;

	if (!(in = fopen(fp, "r")))
		err(EXIT_FAILURE, "fopen failed for '%s'", fp);

	line = NULL;
	llen = 0;

	lnum = 1;
	while ((n = getline(&line, &llen, in)) > 0) {
		if (!(name = parsesu(out, line)))
			errx(EXIT_FAILURE, "%s:%u: malformed input", fp, lnum);
		if (!strcmp(name, fn))
			return out;
		lnum++;
	}
	if (ferror(in))
		errx(EXIT_FAILURE, "ferror failed");

	if (fclose(in))
		err(EXIT_FAILURE, "fclose failed");
	return NULL;
}

static void
printdb(FILE *out, Dwfl *dwfl, int fd)
{
	int i, n;
	Dwfl_Module *mod;

	dwfl_report_begin(dwfl);
	if (!(mod = dwfl_report_offline(dwfl, "main", "main", fd)))
		errx(EXIT_FAILURE, "dwfl_report_offline failed");

	if ((n = dwfl_module_getsymtab(mod)) == -1)
		errx(EXIT_FAILURE, "dwfl_module_getsymtab failed");

	for (i = 0; i < n; i++) {
		stackuse su;
		GElf_Sym sym;
		GElf_Addr addr;
		const char *name;
		const char *sufp;

		name = dwfl_module_getsym_info(mod, i, &sym, &addr, NULL, NULL, NULL);
		if (!name || GELF_ST_TYPE(sym.st_info) != STT_FUNC)
			continue; /* not a function symbol */

		if (!(sufp = getsufp(dwfl, addr))) {
			warnx("no line information for symbol '%s'", name);
			continue;
		}
		if (!getsu(&su, sufp, name)) {
			warnx("stack usage information for symbol '%s' not found", name);
			continue;
		}

		if (!(su.attrs[SU_ATTR_STATIC] || su.attrs[SU_ATTR_BOUNDED]))
			warnx("function '%s' has an unbounded stack", name);
		fprintf(out, "%"PRIu64"\t%s\t%zu\n", (uint64_t)addr, name, su.size);
	}

	dwfl_report_end(dwfl, NULL, NULL);
}

int
main(int argc, char **argv)
{
	int fd;
	char *elf;
	Dwfl *dwfl;

	if (regcomp(&sureg, REGEXPR, REG_EXTENDED))
		errx(EXIT_FAILURE, "regcomp failed");
	if (argc < 3)
		usage(argv[0]);

	elf = argv[1];
	cmd = argv[2];

	if ((fd = open(elf, O_RDONLY)) == -1)
		err(EXIT_FAILURE, "open failed for '%s'", elf);
	if (!(dwfl = dwfl_begin(&offline_callbacks)))
		errx(EXIT_FAILURE, "dwfl_begin failed");
	printdb(stdout, dwfl, fd);
	dwfl_end(dwfl);

	return EXIT_SUCCESS;
}
