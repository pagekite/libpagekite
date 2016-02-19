#!/usr/bin/python
#
# This file is Copyright 2016, The Beanstalks Project ehf.
#
# This tool automatically creates language bindings and documentation, based
# on the contents of pagekite.h.
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
import traceback


def disemvowel(string):
    return re.sub(r'[aeiouy_]', '', string)


def wraplines(string, indent='', breaklen=60):
    ll = re.compile('([^\n]{%d}\S+) +' % breaklen, flags=re.DOTALL)
    while re.search(ll, string):
        string = re.sub(ll, '\\1\n' + indent, string)
    return indent + string


def uncomment(string):
    return re.sub(r'\/\*.+?\*\/', '', string, flags=re.DOTALL)


def comment(string, default=''):
    cmnt = re.search(r'\/\*(.+?)\*\/', string, flags=re.DOTALL)
    if cmnt:
        return cmnt.group(1).strip()
    else:
        return default


def java_ret_type(ret_type):
    ret_type = ret_type.strip()
    if ret_type in ('pagekite_mgr', 'void'):
        return 'boolean'
    elif ret_type.replace(' ', '') in ('char*', 'constchar*'):
        return 'String'
    elif ret_type in ('int', 'pk_neg_fail'):
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


def java_const(cpair):
    name, val = cpair
    if '"' in val:
        return 'public static final String %s = %s;' % (name, val)
    else:
        return 'public static final int %s = %s;' % (name, val)


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

    return '\n'.join([
        BOILERPLATE % {
            'file': 'PageKiteAPI',
            'year': '%s' % datetime.datetime.now().year},
        'package net.pagekite.lib;',
        '',
        'public class PageKiteAPI extends Object',
        '{',
        '    ' + '\n    '.join(java_const(cpair) for cpair in constants),
        '',
        '    ' + '\n    '.join(methods),
        '',
        '    static {',
        '        System.loadLibrary("pagekite");',
        '    }',
        '}'])


def jni_ret_type(ret_type):
    return 'j%s' % java_ret_type(ret_type).lower()


def jni_arg(arg):
    var_type, var_name = uncomment(arg).rsplit(' ', 1)
    return '%s j%s' % (jni_ret_type(var_type), var_name)


def jni_fail(ret_type):
    return {
        'int': '-1',
        'pk_neg_fail': '-1',
        'boolean': 'JNI_FALSE',
        'String': 'NULL'}[java_ret_type(ret_type)];


def jni_func(ret_type, func_name, args):
    if args and args[0].startswith('pagekite_mgr'):
        set_global_manager = False
        args = args[1:]
    else:
        set_global_manager = True

    func = [
        '%s Java_net_pagekite_lib_PageKiteAPI_%s(' % (
            jni_ret_type(ret_type), java_name(func_name)),
        '  JNIEnv* env, jclass unused_class'] + [
        ', %s' % jni_arg(a) for a in args] + [
        '){']
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

    return '\n'.join([
        BOILERPLATE % {
            'file': 'pagekite-jni.c',
            'year': '%s' % datetime.datetime.now().year},
        '#include "pagekite.h"',
        '#include "pkcommon.h"',
        '',
        '#ifdef HAVE_JNI_H',
        '#include <jni.h>',
        '',
        'static pagekite_mgr pagekite_manager_global = NULL;',
        '',
        '\n\n'.join(jni_functions),
        '#else',
        '#  warning Java not found, not building JNI.',
        '#endif'])


def parse_doc_comment(docs):
    if docs is None:
        return {}
    lines = re.sub(r'(\n+)  +',
                   lambda s: ' ' * (1 if (len(s.group(1)) == 1) else 2),
                   '\n'.join([re.sub(r'^[\/\s]+\*([\/\s]|$)', '', l)
                              for l in docs.splitlines()]),
                   flags=re.DOTALL)
    return dict(l.replace('  ', '\n\n').split(': ', 1)
                for l in lines.splitlines() if l.strip())


