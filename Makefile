default:
	@$(MAKE) native PK_TESTS=0

optimized:
	@$(MAKE) all OPT="-O3 -s" PK_TESTS=0

debug:
	@cd libpagekite && $(MAKE) tests PK_MEMORY_CANARIES=1 PK_TRACE=1 PK_TESTS=1
	@$(MAKE) all PK_MEMORY_CANARIES=1 PK_TRACE=1 PK_TESTS=1

debugwindows:
	@$(MAKE) windows PK_MEMORY_CANARIES=1 PK_TRACE=1 PK_TESTS=1

all: native windows

native:
	@echo '=== Building for '`uname`' ==='
	$(MAKE) -C libpagekite
	@mv -v libpagekite/*.so lib/
	$(MAKE) -C contrib/backends
	@mv -v contrib/backends/pagekitec bin/
	@echo
	@echo Note: To run the apps, you may need to do this first:
	@echo
	@echo  $$ export LD_LIBRARY_PATH=`pwd`/lib
	@echo

windows:
	@echo '=== Building for win32 ==='
	../tools/bash.mxe -c '$(MAKE) -C libpagekite windows'
	@mv -v libpagekite/*.dll libpagekite/*.a lib/
	../../tools/bash.mxe -c '$(MAKE) -C contrib/backends windows'
	@mv -v contrib/backends/*.exe bin/
	@echo

version:
	@$(MAKE) -C libpagekite version

buildclean:
	@$(MAKE) -C contrib clean
	@$(MAKE) -C libpagekite clean

clean: buildclean
	rm -rf bin/* lib/*
