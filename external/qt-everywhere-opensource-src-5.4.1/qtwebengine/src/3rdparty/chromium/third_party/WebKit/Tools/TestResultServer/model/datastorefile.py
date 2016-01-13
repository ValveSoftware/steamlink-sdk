# Copyright (C) 2010 Google Inc. All rights reserved.
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

import math
import logging

from google.appengine.ext import blobstore
from google.appengine.ext import db

MAX_DATA_ENTRY_PER_FILE = 30
MAX_ENTRY_LEN = 1000 * 1000


class ChunkData:
    def __init__(self):
        self.reused_key = None
        self.data_entry = None
        self.entry_future = None
        self.index = None


class DataEntry(db.Model):
    """Datastore entry that stores one segmant of file data
       (<1000*1000 bytes).
    """

    data = db.BlobProperty()

    @classmethod
    def get(cls, key):
        return db.get(key)

    @classmethod
    def get_async(cls, key):
        return db.get_async(key)

    @classmethod
    def delete_async(cls, key):
        return db.delete_async(key)


class DataStoreFile(db.Model):
    """This class stores file in datastore.
       If a file is oversize (>1000*1000 bytes), the file is split into
       multiple segments and stored in multiple datastore entries.
    """

    name = db.StringProperty()
    data_keys = db.ListProperty(db.Key)
    # keys to the data store entries that can be reused for new data.
    # If it is emtpy, create new DataEntry.
    new_data_keys = db.ListProperty(db.Key)
    date = db.DateTimeProperty(auto_now_add=True)

    data = None

    def _get_chunk_indices(self, data_length):
        nchunks = math.ceil(float(data_length) / MAX_ENTRY_LEN)
        return xrange(0, int(nchunks) * MAX_ENTRY_LEN, MAX_ENTRY_LEN)

    def _convert_blob_keys(self, keys):
        converted_keys = []
        for key in keys:
            new_key = blobstore.BlobMigrationRecord.get_new_blob_key(key)
            if new_key:
                converted_keys.append(new_key)
            else:
                converted_keys.append(key)
        return keys

    def delete_data(self, keys=None):
        if not keys:
            keys = self._convert_blob_keys(self.data_keys)
        logging.info('Doing async delete of keys: %s', keys)

        get_futures = [DataEntry.get_async(k) for k in keys]
        delete_futures = []
        for get_future in get_futures:
            result = get_future.get_result()
            if result:
                delete_futures.append(DataEntry.delete_async(result.key()))

        for delete_future in delete_futures:
            delete_future.get_result()

    def save_data(self, data):
        if not data:
            logging.warning("No data to save.")
            return False

        if len(data) > (MAX_DATA_ENTRY_PER_FILE * MAX_ENTRY_LEN):
            logging.error("File too big, can't save to datastore: %dK",
                len(data) / 1024)
            return False

        start = 0
        # Use the new_data_keys to store new data. If all new data are saved
        # successfully, swap new_data_keys and data_keys so we can reuse the
        # data_keys entries in next run. If unable to save new data for any
        # reason, only the data pointed by new_data_keys may be corrupted,
        # the existing data_keys data remains untouched. The corrupted data
        # in new_data_keys will be overwritten in next update.
        keys = self._convert_blob_keys(self.new_data_keys)
        self.new_data_keys = []

        chunk_indices = self._get_chunk_indices(len(data))
        logging.info('Saving file in %s chunks', len(chunk_indices))

        chunk_data = []
        for chunk_index in chunk_indices:
            chunk = ChunkData()
            chunk.index = chunk_index
            if keys:
                chunk.reused_key = keys.pop()
                chunk.entry_future = DataEntry.get_async(chunk.reused_key)
            else:
                chunk.data_entry = DataEntry()
            chunk_data.append(chunk)

        put_futures = []
        for chunk in chunk_data:
            if chunk.entry_future:
                data_entry = chunk.entry_future.get_result()
                if not data_entry:
                    logging.warning("Found key, but no data entry: %s", chunk.reused_key)
                    data_entry = DataEntry()
                chunk.data_entry = data_entry

            chunk.data_entry.data = db.Blob(data[chunk.index: chunk.index + MAX_ENTRY_LEN])
            put_futures.append(db.put_async(chunk.data_entry))

        for future in put_futures:
            key = None
            try:
                key = future.get_result()
                self.new_data_keys.append(key)
            except Exception, err:
                logging.error("Failed to save data store entry: %s", err)
                self.delete_data(keys)
                return False

        if keys:
            self.delete_data(keys)

        temp_keys = self._convert_blob_keys(self.data_keys)
        self.data_keys = self.new_data_keys
        self.new_data_keys = temp_keys
        self.data = data

        return True

    def load_data(self):
        if not self.data_keys:
            logging.warning("No data to load.")
            return None

        data_futures = [(k, DataEntry.get_async(k)) for k in self._convert_blob_keys(self.data_keys)]

        data = []
        for key, future in data_futures:
            result = future.get_result()
            if not result:
                logging.error("No data found for key: %s.", key)
                # FIXME: This really shouldn't happen, but it seems to be happening in practice.
                # Figure out how and then change this back to returning None.
                # In the meantime, return empty string so we at least start collecting
                # results from new runs even though it'll mean that the old data is lost.
                # crbug.com/377594
                return ''
            data.append(result)

        self.data = "".join([d.data for d in data])

        return self.data
