#!/usr/bin/env python
# Copyright (C) 2012 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# Usage: make-file-arrays.py [--condition=condition-string] --out-h=<header-file-name> --out-cpp=<cpp-file-name> <input-file>...

import os.path
import re
import sys
from optparse import OptionParser

rjsmin_path = os.path.abspath(os.path.join(
        os.path.dirname(__file__),
        "..",
        "..",
        "build",
        "scripts"))
sys.path.append(rjsmin_path)
import rjsmin


def make_variable_name_and_read(file_name):
    result = re.match(r'([\w\d_]+)\.([\w\d_]+)', os.path.basename(file_name))
    if not result:
        print 'Invalid input file name:', os.path.basename(file_name)
        sys.exit(1)
    variable_name = result.group(1)[0].lower() + result.group(1)[1:] + result.group(2).capitalize()
    with open(file_name, 'rb') as f:
        content = f.read()
    return variable_name, content


def strip_whitespace_and_comments(file_name, content):
    result = re.match(r'.*\.([^.]+)', file_name)
    if not result:
        print 'The file name has no extension:', file_name
        sys.exit(1)
    extension = result.group(1).lower()
    multi_line_comment = re.compile(r'/\*.*?\*/', re.MULTILINE | re.DOTALL)
    # Don't accidentally match URLs (http://...)
    repeating_space = re.compile(r'[ \t]+', re.MULTILINE)
    leading_space = re.compile(r'^[ \t]+', re.MULTILINE)
    trailing_space = re.compile(r'[ \t]+$', re.MULTILINE)
    empty_line = re.compile(r'\n+')
    if extension == 'js':
        content = rjsmin.jsmin(content)
    elif extension == 'css':
        content = multi_line_comment.sub('', content)
        content = repeating_space.sub(' ', content)
        content = leading_space.sub('', content)
        content = trailing_space.sub('', content)
        content = empty_line.sub('\n', content)
    return content


def process_file(file_name):
    variable_name, content = make_variable_name_and_read(file_name)
    content = strip_whitespace_and_comments(file_name, content)
    size = len(content)
    return variable_name, content


def write_header_file(header_file_name, flag, names_and_contents, namespace):
    with open(header_file_name, 'w') as header_file:
        if flag:
            header_file.write('#if ' + flag + '\n')
        header_file.write('namespace %s {\n' % namespace)
        for variable_name, content in names_and_contents:
            size = len(content)
            header_file.write('extern const char %s[%d];\n' % (variable_name, size))
        header_file.write('}\n')
        if flag:
            header_file.write('#endif\n')


def write_cpp_file(cpp_file_name, flag, names_and_contents, header_file_name, namespace):
    with open(cpp_file_name, 'w') as cpp_file:
        cpp_file.write('#include "config.h"\n')
        cpp_file.write('#include "%s"\n' % os.path.basename(header_file_name))
        if flag:
            cpp_file.write('#if ' + flag + '\n')
        cpp_file.write('namespace %s {\n' % namespace)
        for variable_name, content in names_and_contents:
            cpp_file.write(cpp_constant_string(variable_name, content))
        cpp_file.write('}\n')
        if flag:
            cpp_file.write('#endif\n')


def cpp_constant_string(variable_name, content):
    output = []
    size = len(content)
    output.append('const char %s[%d] = {\n' % (variable_name, size))
    for index in range(size):
        char_code = ord(content[index])
        if char_code < 128:
            output.append('%d' % char_code)
        else:
            output.append(r"'\x%02x'" % char_code)
        output.append(',' if index != len(content) - 1 else '};\n')
        if index % 20 == 19:
            output.append('\n')
    output.append('\n')
    return ''.join(output)


def main():
    parser = OptionParser()
    parser.add_option('--out-h', dest='out_header')
    parser.add_option('--out-cpp', dest='out_cpp')
    parser.add_option('--condition', dest='flag')
    parser.add_option('--namespace', dest='namespace', default='blink')
    (options, args) = parser.parse_args()
    if len(args) < 1:
        parser.error('Need one or more input files')
    if not options.out_header:
        parser.error('Need to specify --out-h=filename')
    if not options.out_cpp:
        parser.error('Need to specify --out-cpp=filename')

    if options.flag:
        options.flag = options.flag.replace(' AND ', ' && ')
        options.flag = options.flag.replace(' OR ', ' || ')

    names_and_contents = [process_file(file_name) for file_name in args]

    if options.out_header:
        write_header_file(options.out_header, options.flag, names_and_contents, options.namespace)
    write_cpp_file(options.out_cpp, options.flag, names_and_contents, options.out_header, options.namespace)


if __name__ == '__main__':
    sys.exit(main())
