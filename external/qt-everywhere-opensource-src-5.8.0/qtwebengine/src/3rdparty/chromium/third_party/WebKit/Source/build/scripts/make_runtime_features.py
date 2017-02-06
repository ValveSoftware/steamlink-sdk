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

import sys

import in_generator
import name_utilities
from name_utilities import lower_first
import template_expander


class RuntimeFeatureWriter(in_generator.Writer):
    class_name = 'RuntimeEnabledFeatures'

    # FIXME: valid_values and defaults should probably roll into one object.
    valid_values = {
        'status': ['stable', 'experimental', 'test'],
    }
    defaults = {
        'origin_trial_feature_name': None,
        'condition': None,
        'implied_by': [],
        'depends_on': [],
        'custom': False,
        'status': None,
        'settable_from_internals': False,
    }

    def __init__(self, in_file_path):
        super(RuntimeFeatureWriter, self).__init__(in_file_path)
        self._outputs = {(self.class_name + '.h'): self.generate_header,
                         (self.class_name + '.cpp'): self.generate_implementation,
                        }

        self._features = self.in_file.name_dictionaries
        # Make sure the resulting dictionaries have all the keys we expect.
        for feature in self._features:
            feature['first_lowered_name'] = lower_first(feature['name'])
            # Most features just check their isFooEnabled bool
            # but some depend on or are implied by other bools.
            enabled_condition = 'is%sEnabled' % feature['name']
            assert not feature['implied_by'] or not feature['depends_on'], 'Only one of implied_by and depends_on is allowed'
            for implied_by_name in feature['implied_by']:
                enabled_condition += ' || is%sEnabled' % implied_by_name
            for dependant_name in feature['depends_on']:
                enabled_condition += ' && is%sEnabled' % dependant_name
            feature['enabled_condition'] = enabled_condition
        self._standard_features = [feature for feature in self._features if not feature['custom']]

    def _feature_sets(self):
        # Another way to think of the status levels is as "sets of features"
        # which is how we're referring to them in this generator.
        return list(self.valid_values['status'])

    def _template_inputs(self):
        return {
            'features': self._features,
            'feature_sets': self._feature_sets(),
            'standard_features': self._standard_features,
        }

    @template_expander.use_jinja(class_name + '.h.tmpl')
    def generate_header(self):
        return self._template_inputs()

    @template_expander.use_jinja(class_name + '.cpp.tmpl')
    def generate_implementation(self):
        return self._template_inputs()


if __name__ == '__main__':
    in_generator.Maker(RuntimeFeatureWriter).main(sys.argv)
