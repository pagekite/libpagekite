SCRIPTNAME := cython_demo

PYTHON := python
PYVERSION := $(shell $(PYTHON) -c "import sys; print(sys.version[:3])")
CYTHON := cython

INCDIR := $(shell $(PYTHON) -c "from distutils import sysconfig; print(sysconfig.get_python_inc())")
PLATINCDIR := $(shell $(PYTHON) -c "from distutils import sysconfig; print(sysconfig.get_python_inc(plat_specific=True))")
LIBDIR1 := $(shell $(PYTHON) -c "from distutils import sysconfig; print(sysconfig.get_config_var('LIBDIR'))")
LIBDIR2 := $(shell $(PYTHON) -c "from distutils import sysconfig; print(sysconfig.get_config_var('LIBPL'))")
PYLIB := $(shell $(PYTHON) -c "from distutils import sysconfig; print(sysconfig.get_config_var('LIBRARY')[3:-2])")

CC := $(shell $(PYTHON) -c "import distutils.sysconfig; print(distutils.sysconfig.get_config_var('CC'))")
LINKCC := $(shell $(PYTHON) -c "import distutils.sysconfig; print(distutils.sysconfig.get_config_var('LINKCC'))")
LINKFORSHARED := $(shell $(PYTHON) -c "import distutils.sysconfig; print(distutils.sysconfig.get_config_var('LINKFORSHARED'))")
LIBS := $(shell $(PYTHON) -c "import distutils.sysconfig; print(distutils.sysconfig.get_config_var('LIBS'))")
SYSLIBS :=  $(shell $(PYTHON) -c "import distutils.sysconfig; print(distutils.sysconfig.get_config_var('SYSLIBS'))")

SCRIPTS = $(wildcard *.py)
SOURCES = $(SCRIPTS:.py=.c)
OBJECTS = $(SOURCES:.c=.o)

# prevent deletion of intermediary files
.SECONDARY:

$(SCRIPTNAME): $(OBJECTS)
	$(LINKCC) -o $@ $^ $(LDFLAGS) -L$(LIBDIR1) -L$(LIBDIR2) -l$(PYLIB) $(LIBS) $(SYSLIBS) $(LINKFORSHARED)

%.o: %.c
	$(CC) -c -o $@ $^ -Wall -fno-strict-aliasing $(CFLAGS) -I$(INCDIR) -I$(PLATINCDIR)

%.c: %.py
	$(CYTHON) $<

$(SCRIPTNAME).c: $(SCRIPTNAME).py
	$(CYTHON) --embed $<

.PHONY: all clean

all: $(SCRIPTNAME)

clean:
	@rm -vf *~ *.o *.so core core.* *.c $(SCRIPTNAME)
