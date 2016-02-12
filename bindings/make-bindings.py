#!/usr/bin/python
#
# This file is Copyright 2016, The Beanstalks Project ehf.
#
# This tool automatically creates bindings for other languages, based on
# the contents of pagekite.h.
#
# See boilerplate below. :-)
#
BOILERPLATE = """\
/* ****************************************************************************
   %(file)s - Wrappers for the libpagekite API

      * * *   WARNING: This file is auto-generated, do not edit!  * * *

*******************************************************************************
This file is Copyright 2012-%(year)s, The Beanstalks Project ehf.

This program is free software: you can redistribute it and/or modify it under
the terms  of the  Apache  License 2.0  as published by the  Apache  Software
Foundation.

This program is distributed in the hope that it will be useful,  but  WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE.  See the Apache License for more details.

You should have received a copy of the Apache License along with this program.
If not, see: <http://www.apache.org/licenses/>

Note: For alternate license terms, see the file COPYING.md.
**************************************************************************** */
"""
import datetime
import re
import subprocess
import sys


def uncomment(string):
    return re.sub(r'\/\*.+?\*\/', '', string, flags=re.DOTALL)


def java_ret_type(ret_type):
    ret_type = ret_type.strip()
    if ret_type in ('pagekite_mgr', 'void'):
        return 'boolean'
    elif ret_type.replace(' ', '') in ('char*', 'constchar*'):
        return 'String'
    elif ret_type == 'int':
        return 'int'
    else:
        raise ValueError('Unknown return value type: %s' % ret_type)


def java_name(func_name):
    return re.sub('_([a-z])',
                  lambda s: s.group(0)[1].upper(),
                  func_name[len('pagekite_'):].lower())


def java_arg(arg):
    var_type, var_name = uncomment(arg).rsplit(' ', 1)
    return '%s %s' % (java_ret_type(var_type), var_name)


def java_class(constants, functions):
    methods = []
    for ret_type, func_name, args, docs, argstr in functions:
        if args and args[0].startswith('pagekite_mgr'):
            args = args[1:]
        try:
            methods.append('public static native %s %s(%s);'
                           % (java_ret_type(ret_type),
                              java_name(func_name),
                              ', '.join(java_arg(a) for a in args)))
        except ValueError:
           methods.append('/* FIXME: %s%s(%s) */' % (ret_type, func_name,
                                                     ', '.join(args)))

    return """%(boilerplate)s\
package net.pagekite.lib;

public class PageKiteAPI extends Object
{
    %(constants)s

    %(methods)s

    static {
        System.loadLibrary("pagekite");
    }
}
""" % { 'boilerplate': BOILERPLATE % {
            'file': 'PageKiteAPI',
            'year': '%s' % datetime.datetime.now().year},
        'constants': '\n    '.join(('public static final int %s = %s;'
                                    % cpair) for cpair in constants),
        'methods': '\n    '.join(methods)}


def jni_ret_type(ret_type):
    return 'j%s' % java_ret_type(ret_type).lower()


def jni_arg(arg):
    var_type, var_name = uncomment(arg).rsplit(' ', 1)
    return '%s j%s' % (jni_ret_type(var_type), var_name)


def jni_fail(ret_type):
    return {
        'int': '-1',
        'boolean': 'JNI_FALSE',
        'String': 'NULL'
    }[java_ret_type(ret_type)];


