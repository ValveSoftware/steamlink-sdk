// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This test is POSIX only.

#include "ipc/ipc_message_attachment_set.h"

#include <fcntl.h>
#include <stddef.h>
#include <unistd.h>

#include "base/posix/eintr_wrapper.h"
#include "build/build_config.h"
#include "ipc/ipc_platform_file_attachment_posix.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace IPC {
namespace {

// Get a safe file descriptor for test purposes.
int GetSafeFd() {
  return open("/dev/null", O_RDONLY);
}

// Returns true if fd was already closed.  Closes fd if not closed.
bool VerifyClosed(int fd) {
  const int duped = dup(fd);
  if (duped != -1) {
    EXPECT_NE(IGNORE_EINTR(close(duped)), -1);
    EXPECT_NE(IGNORE_EINTR(close(fd)), -1);
    return false;
  }
  return true;
}

// The MessageAttachmentSet will try and close some of the descriptor numbers
// which we given it. This is the base descriptor value. It's great enough such
// that no real descriptor will accidently be closed.
static const int kFDBase = 50000;

TEST(MessageAttachmentSet, BasicAdd) {
  scoped_refptr<MessageAttachmentSet> set(new MessageAttachmentSet);

  ASSERT_EQ(set->size(), 0u);
  ASSERT_TRUE(set->empty());
  ASSERT_TRUE(
      set->AddAttachment(new internal::PlatformFileAttachment(kFDBase)));
  ASSERT_EQ(set->size(), 1u);
  ASSERT_TRUE(!set->empty());

  // Empties the set and stops a warning about deleting a set with unconsumed
  // descriptors
  set->CommitAllDescriptors();
}

TEST(MessageAttachmentSet, BasicAddAndClose) {
  scoped_refptr<MessageAttachmentSet> set(new MessageAttachmentSet);

  ASSERT_EQ(set->size(), 0u);
  ASSERT_TRUE(set->empty());
  const int fd = GetSafeFd();
  ASSERT_TRUE(set->AddAttachment(
      new internal::PlatformFileAttachment(base::ScopedFD(fd))));
  ASSERT_EQ(set->size(), 1u);
  ASSERT_TRUE(!set->empty());

  set->CommitAllDescriptors();

  ASSERT_TRUE(VerifyClosed(fd));
}
TEST(MessageAttachmentSet, MaxSize) {
  scoped_refptr<MessageAttachmentSet> set(new MessageAttachmentSet);

  for (size_t i = 0; i < MessageAttachmentSet::kMaxDescriptorsPerMessage; ++i)
    ASSERT_TRUE(set->AddAttachment(
        new internal::PlatformFileAttachment(kFDBase + 1 + i)));

  ASSERT_TRUE(
      !set->AddAttachment(new internal::PlatformFileAttachment(kFDBase)));

  set->CommitAllDescriptors();
}

#if defined(OS_ANDROID)
#define MAYBE_SetDescriptors DISABLED_SetDescriptors
#else
#define MAYBE_SetDescriptors SetDescriptors
#endif
TEST(MessageAttachmentSet, MAYBE_SetDescriptors) {
  scoped_refptr<MessageAttachmentSet> set(new MessageAttachmentSet);

  ASSERT_TRUE(set->empty());
  set->AddDescriptorsToOwn(NULL, 0);
  ASSERT_TRUE(set->empty());

  const int fd = GetSafeFd();
  static const int fds[] = {fd};
  set->AddDescriptorsToOwn(fds, 1);
  ASSERT_TRUE(!set->empty());
  ASSERT_EQ(set->size(), 1u);

  set->CommitAllDescriptors();

  ASSERT_TRUE(VerifyClosed(fd));
}

TEST(MessageAttachmentSet, PeekDescriptors) {
  scoped_refptr<MessageAttachmentSet> set(new MessageAttachmentSet);

  set->PeekDescriptors(NULL);
  ASSERT_TRUE(
      set->AddAttachment(new internal::PlatformFileAttachment(kFDBase)));

  int fds[1];
  fds[0] = 0;
  set->PeekDescriptors(fds);
  ASSERT_EQ(fds[0], kFDBase);
  set->CommitAllDescriptors();
  ASSERT_TRUE(set->empty());
}

TEST(MessageAttachmentSet, WalkInOrder) {
  scoped_refptr<MessageAttachmentSet> set(new MessageAttachmentSet);

  // TODO(morrita): This test is wrong. TakeDescriptorAt() shouldn't be
  // used to retrieve borrowed descriptors. That never happens in production.
  ASSERT_TRUE(
      set->AddAttachment(new internal::PlatformFileAttachment(kFDBase)));
  ASSERT_TRUE(
      set->AddAttachment(new internal::PlatformFileAttachment(kFDBase + 1)));
  ASSERT_TRUE(
      set->AddAttachment(new internal::PlatformFileAttachment(kFDBase + 2)));

  ASSERT_EQ(set->GetNonBrokerableAttachmentAt(0)->TakePlatformFile(), kFDBase);
  ASSERT_EQ(set->GetNonBrokerableAttachmentAt(1)->TakePlatformFile(),
            kFDBase + 1);
  ASSERT_EQ(set->GetNonBrokerableAttachmentAt(2)->TakePlatformFile(),
            kFDBase + 2);

  set->CommitAllDescriptors();
}

TEST(MessageAttachmentSet, WalkWrongOrder) {
  scoped_refptr<MessageAttachmentSet> set(new MessageAttachmentSet);

  // TODO(morrita): This test is wrong. TakeDescriptorAt() shouldn't be
  // used to retrieve borrowed descriptors. That never happens in production.
  ASSERT_TRUE(
      set->AddAttachment(new internal::PlatformFileAttachment(kFDBase)));
  ASSERT_TRUE(
      set->AddAttachment(new internal::PlatformFileAttachment(kFDBase + 1)));
  ASSERT_TRUE(
      set->AddAttachment(new internal::PlatformFileAttachment(kFDBase + 2)));

  ASSERT_EQ(set->GetNonBrokerableAttachmentAt(0)->TakePlatformFile(), kFDBase);
  ASSERT_FALSE(set->GetNonBrokerableAttachmentAt(2));

  set->CommitAllDescriptors();
}

TEST(MessageAttachmentSet, WalkCycle) {
  scoped_refptr<MessageAttachmentSet> set(new MessageAttachmentSet);

  // TODO(morrita): This test is wrong. TakeDescriptorAt() shouldn't be
  // used to retrieve borrowed descriptors. That never happens in production.
  ASSERT_TRUE(
      set->AddAttachment(new internal::PlatformFileAttachment(kFDBase)));
  ASSERT_TRUE(
      set->AddAttachment(new internal::PlatformFileAttachment(kFDBase + 1)));
  ASSERT_TRUE(
      set->AddAttachment(new internal::PlatformFileAttachment(kFDBase + 2)));

  ASSERT_EQ(set->GetNonBrokerableAttachmentAt(0)->TakePlatformFile(), kFDBase);
  ASSERT_EQ(set->GetNonBrokerableAttachmentAt(1)->TakePlatformFile(),
            kFDBase + 1);
  ASSERT_EQ(set->GetNonBrokerableAttachmentAt(2)->TakePlatformFile(),
            kFDBase + 2);
  ASSERT_EQ(set->GetNonBrokerableAttachmentAt(0)->TakePlatformFile(), kFDBase);
  ASSERT_EQ(set->GetNonBrokerableAttachmentAt(1)->TakePlatformFile(),
            kFDBase + 1);
  ASSERT_EQ(set->GetNonBrokerableAttachmentAt(2)->TakePlatformFile(),
            kFDBase + 2);
  ASSERT_EQ(set->GetNonBrokerableAttachmentAt(0)->TakePlatformFile(), kFDBase);
  ASSERT_EQ(set->GetNonBrokerableAttachmentAt(1)->TakePlatformFile(),
            kFDBase + 1);
  ASSERT_EQ(set->GetNonBrokerableAttachmentAt(2)->TakePlatformFile(),
            kFDBase + 2);

  set->CommitAllDescriptors();
}

#if defined(OS_ANDROID)
#define MAYBE_DontClose DISABLED_DontClose
#else
#define MAYBE_DontClose DontClose
#endif
TEST(MessageAttachmentSet, MAYBE_DontClose) {
  scoped_refptr<MessageAttachmentSet> set(new MessageAttachmentSet);

  const int fd = GetSafeFd();
  ASSERT_TRUE(set->AddAttachment(new internal::PlatformFileAttachment(fd)));
  set->CommitAllDescriptors();

  ASSERT_FALSE(VerifyClosed(fd));
}

TEST(MessageAttachmentSet, DoClose) {
  scoped_refptr<MessageAttachmentSet> set(new MessageAttachmentSet);

  const int fd = GetSafeFd();
  ASSERT_TRUE(set->AddAttachment(
      new internal::PlatformFileAttachment(base::ScopedFD(fd))));
  set->CommitAllDescriptors();

  ASSERT_TRUE(VerifyClosed(fd));
}

}  // namespace
}  // namespace IPC
