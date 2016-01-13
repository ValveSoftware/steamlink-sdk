// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/streams/stream_registry.h"

#include "content/browser/streams/stream.h"

namespace content {

namespace {
// The maximum size of memory each StreamRegistry instance is allowed to use
// for its Stream instances.
const size_t kDefaultMaxMemoryUsage = 1024 * 1024 * 1024U;  // 1GiB
}

StreamRegistry::StreamRegistry()
    : total_memory_usage_(0),
      max_memory_usage_(kDefaultMaxMemoryUsage) {
}

StreamRegistry::~StreamRegistry() {
}

void StreamRegistry::RegisterStream(scoped_refptr<Stream> stream) {
  DCHECK(CalledOnValidThread());
  DCHECK(stream.get());
  DCHECK(!stream->url().is_empty());
  streams_[stream->url()] = stream;
}

scoped_refptr<Stream> StreamRegistry::GetStream(const GURL& url) {
  DCHECK(CalledOnValidThread());
  StreamMap::const_iterator stream = streams_.find(url);
  if (stream != streams_.end())
    return stream->second;

  return NULL;
}

bool StreamRegistry::CloneStream(const GURL& url, const GURL& src_url) {
  DCHECK(CalledOnValidThread());
  scoped_refptr<Stream> stream(GetStream(src_url));
  if (stream.get()) {
    streams_[url] = stream;
    return true;
  }
  return false;
}

void StreamRegistry::UnregisterStream(const GURL& url) {
  DCHECK(CalledOnValidThread());

  StreamMap::iterator iter = streams_.find(url);
  if (iter == streams_.end())
    return;

  // Only update |total_memory_usage_| if |url| is NOT a Stream clone because
  // cloned streams do not update |total_memory_usage_|.
  if (iter->second->url() == url) {
    size_t buffered_bytes = iter->second->last_total_buffered_bytes();
    DCHECK_LE(buffered_bytes, total_memory_usage_);
    total_memory_usage_ -= buffered_bytes;
  }

  streams_.erase(url);
}

bool StreamRegistry::UpdateMemoryUsage(const GURL& url,
                                       size_t current_size,
                                       size_t increase) {
  DCHECK(CalledOnValidThread());

  StreamMap::iterator iter = streams_.find(url);
  // A Stream must be registered with its parent registry to get memory.
  if (iter == streams_.end())
    return false;

  size_t last_size = iter->second->last_total_buffered_bytes();
  DCHECK_LE(last_size, total_memory_usage_);
  size_t usage_of_others = total_memory_usage_ - last_size;
  DCHECK_LE(current_size, last_size);
  size_t current_total_memory_usage = usage_of_others + current_size;

  if (increase > max_memory_usage_ - current_total_memory_usage)
    return false;

  total_memory_usage_ = current_total_memory_usage + increase;
  return true;
}

}  // namespace content
