// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/shared_worker/worker_document_set.h"

#include "base/logging.h"

namespace content {

WorkerDocumentSet::WorkerDocumentSet() {
}

void WorkerDocumentSet::Add(BrowserMessageFilter* parent,
                            unsigned long long document_id,
                            int render_process_id,
                            int render_frame_id) {
  DocumentInfo info(parent, document_id, render_process_id, render_frame_id);
  document_set_.insert(info);
}

bool WorkerDocumentSet::Contains(BrowserMessageFilter* parent,
                                 unsigned long long document_id) const {
  for (const DocumentInfo& info : document_set_) {
    if (info.filter() == parent && info.document_id() == document_id)
      return true;
  }
  return false;
}

bool WorkerDocumentSet::ContainsExternalRenderer(
      int worker_process_id) const {
  for (const DocumentInfo& info : document_set_) {
    if (info.render_process_id() != worker_process_id)
      return true;
  }
  return false;
}

void WorkerDocumentSet::Remove(BrowserMessageFilter* parent,
                               unsigned long long document_id) {
  for (const DocumentInfo& info : document_set_) {
    if (info.filter() == parent && info.document_id() == document_id) {
      document_set_.erase(info);
      break;
    }
  }
  // Should not be duplicate copies in the document set.
  DCHECK(!Contains(parent, document_id));
}

void WorkerDocumentSet::RemoveAll(BrowserMessageFilter* parent) {
  for (DocumentInfoSet::iterator i = document_set_.begin();
       i != document_set_.end();) {
    // Note this idiom is somewhat tricky - calling document_set_.erase(iter)
    // invalidates any iterators that point to the element being removed, so
    // bump the iterator beyond the item being removed before calling erase.
    if (i->filter() == parent) {
      DocumentInfoSet::iterator item_to_delete = i++;
      document_set_.erase(item_to_delete);
    } else {
      ++i;
    }
  }
}

void WorkerDocumentSet::RemoveRenderFrame(int render_process_id,
                                          int render_frame_id) {
  for (DocumentInfoSet::iterator i = document_set_.begin();
       i != document_set_.end();) {
    if (i->render_process_id() == render_process_id &&
        i->render_frame_id() == render_frame_id) {
      i = document_set_.erase(i);
    } else {
      ++i;
    }
  }
}

WorkerDocumentSet::DocumentInfo::DocumentInfo(
    BrowserMessageFilter* filter, unsigned long long document_id,
    int render_process_id, int render_frame_id)
    : filter_(filter),
      document_id_(document_id),
      render_process_id_(render_process_id),
      render_frame_id_(render_frame_id) {
}

WorkerDocumentSet::~WorkerDocumentSet() {
}

}  // namespace content
