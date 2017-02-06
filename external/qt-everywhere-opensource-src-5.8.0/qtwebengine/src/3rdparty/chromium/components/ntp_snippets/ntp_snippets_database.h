// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_NTP_SNIPPETS_DATABASE_H_
#define COMPONENTS_NTP_SNIPPETS_NTP_SNIPPETS_DATABASE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "components/leveldb_proto/proto_database.h"
#include "components/ntp_snippets/ntp_snippet.h"

namespace base {
class FilePath;
}

namespace ntp_snippets {

class SnippetImageProto;
class SnippetProto;

class NTPSnippetsDatabase {
 public:
  using SnippetsCallback = base::Callback<void(NTPSnippet::PtrVector)>;
  using SnippetImageCallback = base::Callback<void(std::string)>;

  NTPSnippetsDatabase(
      const base::FilePath& database_dir,
      scoped_refptr<base::SequencedTaskRunner> file_task_runner);
  ~NTPSnippetsDatabase();

  // Returns whether the database has finished initialization. While this is
  // false, loads may already be started (they'll be serviced after
  // initialization finishes), but no updates are allowed.
  bool IsInitialized() const;

  // Returns whether the database is in an (unrecoverable) error state. If this
  // is true, the database must not be used anymore
  bool IsErrorState() const;

  // Set a callback to be called when the database enters an error state.
  void SetErrorCallback(const base::Closure& error_callback);

  // Loads all snippets from storage and passes them to |callback|.
  void LoadSnippets(const SnippetsCallback& callback);

  // Adds or updates the given snippet.
  void SaveSnippet(const NTPSnippet& snippet);
  // Adds or updates all the given snippets.
  void SaveSnippets(const NTPSnippet::PtrVector& snippets);

  // Deletes the snippet with the given ID, and its image.
  void DeleteSnippet(const std::string& snippet_id);
  // Deletes all the given snippets (identified by their IDs) and their images.
  void DeleteSnippets(const NTPSnippet::PtrVector& snippets);

  // Loads the image data for the snippet with the given ID and passes it to
  // |callback|. Passes an empty string if not found.
  void LoadImage(const std::string& snippet_id,
                 const SnippetImageCallback& callback);

  // Adds or updates the image data for the given snippet ID.
  void SaveImage(const std::string& snippet_id, const std::string& image_data);

  // Deletes the image data for the given snippet ID.
  void DeleteImage(const std::string& snippet_id);

 private:
  friend class NTPSnippetsDatabaseTest;

  using KeyEntryVector =
      leveldb_proto::ProtoDatabase<SnippetProto>::KeyEntryVector;

  using ImageKeyEntryVector =
      leveldb_proto::ProtoDatabase<SnippetImageProto>::KeyEntryVector;

  // Callbacks for ProtoDatabase<SnippetProto> operations.
  void OnDatabaseInited(bool success);
  void OnDatabaseLoaded(const SnippetsCallback& callback,
                        bool success,
                        std::unique_ptr<std::vector<SnippetProto>> entries);
  void OnDatabaseSaved(bool success);

  // Callbacks for ProtoDatabase<SnippetImageProto> operations.
  void OnImageDatabaseInited(bool success);
  void OnImageDatabaseLoaded(const SnippetImageCallback& callback,
                             bool success,
                             std::unique_ptr<SnippetImageProto> entry);
  void OnImageDatabaseSaved(bool success);

  void OnDatabaseError();

  void ProcessPendingLoads();

  void LoadSnippetsImpl(const SnippetsCallback& callback);
  void SaveSnippetsImpl(std::unique_ptr<KeyEntryVector> entries_to_save);
  void DeleteSnippetsImpl(
      std::unique_ptr<std::vector<std::string>> keys_to_remove);

  void LoadImageImpl(const std::string& snippet_id,
                     const SnippetImageCallback& callback);
  void DeleteImagesImpl(
      std::unique_ptr<std::vector<std::string>> keys_to_remove);

  std::unique_ptr<leveldb_proto::ProtoDatabase<SnippetProto>> database_;
  bool database_initialized_;
  std::vector<SnippetsCallback> pending_snippets_callbacks_;

  std::unique_ptr<leveldb_proto::ProtoDatabase<SnippetImageProto>>
      image_database_;
  bool image_database_initialized_;
  std::vector<std::pair<std::string, SnippetImageCallback>>
      pending_image_callbacks_;

  base::Closure error_callback_;

  base::WeakPtrFactory<NTPSnippetsDatabase> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(NTPSnippetsDatabase);
};

}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_NTP_SNIPPETS_DATABASE_H_
