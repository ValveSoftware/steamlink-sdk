#!/usr/bin/env python
# Copyright (C) 2015 Google Inc. All rights reserved.
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
import make_runtime_features
import name_utilities
import template_expander


# We want exactly the same parsing as RuntimeFeatureWriter
# but generate different files.
class OriginTrialsWriter(make_runtime_features.RuntimeFeatureWriter):
    class_name = 'OriginTrials'

    def __init__(self, in_file_path):
        super(OriginTrialsWriter, self).__init__(in_file_path)
        self._outputs = {
            (self.class_name + '.cpp'): self.generate_implementation,
            (self.class_name + '.h'): self.generate_header,
        }

    @template_expander.use_jinja(class_name + '.cpp.tmpl')
    def generate_implementation(self):
        return {
            'features': self._features,
        }

    @template_expander.use_jinja(class_name + '.h.tmpl')
    def generate_header(self):
        return {
            'features': self._features,
        }


if __name__ == '__main__':
    in_generator.Maker(OriginTrialsWriter).main(sys.argv)
