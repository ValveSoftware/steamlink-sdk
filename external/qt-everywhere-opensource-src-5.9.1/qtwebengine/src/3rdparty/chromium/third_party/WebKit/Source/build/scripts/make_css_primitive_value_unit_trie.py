#!/usr/bin/env python
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys

import in_generator
import trie_builder
import template_expander


class UnitTrieWriter(in_generator.Writer):
    defaults = {
        'unit_type': None
    }

    def __init__(self, in_file_paths):
        super(UnitTrieWriter, self).__init__(in_file_paths)

        self._units = {entry['name']: entry['unit_type'] for entry in self.in_file.name_dictionaries}

        self._outputs = {
            'CSSPrimitiveValueUnitTrie.cpp': self.generate_implementation
        }

    @template_expander.use_jinja('CSSPrimitiveValueUnitTrie.cpp.tmpl')
    def generate_implementation(self):
        return {
            'length_tries': trie_builder.trie_list_by_str_length(self._units)
        }


if __name__ == '__main__':
    in_generator.Maker(UnitTrieWriter).main(sys.argv)