def doc_section(parse):
    if parse:
        return [k for k in parse.keys() if not 'returns' in k.lower()][0]
    else:
        return 'None'


def doc_args(args, argstr, jni=False):
    adocs = []
    if args:
        comments = argstr.strip().splitlines()
        for i in range(0, len(args)):
            adoc = comment(comments[i])
            argn = args[i]
            if jni:
                if argn.startswith('pagekite_mgr'):
                    continue
                argn = java_arg(argn)
            adocs.append('   * `%s`: %s' % (argn, adoc))
    return adocs


def python_ret_type(ret_type):
    ret_type = ret_type.strip()
    if ret_type in ('pagekite_mgr',):
        return 'c_void_p'
    elif ret_type.replace(' ', '') in ('char*', 'constchar*'):
        return 'c_char_p'
    elif ret_type in ('int', 'pk_neg_fail'):
        return 'c_int'
    else:
        raise ValueError('Unknown return value type: %s' % ret_type)


def python_name(func_name):
    return func_name[len('pagekite_'):].lower()


def python_arg_type(arg):
    if ' ' in arg:
        arg = arg.rsplit(' ', 1)[0]
    return python_ret_type(arg)


def python_const(cpair):
    return '%s = %s' % cpair


def python_code(constants, functions):
    retpref = 'API Returns'

    def recomment(text):
        commented = '\n'.join('# %s' % l for l in comment(text).splitlines())
        return re.sub(r'\s+\*{40,}', '#' * 78, commented)

    funcs, func_info = [], {}
    for ret_type, func_name, args, docs, argstr in functions:
        try:
            func_name = python_name(func_name)
            parse = parse_doc_comment(docs)
            doc = ['    ' + l for l in
                   wraplines(parse.get(doc_section(parse), ''),
                             breaklen=52).strip().splitlines()]
            if not doc[0]:
                continue

            is_init = (args and not args[0].strip().startswith('pagekite_mgr'))

            if args:
                adocs = doc_args(args, argstr)
                if not is_init:
                    adocs = adocs[1:]
                if adocs:
                    doc += ['']
                    doc += ['    Args:']
                    doc += ['    ' + d for d in adocs]
            doc += ['']
            doc += ['    Returns:']

            if is_init:
                returns = 'The PageKite object on success, None otherwise'
                doc += ['        ' + returns]
                wrapper = [
                   'def %s(self, *args):' % func_name,
                   '    """'] + doc + [
                   '    """',
                   '    assert(self.pkm is None)',
                   '    self.pkm = self.dll.pagekite_%s(*args)' % func_name,
                   '    return (self if (self.pkm) else None)']
            else:
                returns = parse.get(retpref, parse.get('Returns', ret_type))
                doc += ['        ' + returns]
                wrapper = [
                   'def %s(self, *args):' % func_name,
                   '    """'] + doc + [
                   '    """',
                   '    assert(self.pkm is not None)',
                   '    return self.dll.pagekite_%s(self.pkm, *args)' % func_name]

            func_info[func_name] = {
                'name': func_name,
                'ret_type': python_ret_type(ret_type),
                'args': ', '.join(python_arg_type(a) for a in (args or [])),
                'wrapper': wrapper
            }
            funcs.append(func_name)

        except ValueError:
            traceback.print_exc()
            pass

    return '\n'.join([
        recomment(BOILERPLATE % {
            'file': 'libpagekite-Python',
            'year': '%s' % datetime.datetime.now().year}),
        '', '',
        '\n'.join(python_const(cpair) for cpair in constants),
        '', '',
        'def get_libpagekite_cdll():',
        '    """Fetch a fully configured ctypes.cdll object for libpagekite."""',
        '    from ctypes import cdll, c_void_p, c_char_p, c_int',
        '    dll = cdll.LoadLibrary("libpagekite.so")',
        '    for restype, func_name, argtypes in (',
        '            ' + ',\n            '.join(
             '(%(ret_type)s, "%(name)s", (%(args)s,))'
             % func_info[f] for f in funcs) + '):',
        '        method = getattr(dll, "pagekite_%s" % func_name)',
        '        method.restype = restype',
        '        method.argtypes = argtypes',
        '    return dll',
        '', '',
        'class PageKite(object):',
        '    def __init__(self):',
        '        self.dll = dll = get_libpagekite_cdll()',
        '        self.pkm = pkm = None',
        '        def cleanup():',
        '            try:',
        '                pkm.thread_stop()',
        '                pkm.free()',
        '            except (AssertionError, AttributeError):',
        '                pass',
        '        setattr(self, "cleanup", cleanup)',
        '        setattr(self, "__del__", cleanup)',
        '',
        '    def __exit__(self, *args): return self.cleanup()',
        '    def __enter__(self): return self',
        '',
        '\n\n'.join('    ' + '\n    '.join(func_info[f]['wrapper'])
                    for f in funcs),
        ''])


