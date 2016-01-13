#!/usr/bin/env python
# Copyright (C) 2013 Google Inc. All rights reserved.
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

import re
import sys

import in_generator
import name_utilities
from name_utilities import camelcase_property_name, lower_first
import template_expander


class StyleBuilderWriter(in_generator.Writer):
    class_name = 'StyleBuilder'
    filters = {
        'lower_first': lower_first,
    }

    valid_values = {
        'svg': [True, False],
        'font': [True, False],
        'custom_all': [True, False],
        'custom_initial': [True, False],
        'custom_inherit': [True, False],
        'custom_value': [True, False],
        'direction_aware': [True, False],
        'skip': [True, False],
    }
    defaults = {
        'name_for_methods': None,
        'use_handlers_for': None,
        'svg': False,
        'font': False,
        'converter': None,
        'direction_aware': False,
        'skip': False,
# These depend on property name by default
        'type_name': None,
        'getter': None,
        'setter': None,
        'initial': None,
# Setting these stops default handlers being generated
# Setting custom_all is the same as setting the other three
        'custom_all': False,
        'custom_initial': False,
        'custom_inherit': False,
        'custom_value': False,
    }

    def __init__(self, in_files):
        super(StyleBuilderWriter, self).__init__(in_files)
        self._outputs = {('StyleBuilderFunctions.h'): self.generate_style_builder_functions_h,
                         ('StyleBuilderFunctions.cpp'): self.generate_style_builder_functions_cpp,
                         ('StyleBuilder.cpp'): self.generate_style_builder,
                        }

        self._properties = self.in_file.name_dictionaries

        def set_if_none(property, key, value):
            if property[key] is None:
                property[key] = value

        for property in self._properties:
            cc = camelcase_property_name(property['name'])
            property['property_id'] = 'CSSProperty' + cc
            cc = property['name_for_methods'] or cc.replace('Webkit', '')
            property['camel_case_name'] = cc
            set_if_none(property, 'type_name', 'E' + cc)
            set_if_none(property, 'getter', lower_first(cc))
            set_if_none(property, 'setter', 'set' + cc)
            set_if_none(property, 'initial', 'initial' + cc)
            if property['custom_all']:
                property['custom_initial'] = True
                property['custom_inherit'] = True
                property['custom_value'] = True
            property['should_declare_functions'] = not property['use_handlers_for'] and not property['direction_aware'] and not property['skip']

        self._properties = dict((property['property_id'], property) for property in self._properties)

    @template_expander.use_jinja('StyleBuilderFunctions.h.tmpl',
                                 filters=filters)
    def generate_style_builder_functions_h(self):
        return {
            'properties': self._properties,
        }

    @template_expander.use_jinja('StyleBuilderFunctions.cpp.tmpl',
                                 filters=filters)
    def generate_style_builder_functions_cpp(self):
        return {
            'properties': self._properties,
        }

    @template_expander.use_jinja('StyleBuilder.cpp.tmpl', filters=filters)
    def generate_style_builder(self):
        return {
            'properties': self._properties,
        }


if __name__ == '__main__':
    in_generator.Maker(StyleBuilderWriter).main(sys.argv)
