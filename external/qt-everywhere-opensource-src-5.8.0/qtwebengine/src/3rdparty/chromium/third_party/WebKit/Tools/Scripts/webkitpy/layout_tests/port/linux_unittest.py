# Copyright (C) 2011 Google Inc. All rights reserved.
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


from webkitpy.common.system import executive_mock
from webkitpy.common.system.systemhost_mock import MockSystemHost
from webkitpy.tool.mock_tool import MockOptions

from webkitpy.layout_tests.port import linux
from webkitpy.layout_tests.port import port_testcase


class LinuxPortTest(port_testcase.PortTestCase):
    os_name = 'linux'
    os_version = 'trusty'
    port_name = 'linux'
    full_port_name = 'linux-trusty'
    port_maker = linux.LinuxPort

    def assert_version_properties(self, port_name, os_version, expected_name,
                                  expected_version,
                                  driver_file_output=None):
        host = MockSystemHost(os_name=self.os_name, os_version=(os_version or self.os_version))
        host.filesystem.isfile = lambda x: 'content_shell' in x
        if driver_file_output:
            host.executive = executive_mock.MockExecutive2(driver_file_output)
        port = self.make_port(host=host, port_name=port_name, os_version=os_version)
        self.assertEqual(port.name(), expected_name)
        self.assertEqual(port.version(), expected_version)

    def test_versions(self):
        self.assertTrue(self.make_port().name() in ('linux-precise', 'linux-trusty'))

        self.assert_version_properties('linux', 'trusty', 'linux-trusty', 'trusty')
        self.assert_version_properties('linux', 'precise', 'linux-precise', 'precise')

        self.assert_version_properties('linux-trusty', None, 'linux-trusty', 'trusty')
        self.assert_version_properties('linux-precise', None, 'linux-precise', 'precise')
        self.assertRaises(AssertionError, self.assert_version_properties,
                          'linux-utopic', None, 'ignored', 'ignored', 'ignored')

    def assert_baseline_paths(self, port_name, os_version, *expected_paths):
        port = self.make_port(port_name=port_name, os_version=os_version)
        self.assertEqual(port.baseline_path(), port._webkit_baseline_path(expected_paths[0]))
        self.assertEqual(len(port.baseline_search_path()), len(expected_paths))
        for i, path in enumerate(expected_paths):
            self.assertTrue(port.baseline_search_path()[i].endswith(path))

    def test_baseline_paths(self):
        self.assert_baseline_paths('linux', 'trusty', 'linux', '/win')
        self.assert_baseline_paths('linux', 'precise', 'linux-precise', '/linux', '/win')

        self.assert_baseline_paths('linux-trusty', None, 'linux', '/win')
        self.assert_baseline_paths('linux-precise', None, 'linux-precise', '/linux', '/win')

    def test_check_illegal_port_names(self):
        # FIXME: Check that, for now, these are illegal port names.
        # Eventually we should be able to do the right thing here.
        self.assertRaises(AssertionError, linux.LinuxPort, MockSystemHost(), port_name='linux-x86')

    def test_operating_system(self):
        self.assertEqual('linux', self.make_port().operating_system())

    def test_build_path(self):
        # Test that optional paths are used regardless of whether they exist.
        options = MockOptions(configuration='Release', build_directory='/foo')
        self.assert_build_path(options, ['/mock-checkout/out/Release'], '/foo/Release')

        # Test that optional relative paths are returned unmodified.
        options = MockOptions(configuration='Release', build_directory='foo')
        self.assert_build_path(options, ['/mock-checkout/out/Release'], 'foo/Release')

    def test_driver_name_option(self):
        self.assertTrue(self.make_port()._path_to_driver().endswith('content_shell'))
        self.assertTrue(self.make_port(options=MockOptions(driver_name='OtherDriver'))._path_to_driver().endswith('OtherDriver'))

    def test_path_to_image_diff(self):
        self.assertEqual(self.make_port()._path_to_image_diff(), '/mock-checkout/out/Release/image_diff')
