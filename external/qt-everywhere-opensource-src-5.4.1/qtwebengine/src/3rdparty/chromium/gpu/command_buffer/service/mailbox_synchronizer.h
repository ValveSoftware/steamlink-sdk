// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_MAILBOX_SYNCHRONIZER_H_
#define GPU_COMMAND_BUFFER_SERVICE_MAILBOX_SYNCHRONIZER_H_

#include "gpu/command_buffer/common/mailbox.h"

#include <map>
#include <set>

#include "base/memory/linked_ptr.h"
#include "base/synchronization/lock.h"
#include "gpu/command_buffer/service/texture_definition.h"
#include "gpu/gpu_export.h"

namespace gpu {
namespace gles2 {

class MailboxManager;
class Texture;

// A thread-safe proxy that can be used to emulate texture sharing across
// share-groups.
class MailboxSynchronizer {
 public:
  ~MailboxSynchronizer();

  GPU_EXPORT static bool Initialize();
  GPU_EXPORT static void Terminate();
  static MailboxSynchronizer* GetInstance();

  // Create a texture from a globally visible mailbox.
  Texture* CreateTextureFromMailbox(unsigned target, const Mailbox& mailbox);

  void PushTextureUpdates(MailboxManager* manager);
  void PullTextureUpdates(MailboxManager* manager);

  void TextureDeleted(Texture* texture);

 private:
  MailboxSynchronizer();

  struct TargetName {
    TargetName(unsigned target, const Mailbox& mailbox);
    bool operator<(const TargetName& rhs) const {
      return memcmp(this, &rhs, sizeof(rhs)) < 0;
    }
    bool operator!=(const TargetName& rhs) const {
      return memcmp(this, &rhs, sizeof(rhs)) != 0;
    }
    bool operator==(const TargetName& rhs) const {
      return !operator!=(rhs);
    }
    unsigned target;
    Mailbox mailbox;
  };

  base::Lock lock_;

  struct TextureGroup {
    explicit TextureGroup(const TextureDefinition& definition);
    ~TextureGroup();

    TextureDefinition definition;
    std::set<TargetName> mailboxes;
   private:
    DISALLOW_COPY_AND_ASSIGN(TextureGroup);
  };

  struct TextureVersion {
    explicit TextureVersion(linked_ptr<TextureGroup> group);
    ~TextureVersion();

    unsigned int version;
    linked_ptr<TextureGroup> group;
  };
  typedef std::map<Texture*, TextureVersion> TextureMap;
  TextureMap textures_;

  linked_ptr<TextureGroup> GetGroupForMailboxLocked(
      const TargetName& target_name);
  void ReassociateMailboxLocked(
      const TargetName& target_name,
      TextureGroup* group);
  void UpdateTextureLocked(Texture* texture, TextureVersion& texture_version);

  DISALLOW_COPY_AND_ASSIGN(MailboxSynchronizer);
};

}  // namespage gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_MAILBOX_SYNCHRONIZER_H_

