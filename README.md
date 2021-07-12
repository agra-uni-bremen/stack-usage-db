# stack-usage-db

Generate a single file containing GCC `-fstack-usage` information for
all function symbols defined in an ELF file.

## Motivation

Recont versions of GCC allow generating per-function stack usage
information using the `-fstack-usage` compiler flag. Unfortunately, gcc
stores this information on a per compilation-unit basis. This creates
issue if one considers a linked ELF file and wants to determine the
stack usage of a given function symbol in this ELF file. As an example,
static function may be declared with the same name across different
compilation units.

## Approach

To resolve the problem outlined above. This tool concatenates multiple
`-fstack-usage` files into a single file/database. The tool iterates
over a given ELF file and extracts the source line information for each
function symbol. The source file path is then passed to a user-supplied
script which returns the stack-usage path for a given source file path.
The stack-usage file is then parsed and the function symbol address is
associated with the stack size for this function as determined by the
stack-usage file.

## Usage

A sample script is provided in `convert.sh`. This tool requires
`libdwfl` from [elfutils](https://sourceware.org/elfutils/) and can be
compiled using:

	$ make

Afterwards, compile the example and run `stack-usage-db` as follows:
Afterwards, execute as follows:

	$ make -C ./example
	$ ./stack-usage-db ./example/main ./convert.sh

Which will generate the following tab-separated output:

	1009c	get_mtime	32
	10118	get_mtime	16
	10074	main	32
	10134	myfunc2	48
	100e4	myfunc1	32