def documentation(constants, functions, jni=False):
    toc, constant_docs, function_docs = [], [], []

    last_sect = 'None'
    retpref = 'JNI Returns' if jni else 'API Returns'
    for ret_type, func_name, args, docs, argstr in functions:
        try:
            if jni:
                func_name = java_name(func_name)
                ret_type = java_ret_type(ret_type)

            parse = parse_doc_comment(docs)
            text = wraplines(parse.get(doc_section(parse), '')).strip()
            if not text:
                continue

            sect = doc_section(parse).strip()
            if sect != last_sect:
                doc = ['### %s' % sect]
            else:
                doc = []

            doc += [
                '',
                '<a %76s' % (' name="%s"><hr></a>' % disemvowel(func_name)),
                '',
                '#### `%s %s(...)`' % (ret_type.strip(), func_name),
                '',
                text,
                '']

            if args:
                adocs = doc_args(args, argstr, jni=jni)
                doc += ['**Arguments**:', '', '\n'.join(adocs), '']

            doc.append(
                '**Returns**: %s' % parse.get(retpref,
                                              parse.get('Returns', ret_type)))

            function_docs.append('\n'.join(doc))

            if sect != last_sect:
                toc.append('   * %s' % sect)
                last_sect = sect
            toc.append('      * [`%-44s`](#%s)'
                       % (func_name, disemvowel(func_name)))
        except (ValueError, KeyError):
            traceback.print_exc()
            pass

    toc.append('   * [Constants](#constants)')
    if jni:
        constant_docs = ['PageKiteAPI.%s = %s  ' % cpair for cpair in constants]
    else:
        constant_docs = ['%s = %s  ' % cpair for cpair in constants]

    return '\n'.join([
        '# PageKite API reference manual',
        '',
        '\n'.join(toc),
        '',
        '## Functions',
        '',
        '\n\n'.join(function_docs),
        '',
        '<a %76s' % (' name="constants"><hr></a>'),
        '',
        '## Constants',
        '',
        '\n'.join(constant_docs)])


def get_constants(pagekite_h):
    constants = []
    lastvarname = 'None'
    for line in uncomment(pagekite_h).splitlines():
        try:
            if line.startswith('#define '):
                define, varname, value = line.split(' ', 2)
                if varname[:6] in ('PK_WIT', 'PK_AS_', 'PK_STA',
                                   'PK_LOG', 'PK_VER'):
                    if varname == lastvarname:
                        constants[-1] = (varname, value.strip())
                    else:
                        constants.append((varname, value.strip()))
                        lastvarname = varname
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
            if '_relay_' not in func_name:
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
        elif outfile.endswith('.py'):
            open(outfile, 'w').write(python_code(constants, functions))
        elif outfile.endswith('.md'):
            open(outfile, 'w').write(documentation(
                constants, functions, jni='jni' in outfile.lower()))
        else:
            raise ValueError('What is this? %s' % outfile)
