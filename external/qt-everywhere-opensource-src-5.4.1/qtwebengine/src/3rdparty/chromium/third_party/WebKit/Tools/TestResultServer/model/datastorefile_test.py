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

import unittest

import datastorefile

from google.appengine.ext import db
from google.appengine.ext import testbed


class DataStoreFileTest(unittest.TestCase):
    def setUp(self):
        self.testbed = testbed.Testbed()
        self.testbed.activate()
        self.testbed.init_datastore_v3_stub()

        self.test_file = datastorefile.DataStoreFile()

    def tearDown(self):
        self.testbed.deactivate()

    def testSaveLoadDeleteData(self):
        test_data = 'x' * datastorefile.MAX_ENTRY_LEN * 3

        self.assertTrue(self.test_file.save_data(test_data))
        self.assertEqual(test_data, self.test_file.data)

        self.assertEqual(test_data, self.test_file.load_data())
        self.assertEqual(test_data, self.test_file.data)

        self.test_file.delete_data()
        self.assertFalse(self.test_file.load_data())

    def testLoadDataInvalidKey(self):
        test_data = 'x' * datastorefile.MAX_ENTRY_LEN * 3

        self.assertTrue(self.test_file.save_data(test_data))
        self.assertEqual(test_data, self.test_file.data)

        self.test_file.delete_data()
        self.assertEqual('', self.test_file.load_data())

    def testLoadDataNoKeys(self):
        # This should never happen.
        self.assertEqual(None, self.test_file.load_data())

    def testSaveData(self):
        self.assertFalse(self.test_file.save_data(None))

        too_big_data = 'x' * (datastorefile.MAX_DATA_ENTRY_PER_FILE * datastorefile.MAX_ENTRY_LEN + 1)
        self.assertFalse(self.test_file.save_data(too_big_data))

        test_data = 'x' * datastorefile.MAX_ENTRY_LEN * 5
        self.assertTrue(self.test_file.save_data(test_data))
        nchunks = datastorefile.DataEntry.all().count()
        nkeys = len(self.test_file.data_keys) + len(self.test_file.new_data_keys)
        self.assertEqual(nkeys, nchunks)

    def testSaveDataKeyReuse(self):
        test_data = 'x' * datastorefile.MAX_ENTRY_LEN * 5
        self.assertTrue(self.test_file.save_data(test_data))
        nchunks = datastorefile.DataEntry.all().count()
        nkeys = len(self.test_file.data_keys) + len(self.test_file.new_data_keys)
        self.assertEqual(nkeys, nchunks)

        smaller_data = 'x' * datastorefile.MAX_ENTRY_LEN * 3
        self.assertTrue(self.test_file.save_data(smaller_data))
        nchunks = datastorefile.DataEntry.all().count()
        nkeys_before = len(self.test_file.data_keys) + len(self.test_file.new_data_keys)
        self.assertEqual(nkeys_before, nchunks)

        self.assertTrue(self.test_file.save_data(smaller_data))
        nchunks = datastorefile.DataEntry.all().count()
        nkeys_after = len(self.test_file.data_keys) + len(self.test_file.new_data_keys)
        self.assertEqual(nkeys_after, nchunks)
        self.assertNotEqual(nkeys_before, nkeys_after)

    def testGetChunkIndices(self):
        data_length = datastorefile.MAX_ENTRY_LEN * 3
        chunk_indices = self.test_file._get_chunk_indices(data_length)
        self.assertEqual(len(chunk_indices), 3)
        self.assertNotEqual(chunk_indices[0], chunk_indices[-1])

        data_length += 1
        chunk_indices = self.test_file._get_chunk_indices(data_length)
        self.assertEqual(len(chunk_indices), 4)


if __name__ == '__main__':
    unittest.main()
