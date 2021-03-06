/* Previously, this code was also implemented libdwfl from elfutils.
 * Unfortunately, it turns out that libdwfl does seem to have some
 * problems with extracting source line information for complex RISC-V
 * binaries (e.g. those produced by RIOT). Furthermore, well-documented
 * C libraries for interacting with DWARF are hard to come by. For this
 * reason, this code now uses addr2line(1) from binutils and parses its
 * output.
 *
 * See: https://sourceware.org/bugzilla/show_bug.cgi?id=27805 */

#include <err.h>
#include <inttypes.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "dwarf.h"

/* Reading dwarf line information shouldn't require a cross-toolchain */
#define ADDR2LINE "addr2line -e"

static size_t
req_hex_digits(uint64_t addr)
{
	size_t r;

	r = 1;
	while (addr / 16 > 0) {
		addr /= 16;
		r++;
	}

	return r;
}

char *
dwarf_get_src(const char *elf, GElf_Addr addr)
{
	int r;
	FILE *p;
	size_t cmdlen;
	char *ret, *cmd, *colon;
	static char path[PATH_MAX + 1]; /* +1 for '\0' */

	/* +3 for 2x ' ' and 1x '\0' */
	cmdlen = strlen(ADDR2LINE) + strlen(elf) + req_hex_digits(addr) + 3;
	if (!(cmd = malloc(cmdlen)))
		err(EXIT_FAILURE, "malloc failed");

	r = snprintf(cmd, cmdlen, "%s %s %"PRIx64"", ADDR2LINE, elf, addr);
	if (r < 0)
		err(EXIT_FAILURE, "snprintf failed");
	else if ((size_t)r >= cmdlen)
		errx(EXIT_FAILURE, "buffer for snprintf too small");

	if (!(p = popen(cmd, "r")))
		err(EXIT_FAILURE, "popen failed");
	if (!fgets(path, sizeof(path), p))
		err(EXIT_FAILURE, "fgets failed");

	if ((r = pclose(p)) == -1)
		err(EXIT_FAILURE, "pclose failed");
	else if (r)
		errx(EXIT_FAILURE, "addr2line non-zero termination");

	if ((colon = strchr(path, ':')))
		*colon = '\0';
	else
		errx(EXIT_FAILURE, "unexpected addr2line output");

	if (!strcmp(path, "??")) {
		ret = NULL;
	} else {
		ret = path;
	}

	free(cmd);
	return ret;
}
