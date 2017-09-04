// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/remote/remote_suggestions_database.h"

#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/ntp_snippets/remote/proto/ntp_snippets.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::ElementsAre;
using testing::Eq;
using testing::IsEmpty;
using testing::Mock;
using testing::_;

namespace ntp_snippets {

namespace {

std::vector<SnippetSource> ExtractSources(const NTPSnippet& snippet) {
  std::vector<SnippetSource> result;
  SnippetProto proto = snippet.ToProto();
  for (const auto& source : proto.sources()) {
    result.emplace_back(GURL(source.url()), source.publisher_name(),
                        source.has_amp_url() ? GURL(source.amp_url()) : GURL());
  }
  return result;
}

}  // namespace

bool operator==(const SnippetSource& lhs, const SnippetSource& rhs) {
  return lhs.url == rhs.url && lhs.publisher_name == rhs.publisher_name &&
         lhs.amp_url == rhs.amp_url;
}

bool operator==(const NTPSnippet& lhs, const NTPSnippet& rhs) {
  return lhs.id() == rhs.id() && lhs.title() == rhs.title() &&
         lhs.snippet() == rhs.snippet() &&
         lhs.salient_image_url() == rhs.salient_image_url() &&
         lhs.publish_date() == rhs.publish_date() &&
         lhs.expiry_date() == rhs.expiry_date() &&
         ExtractSources(lhs) == ExtractSources(rhs) &&
         lhs.score() == rhs.score() && lhs.is_dismissed() == rhs.is_dismissed();
}

namespace {

std::unique_ptr<NTPSnippet> CreateTestSnippet() {
  auto snippet =
      base::MakeUnique<NTPSnippet>("http://localhost", kArticlesRemoteId);
  snippet->add_source(
      SnippetSource(GURL("http://localhost"), "Publisher", GURL("http://amp")));
  return snippet;
}

MATCHER_P(SnippetEq, snippet, "") {
  return *arg == *snippet;
}

}  // namespace

class RemoteSuggestionsDatabaseTest : public testing::Test {
 public:
  RemoteSuggestionsDatabaseTest() {
    EXPECT_TRUE(database_dir_.CreateUniqueTempDir());
  }

  ~RemoteSuggestionsDatabaseTest() override {
    // We need to run the message loop after deleting the database, because
    // ProtoDatabaseImpl deletes the actual LevelDB asynchronously on the task
    // runner. Without this, we'd get reports of memory leaks.
    db_.reset();
    base::RunLoop().RunUntilIdle();
  }

  void CreateDatabase() {
    // Explicitly destroy any existing database first, so it releases the lock
    // on the file.
    db_.reset();

    db_.reset(new RemoteSuggestionsDatabase(
        database_dir_.GetPath(), base::ThreadTaskRunnerHandle::Get()));
  }

  RemoteSuggestionsDatabase* db() { return db_.get(); }

  // TODO(tschumann): MOCK_METHODS on non mock objects are an anti-pattern.
  // Clean up.
  void OnSnippetsLoaded(NTPSnippet::PtrVector snippets) {
    OnSnippetsLoadedImpl(snippets);
  }
  MOCK_METHOD1(OnSnippetsLoadedImpl,
               void(const NTPSnippet::PtrVector& snippets));

  MOCK_METHOD1(OnImageLoaded, void(std::string));

 private:
  base::MessageLoop message_loop_;
  base::ScopedTempDir database_dir_;
  std::unique_ptr<RemoteSuggestionsDatabase> db_;

