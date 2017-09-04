// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/notifications/notification_id_generator.h"

#include <sstream>

#include "base/base64.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/sha1.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "content/public/browser/browser_context.h"
#include "url/gurl.h"

namespace content {
namespace {

const char kPersistentNotificationPrefix[] = "p:";
const char kNonPersistentNotificationPrefix[] = "n:";

const char kSeparator = '#';

// Computes a hash based on the path in which the |browser_context| is stored.
// Since we only store the hash, SHA-1 is used to make the probability of
// collisions negligible.
std::string ComputeBrowserContextHash(BrowserContext* browser_context) {
  const base::FilePath path = browser_context->GetPath();

#if defined(OS_WIN)
  std::string path_hash = base::SHA1HashString(base::WideToUTF8(path.value()));
#else
  std::string path_hash = base::SHA1HashString(path.value());
#endif

  return base::HexEncode(path_hash.c_str(), path_hash.length());
}

}  // namespace

NotificationIdGenerator::NotificationIdGenerator(
    BrowserContext* browser_context)
    : browser_context_(browser_context) {}

NotificationIdGenerator::~NotificationIdGenerator() {}

// static
bool NotificationIdGenerator::IsPersistentNotification(
    const base::StringPiece& notification_id) {
  return notification_id.starts_with(kPersistentNotificationPrefix);
}

// static
bool NotificationIdGenerator::IsNonPersistentNotification(
    const base::StringPiece& notification_id) {
  return notification_id.starts_with(kNonPersistentNotificationPrefix);
}

std::string NotificationIdGenerator::GenerateForPersistentNotification(
    const GURL& origin,
    const std::string& tag,
    int64_t persistent_notification_id) const {
  DCHECK(origin.is_valid());
  DCHECK_EQ(origin, origin.GetOrigin());

  std::stringstream stream;

  stream << kPersistentNotificationPrefix;
  stream << ComputeBrowserContextHash(browser_context_);
  stream << base::IntToString(browser_context_->IsOffTheRecord());
  stream << origin;

  stream << base::IntToString(!tag.empty());
  if (tag.size())
    stream << tag;
  else
    stream << base::Int64ToString(persistent_notification_id);

  return stream.str();
}

std::string NotificationIdGenerator::GenerateForNonPersistentNotification(
    const GURL& origin,
    const std::string& tag,
    int non_persistent_notification_id,
    int render_process_id) const {
  DCHECK(origin.is_valid());
  DCHECK_EQ(origin, origin.GetOrigin());

  std::stringstream stream;

  stream << kNonPersistentNotificationPrefix;
  stream << ComputeBrowserContextHash(browser_context_);
  stream << base::IntToString(browser_context_->IsOffTheRecord());
  stream << origin;

  stream << base::IntToString(!tag.empty());
  if (tag.empty()) {
    stream << base::IntToString(render_process_id);
    stream << kSeparator;

    stream << base::IntToString(non_persistent_notification_id);
  } else {
    stream << tag;
  }

  return stream.str();
}

}  // namespace content
