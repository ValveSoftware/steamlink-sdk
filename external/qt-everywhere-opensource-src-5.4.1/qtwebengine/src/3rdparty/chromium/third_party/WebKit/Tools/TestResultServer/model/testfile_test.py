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

import datastorefile
import time
import testfile
import unittest

from datetime import datetime

from google.appengine.datastore import datastore_stub_util
from google.appengine.ext import db
from google.appengine.ext import testbed

TEST_DATA = [
    # master, builder, test_type, name, data; order matters.
    ['ChromiumWebKit', 'WebKit Linux', 'layout-tests', 'webkit_linux_results.json', 'a'],
    ['ChromiumWebKit', 'WebKit Win7', 'layout-tests', 'webkit_win7_results.json', 'b'],
    ['ChromiumWin', 'Win7 (Dbg)', 'unittests', 'win7_dbg_unittests.json', 'c'],
]


class DataStoreFileTest(unittest.TestCase):
    def setUp(self):
        self.testbed = testbed.Testbed()
        self.testbed.activate()

        self.policy = datastore_stub_util.PseudoRandomHRConsistencyPolicy(probability=1)
        self.testbed.init_datastore_v3_stub(consistency_policy=self.policy)

        test_file = testfile.TestFile()

    def _getAllFiles(self):
        return testfile.TestFile.get_files(None, None, None, None, limit=None)

    def _assertFileMatchesData(self, expected_data, actual_file):
        actual_fields = [actual_file.master, actual_file.builder, actual_file.test_type, actual_file.name, actual_file.data]
        self.assertEqual(expected_data, actual_fields, 'Mismatch between expected fields in file and actual file.')

    def _addFileAndAssert(self, file_data):
        _, code = testfile.TestFile.add_file(*file_data)
        self.assertEqual(200, code, 'Unable to create file with data: %s' % file_data)

    def testSaveFile(self):
        file_data = TEST_DATA[0][:]
        self._addFileAndAssert(file_data)

        files = self._getAllFiles()
        self.assertEqual(1, len(files))
        self._assertFileMatchesData(file_data, files[0])

        _, code = testfile.TestFile.save_file(files[0], None)
        self.assertEqual(500, code, 'Expected empty file not to have been saved.')

        files = self._getAllFiles()
        self.assertEqual(1, len(files), 'Expected exactly one file to be present.')
        self._assertFileMatchesData(file_data, files[0])

    def testAddAndGetFile(self):
        for file_data in TEST_DATA:
            self._addFileAndAssert(file_data)

        files = self._getAllFiles()
        self.assertEqual(len(TEST_DATA), len(files), 'Mismatch between number of test records and number of files in db.')

        for f in files:
            fields = [f.master, f.builder, f.test_type, f.name, f.data]
            self.assertIn(fields, TEST_DATA)

    def testOverwriteOrAddFile(self):
        file_data = TEST_DATA[0][:]
        _, code = testfile.TestFile.overwrite_or_add_file(*file_data)
        self.assertEqual(200, code, 'Unable to create file with data: %s' % file_data)
        files = self._getAllFiles()
        self.assertEqual(1, len(files))

        _, code = testfile.TestFile.overwrite_or_add_file(*file_data)
        self.assertEqual(200, code, 'Unable to overwrite or create file with data: %s' % file_data)
        files = self._getAllFiles()
        self.assertEqual(1, len(files))

        file_data = TEST_DATA[1][:]
        _, code = testfile.TestFile.overwrite_or_add_file(*file_data)
        self.assertEqual(200, code, 'Unable to overwrite or create file with different data: %s' % file_data)
        files = self._getAllFiles()
        self.assertEqual(2, len(files))

    def testDeleteFile(self):
        file_contents = 'x' * datastorefile.MAX_ENTRY_LEN * 2
        file_data = ['ChromiumWebKit', 'WebKit Linux', 'layout-tests', 'results.json', file_contents]
        self._addFileAndAssert(file_data)

        ndeleted = testfile.TestFile.delete_file(None, 'ChromiumWebKit', 'WebKit Linux', 'layout-tests', 'results.json', None, None)
        self.assertEqual(1, ndeleted, 'Expected exactly one file to have been deleted.')

        nfiles = testfile.TestFile.all().count()
        self.assertEqual(0, nfiles, 'Expected exactly zero files to be present in db.')

    def testDeleteAll(self):
        for file_data in TEST_DATA:
            self._addFileAndAssert(file_data)

        files = self._getAllFiles()
        self.assertEqual(len(TEST_DATA), len(files))

        files[0]._delete_all()

        files = self._getAllFiles()
        self.assertEqual(len(TEST_DATA) - 1, len(files))


if __name__ == '__main__':
    unittest.main()