  DISALLOW_COPY_AND_ASSIGN(RemoteSuggestionsDatabaseTest);
};

TEST_F(RemoteSuggestionsDatabaseTest, Init) {
  ASSERT_FALSE(db());

  CreateDatabase();
  EXPECT_FALSE(db()->IsInitialized());

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(db()->IsInitialized());
}

TEST_F(RemoteSuggestionsDatabaseTest, LoadBeforeInit) {
  CreateDatabase();
  EXPECT_FALSE(db()->IsInitialized());

  // Start a snippet and image load before the DB is initialized.
  db()->LoadSnippets(
      base::Bind(&RemoteSuggestionsDatabaseTest::OnSnippetsLoaded,
                 base::Unretained(this)));
  db()->LoadImage("id",
                  base::Bind(&RemoteSuggestionsDatabaseTest::OnImageLoaded,
                             base::Unretained(this)));

  // They should be serviced once initialization finishes.
  EXPECT_CALL(*this, OnSnippetsLoadedImpl(_));
  EXPECT_CALL(*this, OnImageLoaded(_));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(db()->IsInitialized());
}

TEST_F(RemoteSuggestionsDatabaseTest, LoadAfterInit) {
  CreateDatabase();
  EXPECT_FALSE(db()->IsInitialized());

  EXPECT_CALL(*this, OnSnippetsLoadedImpl(_)).Times(0);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(db()->IsInitialized());

  Mock::VerifyAndClearExpectations(this);

  EXPECT_CALL(*this, OnSnippetsLoadedImpl(_));
  db()->LoadSnippets(
      base::Bind(&RemoteSuggestionsDatabaseTest::OnSnippetsLoaded,
                 base::Unretained(this)));
  EXPECT_CALL(*this, OnImageLoaded(_));
  db()->LoadImage("id",
                  base::Bind(&RemoteSuggestionsDatabaseTest::OnImageLoaded,
                             base::Unretained(this)));
  base::RunLoop().RunUntilIdle();
}

TEST_F(RemoteSuggestionsDatabaseTest, Save) {
  CreateDatabase();
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(db()->IsInitialized());

  std::unique_ptr<NTPSnippet> snippet = CreateTestSnippet();
  std::string image_data("pretty image");

  // Store a snippet and an image.
  db()->SaveSnippet(*snippet);
  db()->SaveImage(snippet->id(), image_data);

  // Make sure they're there.
  EXPECT_CALL(*this,
              OnSnippetsLoadedImpl(ElementsAre(SnippetEq(snippet.get()))));
  db()->LoadSnippets(
      base::Bind(&RemoteSuggestionsDatabaseTest::OnSnippetsLoaded,
                 base::Unretained(this)));
  base::RunLoop().RunUntilIdle();

  Mock::VerifyAndClearExpectations(this);

  EXPECT_CALL(*this, OnImageLoaded(image_data));
  db()->LoadImage(snippet->id(),
                  base::Bind(&RemoteSuggestionsDatabaseTest::OnImageLoaded,
                             base::Unretained(this)));
  base::RunLoop().RunUntilIdle();
}

TEST_F(RemoteSuggestionsDatabaseTest, SavePersist) {
  CreateDatabase();
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(db()->IsInitialized());

  std::unique_ptr<NTPSnippet> snippet = CreateTestSnippet();
  std::string image_data("pretty image");

  // Store a snippet and an image.
  db()->SaveSnippet(*snippet);
  db()->SaveImage(snippet->id(), image_data);
  base::RunLoop().RunUntilIdle();

  // They should still exist after recreating the database.
  CreateDatabase();

  EXPECT_CALL(*this,
              OnSnippetsLoadedImpl(ElementsAre(SnippetEq(snippet.get()))));
  db()->LoadSnippets(
      base::Bind(&RemoteSuggestionsDatabaseTest::OnSnippetsLoaded,
                 base::Unretained(this)));
  EXPECT_CALL(*this, OnImageLoaded(image_data));
  db()->LoadImage(snippet->id(),
                  base::Bind(&RemoteSuggestionsDatabaseTest::OnImageLoaded,
                             base::Unretained(this)));
  base::RunLoop().RunUntilIdle();
}

TEST_F(RemoteSuggestionsDatabaseTest, Update) {
  CreateDatabase();
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(db()->IsInitialized());

  std::unique_ptr<NTPSnippet> snippet = CreateTestSnippet();

  // Store a snippet.
  db()->SaveSnippet(*snippet);

  // Change it.
  const std::string text("some text");
  snippet->set_snippet(text);
  db()->SaveSnippet(*snippet);

  // Make sure we get the updated version.
  EXPECT_CALL(*this,
              OnSnippetsLoadedImpl(ElementsAre(SnippetEq(snippet.get()))));
  db()->LoadSnippets(
      base::Bind(&RemoteSuggestionsDatabaseTest::OnSnippetsLoaded,
                 base::Unretained(this)));
  base::RunLoop().RunUntilIdle();
}

TEST_F(RemoteSuggestionsDatabaseTest, Delete) {
  CreateDatabase();
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(db()->IsInitialized());

  std::unique_ptr<NTPSnippet> snippet = CreateTestSnippet();

  // Store a snippet.
  db()->SaveSnippet(*snippet);

  // Make sure it's there.
  EXPECT_CALL(*this,
              OnSnippetsLoadedImpl(ElementsAre(SnippetEq(snippet.get()))));
  db()->LoadSnippets(
      base::Bind(&RemoteSuggestionsDatabaseTest::OnSnippetsLoaded,
                 base::Unretained(this)));
  base::RunLoop().RunUntilIdle();

  Mock::VerifyAndClearExpectations(this);

  // Delete the snippet.
  db()->DeleteSnippet(snippet->id());

  // Make sure it's gone.
  EXPECT_CALL(*this, OnSnippetsLoadedImpl(IsEmpty()));
  db()->LoadSnippets(
      base::Bind(&RemoteSuggestionsDatabaseTest::OnSnippetsLoaded,
                 base::Unretained(this)));
  base::RunLoop().RunUntilIdle();
}

TEST_F(RemoteSuggestionsDatabaseTest, DeleteSnippetDoesNotDeleteImage) {
  CreateDatabase();
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(db()->IsInitialized());

  std::unique_ptr<NTPSnippet> snippet = CreateTestSnippet();
  std::string image_data("pretty image");

  // Store a snippet and image.
  db()->SaveSnippet(*snippet);
  db()->SaveImage(snippet->id(), image_data);
  base::RunLoop().RunUntilIdle();

  // Make sure they're there.
  EXPECT_CALL(*this,
              OnSnippetsLoadedImpl(ElementsAre(SnippetEq(snippet.get()))));
  db()->LoadSnippets(
      base::Bind(&RemoteSuggestionsDatabaseTest::OnSnippetsLoaded,
                 base::Unretained(this)));
  EXPECT_CALL(*this, OnImageLoaded(image_data));
  db()->LoadImage(snippet->id(),
                  base::Bind(&RemoteSuggestionsDatabaseTest::OnImageLoaded,
                             base::Unretained(this)));
  base::RunLoop().RunUntilIdle();

  Mock::VerifyAndClearExpectations(this);

  // Delete the snippet.
  db()->DeleteSnippet(snippet->id());

  // Make sure the image is still there.
  EXPECT_CALL(*this, OnImageLoaded(image_data));
  db()->LoadImage(snippet->id(),
                  base::Bind(&RemoteSuggestionsDatabaseTest::OnImageLoaded,
                             base::Unretained(this)));
  base::RunLoop().RunUntilIdle();
}

TEST_F(RemoteSuggestionsDatabaseTest, DeleteImage) {
  CreateDatabase();
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(db()->IsInitialized());

  std::unique_ptr<NTPSnippet> snippet = CreateTestSnippet();
  std::string image_data("pretty image");

  // Store the image.
  db()->SaveImage(snippet->id(), image_data);
  base::RunLoop().RunUntilIdle();

  // Make sure the image is there.
  EXPECT_CALL(*this, OnImageLoaded(image_data));
  db()->LoadImage(snippet->id(),
                  base::Bind(&RemoteSuggestionsDatabaseTest::OnImageLoaded,
                             base::Unretained(this)));
  base::RunLoop().RunUntilIdle();

  Mock::VerifyAndClearExpectations(this);

  // Delete the snippet.
  db()->DeleteImage(snippet->id());

  // Make sure the image is gone.
  EXPECT_CALL(*this, OnImageLoaded(std::string()));
  db()->LoadImage(snippet->id(),
                  base::Bind(&RemoteSuggestionsDatabaseTest::OnImageLoaded,
                             base::Unretained(this)));
  base::RunLoop().RunUntilIdle();
}

namespace {

void LoadExpectedImage(RemoteSuggestionsDatabase* db,
                       const std::string& id,
                       const std::string& expected_data) {
  base::RunLoop run_loop;
  db->LoadImage(id, base::Bind(
                        [](base::Closure signal, std::string expected_data,
                           std::string actual_data) {
                          EXPECT_THAT(actual_data, Eq(expected_data));
                          signal.Run();
                        },
                        run_loop.QuitClosure(), expected_data));
  run_loop.Run();
}

}  // namespace

TEST_F(RemoteSuggestionsDatabaseTest, ShouldGarbageCollectImages) {
  CreateDatabase();
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(db()->IsInitialized());

  // Store images.
  db()->SaveImage("snippet-id-1", "pretty-image-1");
  db()->SaveImage("snippet-id-2", "pretty-image-2");
  db()->SaveImage("snippet-id-3", "pretty-image-3");
  base::RunLoop().RunUntilIdle();

  // Make sure the to-be-garbage collected images are there.
  LoadExpectedImage(db(), "snippet-id-1", "pretty-image-1");
  LoadExpectedImage(db(), "snippet-id-3", "pretty-image-3");

  // Garbage collect all except the second.
  db()->GarbageCollectImages(base::MakeUnique<std::set<std::string>>(
      std::set<std::string>({"snippet-id-2"})));
  base::RunLoop().RunUntilIdle();

  // Make sure the images are gone.
  LoadExpectedImage(db(), "snippet-id-1", "");
  LoadExpectedImage(db(), "snippet-id-3", "");
  // Make sure the second still exists.
  LoadExpectedImage(db(), "snippet-id-2", "pretty-image-2");
}

}  // namespace ntp_snippets
