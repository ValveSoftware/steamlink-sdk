# Copyright (C) 2013 Adobe Systems Incorporated. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the above
#    copyright notice, this list of conditions and the following
#    disclaimer.
# 2. Redistributions in binary form must reproduce the above
#    copyright notice, this list of conditions and the following
#    disclaimer in the documentation and/or other materials
#    provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
# OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
# TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
# THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.

import optparse
import unittest

from webkitpy.common.host_mock import MockHost
from webkitpy.common.system.filesystem_mock import MockFileSystem
from webkitpy.common.system.executive_mock import MockExecutive2, ScriptError
from webkitpy.common.system.outputcapture import OutputCapture
from webkitpy.w3c.test_importer import TestImporter


FAKE_SOURCE_REPO_DIR = '/blink'

FAKE_FILES = {'/mock-checkout/third_party/Webkit/LayoutTests/w3c/OWNERS': '',
              '/blink/w3c/dir/has_shebang.txt': '#!',
              '/blink/w3c/dir/README.txt': '',
              '/blink/w3c/dir/OWNERS': '',
              '/blink/w3c/dir/reftest.list': '',
              '/blink/w3c/dir1/OWNERS': '',
              '/blink/w3c/dir1/reftest.list': '',
              '/mock-checkout/third_party/WebKit/LayoutTests/w3c/README.txt': '',
              '/mock-checkout/third_party/WebKit/LayoutTests/W3CImportExpectations': ''}


class TestImporterTest(unittest.TestCase):

    @staticmethod
    def options(**kwargs):
        """Returns a set of option values for TestImporter."""
        options = {
            "overwrite": False,
            "destination": "w3c",
            "ignore_expectations": False,
            "dry_run": False,
        }
        options.update(kwargs)
        return optparse.Values(options)

    def test_import_dir_with_no_tests(self):
        host = MockHost()
        host.executive = MockExecutive2(exception=ScriptError(
            "abort: no repository found in '/Volumes/Source/src/wk/Tools/Scripts/webkitpy/w3c'"))
        host.filesystem = MockFileSystem(files=FAKE_FILES)

        importer = TestImporter(host, FAKE_SOURCE_REPO_DIR, self.options())

        oc = OutputCapture()
        oc.capture_output()
        try:
            importer.do_import()
        finally:
            oc.restore_output()

    def test_path_too_long_true(self):
        importer = TestImporter(MockHost(), FAKE_SOURCE_REPO_DIR, self.options())
        self.assertTrue(importer.path_too_long(FAKE_SOURCE_REPO_DIR + '/' + ('x' * 150) + '.html'))

    def test_path_too_long_false(self):
        importer = TestImporter(MockHost(), FAKE_SOURCE_REPO_DIR, self.options())
        self.assertFalse(importer.path_too_long(FAKE_SOURCE_REPO_DIR + '/x.html'))

    def test_does_not_import_owner_files(self):
        host = MockHost()
        host.filesystem = MockFileSystem(files=FAKE_FILES)
        importer = TestImporter(host, FAKE_SOURCE_REPO_DIR, self.options())
        importer.find_importable_tests()
        self.assertEqual(importer.import_list,
                         [{'copy_list': [{'dest': 'has_shebang.txt', 'src': '/blink/w3c/dir/has_shebang.txt'}, {'dest': 'README.txt', 'src': '/blink/w3c/dir/README.txt'}],
                           'dirname': '/blink/w3c/dir',
                           'jstests': 0,
                           'reftests': 0,
                           'total_tests': 0}])

    def test_does_not_import_reftestlist_file(self):
        host = MockHost()
        host.filesystem = MockFileSystem(files=FAKE_FILES)
        importer = TestImporter(host, FAKE_SOURCE_REPO_DIR, self.options())
        importer.find_importable_tests()
        self.assertEqual(importer.import_list,
                         [{'copy_list': [{'dest': 'has_shebang.txt', 'src': '/blink/w3c/dir/has_shebang.txt'}, {'dest': 'README.txt', 'src': '/blink/w3c/dir/README.txt'}],
                           'dirname': '/blink/w3c/dir',
                           'jstests': 0,
                           'reftests': 0,
                           'total_tests': 0}])

    def test_executablebit(self):
        # executable source files are executable after importing
        host = MockHost()
        host.filesystem = MockFileSystem(files=FAKE_FILES)
        importer = TestImporter(host, FAKE_SOURCE_REPO_DIR, self.options())
        importer.do_import()
        self.assertEquals(host.filesystem.executable_files, set(['/mock-checkout/third_party/WebKit/LayoutTests/w3c/blink/w3c/dir/has_shebang.txt']))
