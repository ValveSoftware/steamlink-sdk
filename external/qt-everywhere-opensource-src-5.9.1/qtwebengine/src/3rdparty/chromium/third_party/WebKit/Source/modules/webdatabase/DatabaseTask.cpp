/*
 * Copyright (C) 2007, 2008, 2013 Apple Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
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

#include "modules/webdatabase/DatabaseTask.h"

#include "modules/webdatabase/Database.h"
#include "modules/webdatabase/DatabaseContext.h"
#include "modules/webdatabase/DatabaseThread.h"
#include "modules/webdatabase/StorageLog.h"

namespace blink {

DatabaseTask::DatabaseTask(Database* database, WaitableEvent* completeEvent)
    : m_database(database),
      m_completeEvent(completeEvent)
#if DCHECK_IS_ON()
      ,
      m_complete(false)
#endif
{
}

DatabaseTask::~DatabaseTask() {
#if DCHECK_IS_ON()
  DCHECK(m_complete || !m_completeEvent);
#endif
}

void DatabaseTask::run() {
// Database tasks are meant to be used only once, so make sure this one hasn't
// been performed before.
#if DCHECK_IS_ON()
  ASSERT(!m_complete);
#endif

  if (!m_completeEvent &&
      !m_database->getDatabaseContext()->databaseThread()->isDatabaseOpen(
          m_database.get())) {
    taskCancelled();
#if DCHECK_IS_ON()
    m_complete = true;
#endif
    return;
  }
#if DCHECK_IS_ON()
  STORAGE_DVLOG(1) << "Performing " << debugTaskName() << " " << this;
#endif
  m_database->resetAuthorizer();
  doPerformTask();

  if (m_completeEvent)
    m_completeEvent->signal();

#if DCHECK_IS_ON()
  m_complete = true;
#endif
}

// *** DatabaseOpenTask ***
// Opens the database file and verifies the version matches the expected
// version.

Database::DatabaseOpenTask::DatabaseOpenTask(Database* database,
                                             bool setVersionInNewDatabase,
                                             WaitableEvent* completeEvent,
                                             DatabaseError& error,
                                             String& errorMessage,
                                             bool& success)
    : DatabaseTask(database, completeEvent),
      m_setVersionInNewDatabase(setVersionInNewDatabase),
      m_error(error),
      m_errorMessage(errorMessage),
      m_success(success) {
  DCHECK(completeEvent);  // A task with output parameters is supposed to be
                          // synchronous.
}

void Database::DatabaseOpenTask::doPerformTask() {
  String errorMessage;
  m_success = database()->performOpenAndVerify(m_setVersionInNewDatabase,
                                               m_error, errorMessage);
  if (!m_success)
    m_errorMessage = errorMessage.isolatedCopy();
}

#if DCHECK_IS_ON()
const char* Database::DatabaseOpenTask::debugTaskName() const {
  return "DatabaseOpenTask";
}
#endif

// *** DatabaseCloseTask ***
// Closes the database.

Database::DatabaseCloseTask::DatabaseCloseTask(Database* database,
                                               WaitableEvent* completeEvent)
    : DatabaseTask(database, completeEvent) {}

void Database::DatabaseCloseTask::doPerformTask() {
  database()->close();
}

#if DCHECK_IS_ON()
const char* Database::DatabaseCloseTask::debugTaskName() const {
  return "DatabaseCloseTask";
}
#endif

// *** DatabaseTransactionTask ***
// Starts a transaction that will report its results via a callback.

Database::DatabaseTransactionTask::DatabaseTransactionTask(
    SQLTransactionBackend* transaction)
    : DatabaseTask(transaction->database(), 0), m_transaction(transaction) {}

Database::DatabaseTransactionTask::~DatabaseTransactionTask() {}

void Database::DatabaseTransactionTask::doPerformTask() {
  m_transaction->performNextStep();
}

void Database::DatabaseTransactionTask::taskCancelled() {
  // If the task is being destructed without the transaction ever being run,
  // then we must either have an error or an interruption. Give the
  // transaction a chance to clean up since it may not have been able to
  // run to its clean up state.

  // Transaction phase 2 cleanup. See comment on "What happens if a
  // transaction is interrupted?" at the top of SQLTransactionBackend.cpp.

  m_transaction->notifyDatabaseThreadIsShuttingDown();
}

#if DCHECK_IS_ON()
const char* Database::DatabaseTransactionTask::debugTaskName() const {
  return "DatabaseTransactionTask";
}
#endif

// *** DatabaseTableNamesTask ***
// Retrieves a list of all tables in the database - for WebInspector support.

Database::DatabaseTableNamesTask::DatabaseTableNamesTask(
    Database* database,
    WaitableEvent* completeEvent,
    Vector<String>& names)
    : DatabaseTask(database, completeEvent), m_tableNames(names) {
  DCHECK(completeEvent);  // A task with output parameters is supposed to be
                          // synchronous.
}

void Database::DatabaseTableNamesTask::doPerformTask() {
  m_tableNames = database()->performGetTableNames();
}

#if DCHECK_IS_ON()
const char* Database::DatabaseTableNamesTask::debugTaskName() const {
  return "DatabaseTableNamesTask";
}
#endif

}  // namespace blink
