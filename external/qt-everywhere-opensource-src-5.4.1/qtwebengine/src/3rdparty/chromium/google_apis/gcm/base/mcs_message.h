// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GCM_BASE_MCS_MESSAGE_H_
#define GOOGLE_APIS_GCM_BASE_MCS_MESSAGE_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "google_apis/gcm/base/gcm_export.h"

namespace google {
namespace protobuf {
class MessageLite;
}  // namespace protobuf
}  // namespace google

namespace gcm {

// A wrapper for MCS protobuffers that encapsulates their tag, size and data
// in an immutable and thread-safe format. If a mutable version is desired,
// CloneProtobuf() should use used to create a new copy of the protobuf.
//
// Note: default copy and assign welcome.
class GCM_EXPORT MCSMessage {
 public:
  // Creates an invalid MCSMessage.
  MCSMessage();
  // Infers tag from |message|.
  explicit MCSMessage(const google::protobuf::MessageLite& protobuf);
  // |tag| must match |protobuf|'s message type.
  MCSMessage(uint8 tag, const google::protobuf::MessageLite& protobuf);
  // |tag| must match |protobuf|'s message type. Takes ownership of |protobuf|.
  MCSMessage(uint8 tag,
             scoped_ptr<const google::protobuf::MessageLite> protobuf);
  ~MCSMessage();

  // Returns whether this message is valid or not (whether a protobuf was
  // provided at construction time or not).
  bool IsValid() const;

  // Getters for serialization.
  uint8 tag() const { return tag_; }
  int size() const {return size_; }
  std::string SerializeAsString() const;

  // Getter for accessing immutable probotuf fields.
  const google::protobuf::MessageLite& GetProtobuf() const;

  // Getter for creating a mutated version of the protobuf.
  scoped_ptr<google::protobuf::MessageLite> CloneProtobuf() const;

 private:
  class Core : public base::RefCountedThreadSafe<MCSMessage::Core> {
   public:
    Core();
    Core(uint8 tag, const google::protobuf::MessageLite& protobuf);
    Core(uint8 tag, scoped_ptr<const google::protobuf::MessageLite> protobuf);

    const google::protobuf::MessageLite& Get() const;

   private:
    friend class base::RefCountedThreadSafe<MCSMessage::Core>;
    ~Core();

    // The immutable protobuf.
    scoped_ptr<const google::protobuf::MessageLite> protobuf_;

    DISALLOW_COPY_AND_ASSIGN(Core);
  };

  // These are cached separately to avoid having to recompute them.
  const uint8 tag_;
  const int size_;

  // The refcounted core, containing the protobuf memory.
  scoped_refptr<const Core> core_;
};

}  // namespace gcm

#endif  // GOOGLE_APIS_GCM_BASE_MCS_MESSAGE_H_
