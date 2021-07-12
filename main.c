#include <err.h>
#include <libgen.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include <elfutils/libdwfl.h>

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
			err(EXIT_FAILURE, "fdopen failed");

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
getsu(Dwfl *dwfl, GElf_Addr addr)
{
	Dwfl_Line *line;
	const char *srcfp;

	line = dwfl_getsrc(dwfl, addr);
	if (!(srcfp = dwfl_lineinfo(line, NULL, NULL, NULL, NULL, NULL)))
		return NULL;

	return src2su(srcfp);
}

static void
printdb(Dwfl *dwfl, int fd)
{
	int i, n;
	Dwfl_Module *mod;

	dwfl_report_begin(dwfl);
	if (!(mod = dwfl_report_offline(dwfl, "main", "main", fd)))
		errx(EXIT_FAILURE, "dwfl_report_offline failed");

	if ((n = dwfl_module_getsymtab(mod)) == -1)
		errx(EXIT_FAILURE, "dwfl_module_getsymtab failed");

	for (i = 0; i < n; i++) {
		GElf_Sym sym;
		GElf_Addr addr;
		const char *name;
		const char *sufp;

		name = dwfl_module_getsym_info(mod, i, &sym, &addr, NULL, NULL, NULL);
		if (!name || GELF_ST_TYPE(sym.st_info) != STT_FUNC)
			continue; /* not a function symbol */

		if (!(sufp = getsu(dwfl, addr))) {
			warnx("no line information for symbol '%s'", name);
			continue;
		}

		printf("sufp: %s\n", sufp);
	}

	dwfl_report_end(dwfl, NULL, NULL);
}

int
main(int argc, char **argv)
{
	int fd;
	char *elf;
	Dwfl *dwfl;

	if (argc < 3)
		usage(argv[0]);

	elf = argv[1];
	cmd = argv[2];

	if ((fd = open(elf, O_RDONLY)) == -1)
		err(EXIT_FAILURE, "open failed");
	if (!(dwfl = dwfl_begin(&offline_callbacks)))
		errx(EXIT_FAILURE, "dwfl_begin failed");
	printdb(dwfl, fd);
	dwfl_end(dwfl);

	return EXIT_SUCCESS;
}
