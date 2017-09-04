# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# pylint: disable=import-error,print-statement,relative-import

"""Generates Blink Web Module bindings.

The Blink Web Module bindings provide a stable, IDL-generated interface for the
Web Modules.

The Web Modules are the high-level services like Autofill,
Autocomplete, Translate, Distiller, Phishing Detector, and others. Web Modules
typically want to introspec the document and rendering infromation to implement
browser features.

The bindings are meant to be as simple and as ephemeral as possible, mostly just
wrapping existing DOM classes. Their primary goal is to avoid leaking the actual
DOM classes to the Web Modules layer.
"""

import os
import posixpath

from code_generator import CodeGeneratorBase
# TODO(dglazkov): Move TypedefResolver to code_generator.py
from code_generator_v8 import TypedefResolver
from v8_interface import interface_context

MODULE_PYNAME = os.path.splitext(os.path.basename(__file__))[0] + '.py'

WEB_MODULE_IDL_ATTRIBUTE = 'WebModuleAPI'


class CodeGeneratorWebModule(CodeGeneratorBase):
    def __init__(self, info_provider, cache_dir, output_dir):
        CodeGeneratorBase.__init__(self, MODULE_PYNAME, info_provider,
                                   cache_dir, output_dir)
        self.typedef_resolver = TypedefResolver(info_provider)

    def get_template(self):
        template_filename = 'web_module_interface.cpp.tmpl'
        return self.jinja_env.get_template(template_filename)

    # TODO(dglazkov): Move to CodeGeneratorBase.
    def output_paths(self, definition_name):
        header_path = posixpath.join(self.output_dir,
                                     'Web%s.h' % definition_name)
        cpp_path = posixpath.join(self.output_dir,
                                  'Web%s.cpp' % definition_name)
        return header_path, cpp_path

    def generate_code(self, definitions, definition_name):
        self.typedef_resolver.resolve(definitions, definition_name)
        header_path, cpp_path = self.output_paths(definition_name)

        template_context = {}
        # TODO(dglazkov): Implement dictionaries
        if definition_name not in definitions.interfaces:
            return None

        interface = definitions.interfaces[definition_name]
        if WEB_MODULE_IDL_ATTRIBUTE not in interface.extended_attributes:
            return None

        # TODO(dglazkov): Implement callback interfaces.
        # TODO(dglazkov): Implement partial interfaces.
        if interface.is_callback and interface.is_partial:
            return None

        template_context = interface_context(interface,
                                             definitions.interfaces)
        template_context['code_generator'] = MODULE_PYNAME
        template_context['class_name'] = definition_name

        cpp_template = self.get_template()
        cpp_text = cpp_template.render(template_context)

        header_text = 'header'
        return (
            (header_path, header_text),
            (cpp_path, cpp_text)
        )
