# Copyright (C) 2016 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#    * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#    * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#    * Neither the name of Google Inc. nor the names of its
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

import unittest

from webkitpy.layout_tests.builder_list import BuilderList


class BuilderListTest(unittest.TestCase):

    @staticmethod
    def sample_builder_list():
        return BuilderList({
            'Blink A': {'port_name': 'port-a', 'specifiers': ['A', 'Release']},
            'Blink B': {'port_name': 'port-b', 'specifiers': ['B', 'Release']},
            'Blink B (dbg)': {'port_name': 'port-b', 'specifiers': ['B', 'Debug']},
            'Blink C (dbg)': {'port_name': 'port-c', 'specifiers': ['C', 'Release']},
            'Try A': {'port_name': 'port-a', 'specifiers': ['A', 'Release'], "is_try_builder": True},
            'Try B': {'port_name': 'port-b', 'specifiers': ['B', 'Release'], "is_try_builder": True},
        })

    def test_all_builder_names(self):
        b = self.sample_builder_list()
        self.assertEqual(['Blink A', 'Blink B', 'Blink B (dbg)', 'Blink C (dbg)', 'Try A', 'Try B'], b.all_builder_names())

    def test_all_continuous_builder_names(self):
        b = self.sample_builder_list()
        self.assertEqual(['Blink A', 'Blink B', 'Blink B (dbg)', 'Blink C (dbg)'], b.all_continuous_builder_names())

    def test_all_port_names(self):
        b = self.sample_builder_list()
        self.assertEqual(['port-a', 'port-b', 'port-c'], b.all_port_names())

    def test_port_name_for_builder_name(self):
        b = self.sample_builder_list()
        self.assertEqual('port-b', b.port_name_for_builder_name('Blink B'))

    def test_specifiers_for_builder(self):
        b = self.sample_builder_list()
        self.assertEqual(['B', 'Release'], b.specifiers_for_builder('Blink B'))

    def test_port_name_for_builder_name_with_missing_builder(self):
        b = self.sample_builder_list()
        with self.assertRaises(KeyError):
            b.port_name_for_builder_name('Blink_B')

    def test_specifiers_for_builder_with_missing_builder(self):
        b = self.sample_builder_list()
        with self.assertRaises(KeyError):
            b.specifiers_for_builder('Blink_B')

    def test_builder_name_for_port_name_with_no_debug_builder(self):
        b = self.sample_builder_list()
        self.assertEqual('Blink A', b.builder_name_for_port_name('port-a'))

    def test_builder_name_for_port_name_with_debug_builder(self):
        b = self.sample_builder_list()
        self.assertEqual('Blink B', b.builder_name_for_port_name('port-b'))

    def test_builder_name_for_port_name_with_only_debug_builder(self):
        b = self.sample_builder_list()
        self.assertEqual('Blink C (dbg)', b.builder_name_for_port_name('port-c'))
