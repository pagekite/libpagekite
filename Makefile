default:
	@make native PK_TESTS=0

optimized:
	@make all OPT="-O3 -s" PK_TESTS=0

debug:
	@cd libpagekite && make tests PK_MEMORY_CANARIES=1 PK_TRACE=1 PK_TESTS=1
	@make all PK_MEMORY_CANARIES=1 PK_TRACE=1 PK_TESTS=1

debugwindows:
	@make windows PK_MEMORY_CANARIES=1 PK_TRACE=1 PK_TESTS=1

all: native windows

native:
	@echo '=== Building for '`uname`' ==='
	cd libpagekite && make
	@mv -v libpagekite/*.so lib/
	cd contrib/backends/ && make
	@mv -v contrib/backends/pagekitec bin/
	@echo
	@echo Note: To run the apps, you may need to do this first:
	@echo
	@echo  $$ export LD_LIBRARY_PATH=`pwd`/lib
	@echo

windows:
	@echo '=== Building for win32 ==='
	cd libpagekite && ../tools/bash.mxe -c 'make windows'
	@mv -v libpagekite/*.dll libpagekite/*.a lib/
	cd contrib/backends/ && ../../tools/bash.mxe -c 'make windows'
	@mv -v contrib/backends/*.exe bin/
	@echo

version:
	@cd libpagekite && make version

buildclean:
	@cd contrib && make clean
	@cd libpagekite && make clean

clean: buildclean
	rm -rf bin/* lib/*
