// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/mailbox_synchronizer.h"

#include "base/bind.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "ui/gl/gl_implementation.h"

namespace gpu {
namespace gles2 {

namespace {

MailboxSynchronizer* g_instance = NULL;

}  // anonymous namespace

// static
bool MailboxSynchronizer::Initialize() {
  DCHECK(!g_instance);
  DCHECK(gfx::GetGLImplementation() != gfx::kGLImplementationNone)
      << "GL bindings not initialized";
  switch (gfx::GetGLImplementation()) {
    case gfx::kGLImplementationMockGL:
      break;
    case gfx::kGLImplementationEGLGLES2:
#if !defined(OS_MACOSX)
      {
        if (!gfx::g_driver_egl.ext.b_EGL_KHR_image_base ||
            !gfx::g_driver_egl.ext.b_EGL_KHR_gl_texture_2D_image ||
            !gfx::g_driver_gl.ext.b_GL_OES_EGL_image ||
            !gfx::g_driver_egl.ext.b_EGL_KHR_fence_sync) {
          LOG(WARNING) << "MailboxSync not supported due to missing EGL "
                          "image/fence support";
          return false;
        }
      }
      break;
#endif
    default:
      NOTREACHED();
      return false;
  }
  g_instance = new MailboxSynchronizer;
  return true;
}

// static
void MailboxSynchronizer::Terminate() {
  DCHECK(g_instance);
  delete g_instance;
  g_instance = NULL;
}

// static
MailboxSynchronizer* MailboxSynchronizer::GetInstance() {
  return g_instance;
}

MailboxSynchronizer::TargetName::TargetName(unsigned target,
                                            const Mailbox& mailbox)
    : target(target), mailbox(mailbox) {}

MailboxSynchronizer::TextureGroup::TextureGroup(
    const TextureDefinition& definition)
    : definition(definition) {}

MailboxSynchronizer::TextureGroup::~TextureGroup() {}

MailboxSynchronizer::TextureVersion::TextureVersion(
    linked_ptr<TextureGroup> group)
    : version(group->definition.version()), group(group) {}

MailboxSynchronizer::TextureVersion::~TextureVersion() {}

MailboxSynchronizer::MailboxSynchronizer() {}

MailboxSynchronizer::~MailboxSynchronizer() {
  DCHECK_EQ(0U, textures_.size());
}

void MailboxSynchronizer::ReassociateMailboxLocked(
    const TargetName& target_name,
    TextureGroup* group) {
  lock_.AssertAcquired();
  for (TextureMap::iterator it = textures_.begin(); it != textures_.end();
       it++) {
    std::set<TargetName>::iterator mb_it =
        it->second.group->mailboxes.find(target_name);
    if (it->second.group != group &&
        mb_it != it->second.group->mailboxes.end()) {
      it->second.group->mailboxes.erase(mb_it);
    }
  }
  group->mailboxes.insert(target_name);
}

linked_ptr<MailboxSynchronizer::TextureGroup>
MailboxSynchronizer::GetGroupForMailboxLocked(const TargetName& target_name) {
  lock_.AssertAcquired();
  for (TextureMap::iterator it = textures_.begin(); it != textures_.end();
       it++) {
    std::set<TargetName>::const_iterator mb_it =
        it->second.group->mailboxes.find(target_name);
    if (mb_it != it->second.group->mailboxes.end())
      return it->second.group;
  }
  return make_linked_ptr<MailboxSynchronizer::TextureGroup>(NULL);
}

Texture* MailboxSynchronizer::CreateTextureFromMailbox(unsigned target,
                                                       const Mailbox& mailbox) {
  base::AutoLock lock(lock_);
  TargetName target_name(target, mailbox);
  linked_ptr<TextureGroup> group = GetGroupForMailboxLocked(target_name);
  if (group.get()) {
    Texture* new_texture = group->definition.CreateTexture();
    if (new_texture)
      textures_.insert(std::make_pair(new_texture, TextureVersion(group)));
    return new_texture;
  }

  return NULL;
}

void MailboxSynchronizer::TextureDeleted(Texture* texture) {
  base::AutoLock lock(lock_);
  TextureMap::iterator it = textures_.find(texture);
  if (it != textures_.end()) {
    // TODO: We could avoid the update if this was the last ref.
    UpdateTextureLocked(it->first, it->second);
    textures_.erase(it);
  }
}

void MailboxSynchronizer::PushTextureUpdates(MailboxManager* manager) {
  base::AutoLock lock(lock_);
  for (MailboxManager::MailboxToTextureMap::const_iterator texture_it =
           manager->mailbox_to_textures_.begin();
       texture_it != manager->mailbox_to_textures_.end();
       texture_it++) {
    TargetName target_name(texture_it->first.target, texture_it->first.mailbox);
    Texture* texture = texture_it->second->first;
    // TODO(sievers): crbug.com/352274
    // Should probably only fail if it already *has* mipmaps, while allowing
    // incomplete textures here. Also reconsider how to fail otherwise.
    bool needs_mips = texture->min_filter() != GL_NEAREST &&
                      texture->min_filter() != GL_LINEAR;
    if (target_name.target != GL_TEXTURE_2D || needs_mips)
      continue;

    TextureMap::iterator it = textures_.find(texture);
    if (it != textures_.end()) {
      TextureVersion& texture_version = it->second;
      TextureGroup* group = texture_version.group.get();
      std::set<TargetName>::const_iterator mb_it =
          group->mailboxes.find(target_name);
      if (mb_it == group->mailboxes.end()) {
        // We previously did not associate this texture with the given mailbox.
        // Unlink other texture groups from the mailbox.
        ReassociateMailboxLocked(target_name, group);
      }
      UpdateTextureLocked(texture, texture_version);

    } else {
      linked_ptr<TextureGroup> group = make_linked_ptr(new TextureGroup(
          TextureDefinition(target_name.target, texture, 1, NULL)));

      // Unlink other textures from this mailbox in case the name is not new.
      ReassociateMailboxLocked(target_name, group.get());
      textures_.insert(std::make_pair(texture, TextureVersion(group)));
    }
  }
}

void MailboxSynchronizer::UpdateTextureLocked(Texture* texture,
                                              TextureVersion& texture_version) {
  lock_.AssertAcquired();
  gfx::GLImage* gl_image = texture->GetLevelImage(texture->target(), 0);
  TextureGroup* group = texture_version.group.get();
  scoped_refptr<NativeImageBuffer> image_buffer = group->definition.image();

  // Make sure we don't clobber with an older version
  if (!group->definition.IsOlderThan(texture_version.version))
    return;

  // Also don't push redundant updates. Note that it would break the
  // versioning.
  if (group->definition.Matches(texture))
    return;

  if (gl_image && !image_buffer->IsClient(gl_image)) {
    LOG(ERROR) << "MailboxSync: Incompatible attachment";
    return;
  }

  group->definition = TextureDefinition(texture->target(),
                                        texture,
                                        ++texture_version.version,
                                        gl_image ? image_buffer : NULL);
}

void MailboxSynchronizer::PullTextureUpdates(MailboxManager* manager) {
  base::AutoLock lock(lock_);
  for (MailboxManager::MailboxToTextureMap::const_iterator texture_it =
           manager->mailbox_to_textures_.begin();
       texture_it != manager->mailbox_to_textures_.end();
       texture_it++) {
    Texture* texture = texture_it->second->first;
    TextureMap::iterator it = textures_.find(texture);
    if (it != textures_.end()) {
      TextureDefinition& definition = it->second.group->definition;
      if (it->second.version == definition.version() ||
          definition.IsOlderThan(it->second.version))
        continue;
      it->second.version = definition.version();
      definition.UpdateTexture(texture);
    }
  }
}

}  // namespace gles2
}  // namespace gpu
