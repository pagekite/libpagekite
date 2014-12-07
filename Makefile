optimized:
	@make all OPT="-O3 -s"

debug:
	@make all PK_MEMORY_CANARIES=1 PK_TRACE=1

all: native windows

native: buildclean
	@echo '=== Building for '`uname`' ==='
	@cd libpagekite && make
	@mv -v libpagekite/*.so lib/
	@mv -v libpagekite/pagekitec bin/
	@echo

windows: buildclean
	@echo '=== Building for win32 ==='
	@cd libpagekite && ../tools/bash.mxe -c 'make pagekitec.exe'
	@mv -v libpagekite/*.{dll,a} lib
	@mv -v libpagekite/*.exe bin
	@echo

buildclean:
	@cd libpagekite && make clean

clean: buildclean
	rm -rf bin/* lib/*
