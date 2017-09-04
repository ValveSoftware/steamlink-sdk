#!/usr/bin/env python
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from collections import namedtuple
import math
import sys

import in_generator
import template_expander
import make_style_builder


class ComputedStyleBaseWriter(make_style_builder.StyleBuilderWriter):
    def __init__(self, in_file_path):
        super(ComputedStyleBaseWriter, self).__init__(in_file_path)
        self._outputs = {
            'ComputedStyleBase.h': self.generate_base_computed_style_h,
            'ComputedStyleBase.cpp': self.generate_base_computed_style_cpp,
            'ComputedStyleBaseConstants.h': self.generate_base_computed_style_constants,
        }

        # A map of enum name -> list of enum values
        self._computed_enums = {}
        for property in self._properties.values():
            if property['keyword_only']:
                enum_name = property['type_name']
                # From the Blink style guide: Enum members should use InterCaps with an initial capital letter. [names-enum-members]
                enum_values = [k.title() for k in property['keywords']]
                self._computed_enums[enum_name] = enum_values

        # A list of fields
        Field = namedtuple('Field', [
            # Name of field member variable
            'name',
            # Property field is for
            'property',
            # Field storage type
            'type',
            # Bits needed for storage
            'size',
            # Default value for field
            'default_value',
            # Method names
            'getter_method_name',
            'setter_method_name',
            'initial_method_name',
            'resetter_method_name',
        ])
        self._fields = []
        for property in self._properties.values():
            if property['keyword_only']:
                property_name = property['upper_camel_name']
                if property['name_for_methods']:
                    property_name = property['name_for_methods']
                property_name_lower = property_name[0].lower() + property_name[1:]

                # From the Blink style guide: Other data members should be prefixed by "m_". [names-data-members]
                field_name = 'm_' + property_name_lower
                bits_needed = math.log(len(property['keywords']), 2)
                type_name = property['type_name']
                # For now, assume the default value is the first enum value.
                default_value = type_name + '::' + self._computed_enums[type_name][0]

                self._fields.append(Field(
                    name=field_name,
                    property=property,
                    type=type_name,
                    size=int(math.ceil(bits_needed)),
                    default_value=default_value,
                    getter_method_name=property_name_lower,
                    setter_method_name='set' + property_name,
                    initial_method_name='initial' + property_name,
                    resetter_method_name='reset' + property_name,
                ))

    @template_expander.use_jinja('ComputedStyleBase.h.tmpl')
    def generate_base_computed_style_h(self):
        return {
            'properties': self._properties,
            'enums': self._computed_enums,
            'fields': self._fields,
        }

    @template_expander.use_jinja('ComputedStyleBase.cpp.tmpl')
    def generate_base_computed_style_cpp(self):
        return {
            'properties': self._properties,
            'enums': self._computed_enums,
            'fields': self._fields,
        }

    @template_expander.use_jinja('ComputedStyleBaseConstants.h.tmpl')
    def generate_base_computed_style_constants(self):
        return {
            'properties': self._properties,
            'enums': self._computed_enums,
            'fields': self._fields,
        }

if __name__ == '__main__':
    in_generator.Maker(ComputedStyleBaseWriter).main(sys.argv)
