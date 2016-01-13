// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/tracing/tracing_cache_backend.h"

#include "net/base/net_errors.h"

namespace disk_cache {

// Proxies entry objects created by the real underlying backend. Backend users
// will only see the proxy entries. It is necessary for recording the backend
// operations since often non-trivial work is invoked directly on entries.
class EntryProxy : public Entry, public base::RefCountedThreadSafe<EntryProxy> {
 public:
  EntryProxy(Entry *entry, TracingCacheBackend* backend);
  virtual void Doom() OVERRIDE;
  virtual void Close() OVERRIDE;
  virtual std::string GetKey() const OVERRIDE;
  virtual base::Time GetLastUsed() const OVERRIDE;
  virtual base::Time GetLastModified() const OVERRIDE;
  virtual int32 GetDataSize(int index) const OVERRIDE;
  virtual int ReadData(int index, int offset, IOBuffer* buf, int buf_len,
                       const CompletionCallback& callback) OVERRIDE;
  virtual int WriteData(int index, int offset, IOBuffer* buf, int buf_len,
                        const CompletionCallback& callback,
                        bool truncate) OVERRIDE;
  virtual int ReadSparseData(int64 offset, IOBuffer* buf, int buf_len,
                             const CompletionCallback& callback) OVERRIDE;
  virtual int WriteSparseData(int64 offset, IOBuffer* buf, int buf_len,
                              const CompletionCallback& callback) OVERRIDE;
  virtual int GetAvailableRange(int64 offset, int len, int64* start,
                                const CompletionCallback& callback) OVERRIDE;
  virtual bool CouldBeSparse() const OVERRIDE;
  virtual void CancelSparseIO() OVERRIDE;
  virtual int ReadyForSparseIO(const CompletionCallback& callback) OVERRIDE;

 private:
  friend class base::RefCountedThreadSafe<EntryProxy>;
  typedef TracingCacheBackend::Operation Operation;
  virtual ~EntryProxy();

  struct RwOpExtra {
    int index;
    int offset;
    int buf_len;
    bool truncate;
  };

  void RecordEvent(base::TimeTicks start_time, Operation op, RwOpExtra extra,
                   int result_to_record);
  void EntryOpComplete(base::TimeTicks start_time, Operation op,
                       RwOpExtra extra, const CompletionCallback& cb,
                       int result);
  Entry* entry_;
  base::WeakPtr<TracingCacheBackend> backend_;

