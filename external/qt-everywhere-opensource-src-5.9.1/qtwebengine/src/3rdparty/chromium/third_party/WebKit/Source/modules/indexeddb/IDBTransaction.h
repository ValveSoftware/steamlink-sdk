/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef IDBTransaction_h
#define IDBTransaction_h

#include "bindings/core/v8/ActiveScriptWrappable.h"
#include "core/dom/ActiveDOMObject.h"
#include "core/dom/DOMStringList.h"
#include "core/events/EventListener.h"
#include "modules/EventModules.h"
#include "modules/EventTargetModules.h"
#include "modules/ModulesExport.h"
#include "modules/indexeddb/IDBMetadata.h"
#include "modules/indexeddb/IndexedDB.h"
#include "platform/heap/Handle.h"
#include "public/platform/modules/indexeddb/WebIDBDatabase.h"
#include "public/platform/modules/indexeddb/WebIDBTypes.h"
#include "wtf/HashSet.h"
#include "wtf/Vector.h"

namespace blink {

class DOMException;
class ExecutionContext;
class ExceptionState;
class IDBDatabase;
class IDBIndex;
class IDBObjectStore;
class IDBOpenDBRequest;
class IDBRequest;
class ScriptState;

class MODULES_EXPORT IDBTransaction final : public EventTargetWithInlineData,
                                            public ActiveScriptWrappable,
                                            public ActiveDOMObject {
  USING_GARBAGE_COLLECTED_MIXIN(IDBTransaction);
  DEFINE_WRAPPERTYPEINFO();

 public:
  static IDBTransaction* createNonVersionChange(ScriptState*,
                                                int64_t,
                                                const HashSet<String>& scope,
                                                WebIDBTransactionMode,
                                                IDBDatabase*);
  static IDBTransaction* createVersionChange(
      ExecutionContext*,
      int64_t,
      IDBDatabase*,
      IDBOpenDBRequest*,
      const IDBDatabaseMetadata& oldMetadata);
  ~IDBTransaction() override;
  DECLARE_VIRTUAL_TRACE();

  static WebIDBTransactionMode stringToMode(const String&);

  // When the connection is closed backend will be 0.
  WebIDBDatabase* backendDB() const;

  int64_t id() const { return m_id; }
  bool isActive() const { return m_state == Active; }
  bool isFinished() const { return m_state == Finished; }
  bool isFinishing() const { return m_state == Finishing; }
  bool isReadOnly() const { return m_mode == WebIDBTransactionModeReadOnly; }
  bool isVersionChange() const {
    return m_mode == WebIDBTransactionModeVersionChange;
  }

  // Implement the IDBTransaction IDL
  const String& mode() const;
  DOMStringList* objectStoreNames() const;
  IDBDatabase* db() const { return m_database.get(); }
  DOMException* error() const { return m_error; }
  IDBObjectStore* objectStore(const String& name, ExceptionState&);
  void abort(ExceptionState&);

  void registerRequest(IDBRequest*);
  void unregisterRequest(IDBRequest*);

  // The methods below are called right before the changes are applied to the
  // database's metadata. We use this unusual sequencing because some of the
  // methods below need to access the metadata values before the change, and
  // following the same lifecycle for all methods makes the code easier to
  // reason about.
  void objectStoreCreated(const String& name, IDBObjectStore*);
  void objectStoreDeleted(const int64_t objectStoreId, const String& name);
  void objectStoreRenamed(const String& oldName, const String& newName);
  // Called when deleting an index whose IDBIndex had been created.
  void indexDeleted(IDBIndex*);

  void setActive(bool);
  void setError(DOMException*);

  DEFINE_ATTRIBUTE_EVENT_LISTENER(abort);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(complete);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(error);

  void onAbort(DOMException*);
  void onComplete();

  // EventTarget
  const AtomicString& interfaceName() const override;
  ExecutionContext* getExecutionContext() const override;

  // ScriptWrappable
  bool hasPendingActivity() const final;

  // For use in IDBObjectStore.isNewlyCreated(). The rest of the code should use
  // IDBObjectStore.isNewlyCreated() instead of calling this method directly.
  int64_t oldMaxObjectStoreId() const {
    DCHECK(isVersionChange());
    return m_oldDatabaseMetadata.maxObjectStoreId;
  }

 protected:
  // EventTarget
  DispatchEventResult dispatchEventInternal(Event*) override;

 private:
  using IDBObjectStoreMap = HeapHashMap<String, Member<IDBObjectStore>>;

  // For non-upgrade transactions.
  IDBTransaction(ScriptState*,
                 int64_t,
                 const HashSet<String>& scope,
                 WebIDBTransactionMode,
                 IDBDatabase*);

  // For upgrade transactions.
  IDBTransaction(ExecutionContext*,
                 int64_t,
                 IDBDatabase*,
                 IDBOpenDBRequest*,
                 const IDBDatabaseMetadata&);

  void enqueueEvent(Event*);

  // Called when a transaction is aborted.
  void abortOutstandingRequests();
  void revertDatabaseMetadata();

  // Called when a transaction is completed (committed or aborted).
  void finished();

  enum State {
    Inactive,   // Created or started, but not in an event callback
    Active,     // Created or started, in creation scope or an event callback
    Finishing,  // In the process of aborting or completing.
    Finished,   // No more events will fire and no new requests may be filed.
  };

  const int64_t m_id;
  Member<IDBDatabase> m_database;
  Member<IDBOpenDBRequest> m_openDBRequest;
  const WebIDBTransactionMode m_mode;

  // The names of the object stores that make up this transaction's scope.
  //
  // Transactions may not access object stores outside their scope.
  //
  // The scope of versionchange transactions is the entire database. We
  // represent this case with an empty m_scope, because copying all the store
  // names would waste both time and memory.
  //
  // Using object store names to represent a transaction's scope is safe
  // because object stores cannot be renamed in non-versionchange
  // transactions.
  const HashSet<String> m_scope;

  State m_state = Active;
  bool m_hasPendingActivity = true;
  Member<DOMException> m_error;

  HeapListHashSet<Member<IDBRequest>> m_requestList;

#if DCHECK_IS_ON()
  bool m_finishCalled = false;
#endif  // DCHECK_IS_ON()

  // Caches the IDBObjectStore instances returned by the objectStore() method.
  //
  // The spec requires that a transaction's objectStore() returns the same
  // IDBObjectStore instance for a specific store, so this cache is necessary
  // for correctness.
  //
  // objectStore() throws for completed/aborted transactions, so this is not
  // used after a transaction is finished, and can be cleared.
  IDBObjectStoreMap m_objectStoreMap;

  // The metadata of object stores when they are opened by this transaction.
  //
  // Only valid for versionchange transactions.
  HeapHashMap<Member<IDBObjectStore>, RefPtr<IDBObjectStoreMetadata>>
      m_oldStoreMetadata;

  // The metadata of deleted object stores without IDBObjectStore instances.
  //
  // Only valid for versionchange transactions.
  Vector<RefPtr<IDBObjectStoreMetadata>> m_deletedObjectStores;

  // Tracks the indexes deleted by this transaction.
  //
  // This set only includes indexes that were created before this transaction,
  // and were deleted during this transaction. Once marked for deletion, these
  // indexes are removed from their object stores' index maps, so we need to
  // stash them somewhere else in case the transaction gets aborted.
  //
  // This set does not include indexes created and deleted during this
  // transaction, because we don't need to change their metadata when the
  // transaction aborts, as they will still be marked for deletion.
  //
  // Only valid for versionchange transactions.
  HeapVector<Member<IDBIndex>> m_deletedIndexes;

  // Shallow snapshot of the database metadata when the transaction starts.
  //
  // This does not include a snapshot of the database's object store / index
  // metadata.
  //
  // Only valid for versionchange transactions.
  IDBDatabaseMetadata m_oldDatabaseMetadata;
};

}  // namespace blink

#endif  // IDBTransaction_h
