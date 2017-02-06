#!/usr/bin/env python
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys

import css_properties
import in_generator
from name_utilities import enum_for_css_keyword
import template_expander


class CSSOMTypesWriter(css_properties.CSSProperties):
    def __init__(self, in_file_path):
        super(CSSOMTypesWriter, self).__init__(in_file_path)

        for property in self._properties.values():
            types = []
            # Expand types
            for singleType in property['typedom_types']:
                if singleType == 'Length':
                    types.append('SimpleLength')
                    types.append('CalcLength')
                else:
                    types.append(singleType)
            property['typedom_types'] = types

            # Generate Keyword ID values from keywords.
            property['keywordIDs'] = map(
                enum_for_css_keyword, property['keywords'])

        self._outputs = {
            'CSSOMTypes.cpp': self.generate_types,
            'CSSOMKeywords.cpp': self.generate_keywords,
        }

    @template_expander.use_jinja('CSSOMTypes.cpp.tmpl')
    def generate_types(self):
        return {
            'properties': self._properties,
        }

    @template_expander.use_jinja('CSSOMKeywords.cpp.tmpl')
    def generate_keywords(self):
        return {
            'properties': self._properties,
        }

if __name__ == '__main__':
    in_generator.Maker(CSSOMTypesWriter).main(sys.argv)