def jni_func(ret_type, func_name, args):
    if args and args[0].startswith('pagekite_mgr'):
        set_global_manager = False
        args = args[1:]
    else:
        set_global_manager = True

    func = [
        '%s Java_net_pagekite_lib_PageKiteAPI_%s(' % (
            jni_ret_type(ret_type), java_name(func_name)),
        '  JNIEnv* env, jclass unused_class'
    ] + [', %s' % jni_arg(a) for a in args] + [
        '){'
    ]
    if set_global_manager:
        func += ['  if (pagekite_manager_global != NULL) return JNI_FALSE;']
    else:
        func += ['  if (pagekite_manager_global == NULL) return %s;'
                 % jni_fail(ret_type)]
    func += ['']

    clnp = []
    for arg in args:
        lspec = arg.replace('const ', '')
        lvar = lspec.split()[-1];
        jvar = jni_arg(arg).split()[-1]
        if lspec.replace(' ', '').startswith('char*'):
            func += ['  const jbyte* %s = NULL;' % lvar,
                     '  if (%s != NULL)'
                     ' %s = (*env)->GetStringUTFChars(env, %s, NULL);'
                     % (jvar, lvar, jvar)];
            clnp += ['  if (%s != NULL)'
                     ' (*env)->ReleaseStringUTFChars(env, %s, %s);'
                     % (jvar, jvar, lvar)];
        else:
            func += ['  %s = %s;' % (lspec, jvar)]

    if set_global_manager:
        fn, a = func_name, ', '.join(a.split()[-1] for a in args)
        func += ['', '  pagekite_manager_global = %s(%s);' % (fn, a), '']
        retn = ['  return (pagekite_manager_global != NULL);']
    else:
        fn, a = func_name, ', '.join(a.split()[-1] for a
                                     in ['pagekite_manager_global'] + args)
        func += ['', ('  %s rv = %s(%s);'
                      ) % (jni_ret_type(ret_type), fn, a), '']
        retn = ['  return rv;']

    func += clnp + retn + ['}']
    return '\n'.join(func)


def jni_code(functions):
    jni_functions = []
    for ret_type, func_name, args, docs, argstr in functions:
        try:
            jni_functions.append(jni_func(ret_type, func_name, args))
        except ValueError:
            jni_functions.append('/* FIXME: %s%s(%s) */'
                                 % (ret_type, func_name, ', '.join(args)))

    return """%(boilerplate)s\
#include "pagekite.h"
#include "pkcommon.h"

#ifdef HAVE_JNI_H
#include <jni.h>

static pagekite_mgr pagekite_manager_global = NULL;
%(constants)s

%(functions)s
#else
#  warning Java not found, not building JNI.
#endif
""" % { 'boilerplate': BOILERPLATE % {
            'file': 'pagekite-jni.c',
            'year': '%s' % datetime.datetime.now().year},
        'constants': '',
        'functions': '\n\n'.join(jni_functions)}

def get_constants(pagekite_h):
    constants = []
    for line in uncomment(pagekite_h).splitlines():
        try:
            if line.startswith('#define '):
                define, varname, value = line.split(' ', 2)
                if varname[:6] in ('PK_WIT', 'PK_AS_', 'PK_LOG'):
                    constants.append((varname, value.strip()))
        except ValueError:
            pass
    return constants


def get_functions(pagekite_h):
    functions = []
    func_re = re.compile(r'(?:\/\*(.*)\*\/\s+)?'
                          '(?:DECLSPEC_DLL\s+)?'
                          '((?:[^\(\s]+\s+)*)'
                          '(pagekite_\S+)'
                          '\((.*)\)$', flags=re.DOTALL)
    for statement in (s.strip() for s in pagekite_h.split(';')):
        m = re.match(func_re, statement)
        if m:
            docs, ret_type, func_name, argstr = (
                m.group(1), m.group(2), m.group(3), m.group(4))
            args = [a.strip() for a in uncomment(argstr).split(',')]
            if '_relay_' not in func_name and '_lua_' not in func_name:
                functions.append([ret_type, func_name, args, docs, argstr])
    return functions


if __name__ == '__main__':
    # FIXME: Would be better to use cpp here, and refactor our constants
    #        so they aren't just #defines.
    pagekite_h = subprocess.check_output(['cat', sys.argv[1]])
    constants = get_constants(pagekite_h)
    functions = get_functions(pagekite_h)
    for outfile in sys.argv[2:]:
        if outfile.endswith('.java'):
            open(outfile, 'w').write(java_class(constants, functions))
        elif outfile.endswith('.c'):
            open(outfile, 'w').write(jni_code(functions))
        else:
            raise ValueError('What is this? %s' % outfile)