  DISALLOW_COPY_AND_ASSIGN(EntryProxy);
};

EntryProxy::EntryProxy(Entry *entry, TracingCacheBackend* backend)
  : entry_(entry),
    backend_(backend->AsWeakPtr()) {
}

void EntryProxy::Doom() {
  // TODO(pasko): Record the event.
  entry_->Doom();
}

void EntryProxy::Close() {
  // TODO(pasko): Record the event.
  entry_->Close();
  Release();
}

std::string EntryProxy::GetKey() const {
  return entry_->GetKey();
}

base::Time EntryProxy::GetLastUsed() const {
  return entry_->GetLastUsed();
}

base::Time EntryProxy::GetLastModified() const {
  return entry_->GetLastModified();
}

int32 EntryProxy::GetDataSize(int index) const {
  return entry_->GetDataSize(index);
}

int EntryProxy::ReadData(int index, int offset, IOBuffer* buf, int buf_len,
                         const CompletionCallback& callback) {
  base::TimeTicks start_time = base::TimeTicks::Now();
  RwOpExtra extra;
  extra.index = index;
  extra.offset = offset;
  extra.buf_len = buf_len;
  extra.truncate = false;
  int rv = entry_->ReadData(
      index, offset, buf, buf_len,
      base::Bind(&EntryProxy::EntryOpComplete, this, start_time,
                 TracingCacheBackend::OP_READ, extra, callback));
  if (rv != net::ERR_IO_PENDING) {
    RecordEvent(start_time, TracingCacheBackend::OP_READ, extra, rv);
  }
  return rv;
}

int EntryProxy::WriteData(int index, int offset, IOBuffer* buf, int buf_len,
                      const CompletionCallback& callback,
                      bool truncate) {
  base::TimeTicks start_time = base::TimeTicks::Now();
  RwOpExtra extra;
  extra.index = index;
  extra.offset = offset;
  extra.buf_len = buf_len;
  extra.truncate = truncate;
  int rv = entry_->WriteData(index, offset, buf, buf_len,
      base::Bind(&EntryProxy::EntryOpComplete, this, start_time,
                 TracingCacheBackend::OP_WRITE, extra, callback),
      truncate);
  if (rv != net::ERR_IO_PENDING) {
    RecordEvent(start_time, TracingCacheBackend::OP_WRITE, extra, rv);
  }
  return rv;
}

int EntryProxy::ReadSparseData(int64 offset, IOBuffer* buf, int buf_len,
                           const CompletionCallback& callback) {
  // TODO(pasko): Record the event.
  return entry_->ReadSparseData(offset, buf, buf_len, callback);
}

int EntryProxy::WriteSparseData(int64 offset, IOBuffer* buf, int buf_len,
                            const CompletionCallback& callback) {
  // TODO(pasko): Record the event.
  return entry_->WriteSparseData(offset, buf, buf_len, callback);
}

int EntryProxy::GetAvailableRange(int64 offset, int len, int64* start,
                              const CompletionCallback& callback) {
  return entry_->GetAvailableRange(offset, len, start, callback);
}

bool EntryProxy::CouldBeSparse() const {
  return entry_->CouldBeSparse();
}

void EntryProxy::CancelSparseIO() {
  return entry_->CancelSparseIO();
}

int EntryProxy::ReadyForSparseIO(const CompletionCallback& callback) {
  return entry_->ReadyForSparseIO(callback);
}

void EntryProxy::RecordEvent(base::TimeTicks start_time, Operation op,
                             RwOpExtra extra, int result_to_record) {
  // TODO(pasko): Implement.
}

void EntryProxy::EntryOpComplete(base::TimeTicks start_time, Operation op,
                                 RwOpExtra extra, const CompletionCallback& cb,
                                 int result) {
  RecordEvent(start_time, op, extra, result);
  if (!cb.is_null()) {
    cb.Run(result);
  }
}

EntryProxy::~EntryProxy() {
  if (backend_.get()) {
    backend_->OnDeleteEntry(entry_);
  }
}

TracingCacheBackend::TracingCacheBackend(scoped_ptr<Backend> backend)
  : backend_(backend.Pass()) {
}

TracingCacheBackend::~TracingCacheBackend() {
}

net::CacheType TracingCacheBackend::GetCacheType() const {
  return backend_->GetCacheType();
}

int32 TracingCacheBackend::GetEntryCount() const {
  return backend_->GetEntryCount();
}

void TracingCacheBackend::RecordEvent(base::TimeTicks start_time, Operation op,
                                      std::string key, Entry* entry, int rv) {
  // TODO(pasko): Implement.
}

EntryProxy* TracingCacheBackend::FindOrCreateEntryProxy(Entry* entry) {
  EntryProxy* entry_proxy;
  EntryToProxyMap::iterator it = open_entries_.find(entry);
  if (it != open_entries_.end()) {
    entry_proxy = it->second;
    entry_proxy->AddRef();
    return entry_proxy;
  }
  entry_proxy = new EntryProxy(entry, this);
  entry_proxy->AddRef();
  open_entries_[entry] = entry_proxy;
  return entry_proxy;
}

void TracingCacheBackend::OnDeleteEntry(Entry* entry) {
  EntryToProxyMap::iterator it = open_entries_.find(entry);
  if (it != open_entries_.end()) {
    open_entries_.erase(it);
  }
}

void TracingCacheBackend::BackendOpComplete(base::TimeTicks start_time,
                                            Operation op,
                                            std::string key,
                                            Entry** entry,
                                            const CompletionCallback& callback,
                                            int result) {
  RecordEvent(start_time, op, key, *entry, result);
  if (*entry) {
    *entry = FindOrCreateEntryProxy(*entry);
  }
  if (!callback.is_null()) {
    callback.Run(result);
  }
}

net::CompletionCallback TracingCacheBackend::BindCompletion(
    Operation op, base::TimeTicks start_time, const std::string& key,
    Entry **entry, const net::CompletionCallback& cb) {
  return base::Bind(&TracingCacheBackend::BackendOpComplete,
                    AsWeakPtr(), start_time, op, key, entry, cb);
}

int TracingCacheBackend::OpenEntry(const std::string& key, Entry** entry,
                                   const CompletionCallback& callback) {
  DCHECK(*entry == NULL);
  base::TimeTicks start_time = base::TimeTicks::Now();
  int rv = backend_->OpenEntry(key, entry,
                               BindCompletion(OP_OPEN, start_time, key, entry,
                                              callback));
  if (rv != net::ERR_IO_PENDING) {
    RecordEvent(start_time, OP_OPEN, key, *entry, rv);
    if (*entry) {
      *entry = FindOrCreateEntryProxy(*entry);
    }
  }
  return rv;
}

int TracingCacheBackend::CreateEntry(const std::string& key, Entry** entry,
                                     const CompletionCallback& callback) {
  base::TimeTicks start_time = base::TimeTicks::Now();
  int rv = backend_->CreateEntry(key, entry,
                                 BindCompletion(OP_CREATE, start_time, key,
                                                entry, callback));
  if (rv != net::ERR_IO_PENDING) {
    RecordEvent(start_time, OP_CREATE, key, *entry, rv);
    if (*entry) {
      *entry = FindOrCreateEntryProxy(*entry);
    }
  }
  return rv;
}

int TracingCacheBackend::DoomEntry(const std::string& key,
                                   const CompletionCallback& callback) {
  base::TimeTicks start_time = base::TimeTicks::Now();
  int rv = backend_->DoomEntry(key, BindCompletion(OP_DOOM_ENTRY,
                                                   start_time, key, NULL,
                                                   callback));
  if (rv != net::ERR_IO_PENDING) {
    RecordEvent(start_time, OP_DOOM_ENTRY, key, NULL, rv);
  }
  return rv;
}

int TracingCacheBackend::DoomAllEntries(const CompletionCallback& callback) {
  return backend_->DoomAllEntries(callback);
}

int TracingCacheBackend::DoomEntriesBetween(base::Time initial_time,
                                            base::Time end_time,
                                            const CompletionCallback& cb) {
  return backend_->DoomEntriesBetween(initial_time, end_time, cb);
}

int TracingCacheBackend::DoomEntriesSince(base::Time initial_time,
                                          const CompletionCallback& callback) {
  return backend_->DoomEntriesSince(initial_time, callback);
}

int TracingCacheBackend::OpenNextEntry(void** iter, Entry** next_entry,
                                       const CompletionCallback& callback) {
  return backend_->OpenNextEntry(iter, next_entry, callback);
}

void TracingCacheBackend::EndEnumeration(void** iter) {
  return backend_->EndEnumeration(iter);
}

void TracingCacheBackend::GetStats(StatsItems* stats) {
  return backend_->GetStats(stats);
}

void TracingCacheBackend::OnExternalCacheHit(const std::string& key) {
  return backend_->OnExternalCacheHit(key);
}

}  // namespace disk_cache
