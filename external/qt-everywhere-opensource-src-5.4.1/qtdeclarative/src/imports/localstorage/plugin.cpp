/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/
#include <QStringList>
#include <QtQml/qqmlextensionplugin.h>
#include <QtQml/qqml.h>
#include <private/qqmlengine_p.h>
#include <QDebug>
#include <private/qv8engine_p.h>
#include <QtSql/qsqldatabase.h>
#include <QtSql/qsqlquery.h>
#include <QtSql/qsqlerror.h>
#include <QtSql/qsqlrecord.h>
#include <QtSql/qsqlfield.h>
#include <QtCore/qstandardpaths.h>
#include <QtCore/qstack.h>
#include <QtCore/qcryptographichash.h>
#include <QtCore/qsettings.h>
#include <QtCore/qdir.h>
#include <private/qv4sqlerrors_p.h>
#include <private/qv4engine_p.h>
#include <private/qv4object_p.h>
#include <private/qv4functionobject_p.h>
#include <private/qv4objectproto_p.h>
#include <private/qv4scopedvalue_p.h>

using namespace QV4;

#define V4THROW_SQL(error, desc) { \
    QV4::Scoped<String> v(scope, scope.engine->newString(desc)); \
    QV4::Scoped<Object> ex(scope, scope.engine->newErrorObject(v)); \
    ex->put(QV4::ScopedString(scope, scope.engine->newIdentifier(QStringLiteral("code"))).getPointer(), QV4::ScopedValue(scope, Primitive::fromInt32(error))); \
    ctx->throwError(ex); \
    return Encode::undefined(); \
}

#define V4THROW_SQL2(error, desc) { \
    QV4::Scoped<String> v(scope, scope.engine->newString(desc)); \
    QV4::Scoped<Object> ex(scope, scope.engine->newErrorObject(v)); \
    ex->put(QV4::ScopedString(scope, scope.engine->newIdentifier(QStringLiteral("code"))).getPointer(), QV4::ScopedValue(scope, Primitive::fromInt32(error))); \
    args->setReturnValue(ctx->throwError(ex)); \
    return; \
}

#define V4THROW_REFERENCE(string) { \
    QV4::Scoped<String> v(scope, scope.engine->newString(string)); \
    ctx->throwReferenceError(v); \
    return Encode::undefined(); \
}


class QQmlSqlDatabaseData : public QV8Engine::Deletable
{
public:
    QQmlSqlDatabaseData(QV8Engine *engine);
    ~QQmlSqlDatabaseData();

    PersistentValue databaseProto;
    PersistentValue queryProto;
    PersistentValue rowsProto;
};

V8_DEFINE_EXTENSION(QQmlSqlDatabaseData, databaseData)

class QQmlSqlDatabaseWrapper : public Object
{
public:
    enum Type { Database, Query, Rows };
    struct Data : Object::Data {
        Data(ExecutionEngine *e)
            : Object::Data(e)
        {
            setVTable(staticVTable());
            type = Database;
        }

        Type type;
        QSqlDatabase database;

        QString version; // type == Database

        bool inTransaction; // type == Query
        bool readonly;   // type == Query

        QSqlQuery sqlQuery; // type == Rows
        bool forwardOnly; // type == Rows
    };
    V4_OBJECT(Object)

    static QQmlSqlDatabaseWrapper *create(QV8Engine *engine)
    {
        QV4::ExecutionEngine *e = QV8Engine::getV4(engine);
        return e->memoryManager->alloc<QQmlSqlDatabaseWrapper>(e);
    }

    ~QQmlSqlDatabaseWrapper() {
    }

    static ReturnedValue getIndexed(Managed *m, uint index, bool *hasProperty);
    static void destroy(Managed *that) {
        static_cast<QQmlSqlDatabaseWrapper *>(that)->~QQmlSqlDatabaseWrapper();
    }
};

DEFINE_OBJECT_VTABLE(QQmlSqlDatabaseWrapper);

static ReturnedValue qmlsqldatabase_version(CallContext *ctx)
{
    QV4::Scope scope(ctx);
    QV4::Scoped<QQmlSqlDatabaseWrapper> r(scope, ctx->d()->callData->thisObject.as<QQmlSqlDatabaseWrapper>());
    if (!r || r->d()->type != QQmlSqlDatabaseWrapper::Database)
        V4THROW_REFERENCE("Not a SQLDatabase object");

    return Encode(scope.engine->newString(r->d()->version));
}

static ReturnedValue qmlsqldatabase_rows_length(CallContext *ctx)
{
    QV4::Scope scope(ctx);
    QV4::Scoped<QQmlSqlDatabaseWrapper> r(scope, ctx->d()->callData->thisObject.as<QQmlSqlDatabaseWrapper>());
    if (!r || r->d()->type != QQmlSqlDatabaseWrapper::Rows)
        V4THROW_REFERENCE("Not a SQLDatabase::Rows object");

    int s = r->d()->sqlQuery.size();
    if (s < 0) {
        // Inefficient
        if (r->d()->sqlQuery.last()) {
            s = r->d()->sqlQuery.at() + 1;
        } else {
            s = 0;
        }
    }
    return Encode(s);
}

static ReturnedValue qmlsqldatabase_rows_forwardOnly(CallContext *ctx)
{
    QV4::Scope scope(ctx);
    QV4::Scoped<QQmlSqlDatabaseWrapper> r(scope, ctx->d()->callData->thisObject.as<QQmlSqlDatabaseWrapper>());
    if (!r || r->d()->type != QQmlSqlDatabaseWrapper::Rows)
        V4THROW_REFERENCE("Not a SQLDatabase::Rows object");
    return Encode(r->d()->sqlQuery.isForwardOnly());
}

static ReturnedValue qmlsqldatabase_rows_setForwardOnly(CallContext *ctx)
{
    QV4::Scope scope(ctx);
    QV4::Scoped<QQmlSqlDatabaseWrapper> r(scope, ctx->d()->callData->thisObject.as<QQmlSqlDatabaseWrapper>());
    if (!r || r->d()->type != QQmlSqlDatabaseWrapper::Rows)
        V4THROW_REFERENCE("Not a SQLDatabase::Rows object");
    if (ctx->d()->callData->argc < 1)
        return ctx->throwTypeError();

    r->d()->sqlQuery.setForwardOnly(ctx->d()->callData->args[0].toBoolean());
    return Encode::undefined();
}

QQmlSqlDatabaseData::~QQmlSqlDatabaseData()
{
}

static QString qmlsqldatabase_databasesPath(QV8Engine *engine)
{
    return engine->engine()->offlineStoragePath() +
           QDir::separator() + QLatin1String("Databases");
}

static void qmlsqldatabase_initDatabasesPath(QV8Engine *engine)
{
    QString databasesPath = qmlsqldatabase_databasesPath(engine);
    if (!QDir().mkpath(databasesPath))
        qWarning() << "LocalStorage: can't create path - " << databasesPath;
}

static QString qmlsqldatabase_databaseFile(const QString& connectionName, QV8Engine *engine)
{
    return qmlsqldatabase_databasesPath(engine) + QDir::separator() + connectionName;
}

static ReturnedValue qmlsqldatabase_rows_index(QQmlSqlDatabaseWrapper *r, ExecutionEngine *v4, quint32 index, bool *hasProperty = 0)
{
    Scope scope(v4);
    QV8Engine *v8 = v4->v8Engine;

    if (r->d()->sqlQuery.at() == (int)index || r->d()->sqlQuery.seek(index)) {
        QSqlRecord record = r->d()->sqlQuery.record();
        // XXX optimize
        Scoped<Object> row(scope, v4->newObject());
        for (int ii = 0; ii < record.count(); ++ii) {
            QVariant v = record.value(ii);
            ScopedString s(scope, v4->newIdentifier(record.fieldName(ii)));
            ScopedValue val(scope, v.isNull() ? Encode::null() : v8->fromVariant(v));
            row->put(s.getPointer(), val);
        }
        if (hasProperty)
            *hasProperty = true;
        return row.asReturnedValue();
    } else {
        if (hasProperty)
            *hasProperty = false;
        return Encode::undefined();
    }
}

ReturnedValue QQmlSqlDatabaseWrapper::getIndexed(Managed *m, uint index, bool *hasProperty)
{
    QV4::Scope scope(m->engine());
    Q_ASSERT(m->as<QQmlSqlDatabaseWrapper>());
    QV4::Scoped<QQmlSqlDatabaseWrapper> r(scope, static_cast<QQmlSqlDatabaseWrapper *>(m));
    if (!r || r->d()->type != QQmlSqlDatabaseWrapper::Rows)
        return Object::getIndexed(m, index, hasProperty);

    return qmlsqldatabase_rows_index(r, m->engine(), index, hasProperty);
}

static ReturnedValue qmlsqldatabase_rows_item(CallContext *ctx)
{
    QV4::Scope scope(ctx);
    QV4::Scoped<QQmlSqlDatabaseWrapper> r(scope, ctx->d()->callData->thisObject.as<QQmlSqlDatabaseWrapper>());
    if (!r || r->d()->type != QQmlSqlDatabaseWrapper::Rows)
        V4THROW_REFERENCE("Not a SQLDatabase::Rows object");

    return qmlsqldatabase_rows_index(r, scope.engine, ctx->d()->callData->argc ? ctx->d()->callData->args[0].toUInt32() : 0);
}

static ReturnedValue qmlsqldatabase_executeSql(CallContext *ctx)
{
    QV4::Scope scope(ctx);
    QV4::Scoped<QQmlSqlDatabaseWrapper> r(scope, ctx->d()->callData->thisObject.as<QQmlSqlDatabaseWrapper>());
    if (!r || r->d()->type != QQmlSqlDatabaseWrapper::Query)
        V4THROW_REFERENCE("Not a SQLDatabase::Query object");

    QV8Engine *engine = scope.engine->v8Engine;

    if (!r->d()->inTransaction)
        V4THROW_SQL(SQLEXCEPTION_DATABASE_ERR,QQmlEngine::tr("executeSql called outside transaction()"));

    QSqlDatabase db = r->d()->database;

    QString sql = ctx->d()->callData->argc ? ctx->d()->callData->args[0].toQString() : QString();

    if (r->d()->readonly && !sql.startsWith(QLatin1String("SELECT"),Qt::CaseInsensitive)) {
        V4THROW_SQL(SQLEXCEPTION_SYNTAX_ERR, QQmlEngine::tr("Read-only Transaction"));
    }

    QSqlQuery query(db);
    bool err = false;

    ScopedValue result(scope, Primitive::undefinedValue());

    if (query.prepare(sql)) {
        if (ctx->d()->callData->argc > 1) {
            ScopedValue values(scope, ctx->d()->callData->args[1]);
            if (values->asArrayObject()) {
                ScopedArrayObject array(scope, values);
                quint32 size = array->getLength();
                QV4::ScopedValue v(scope);
                for (quint32 ii = 0; ii < size; ++ii)
                    query.bindValue(ii, engine->toVariant((v = array->getIndexed(ii)), -1));
            } else if (values->asObject()) {
                ScopedObject object(scope, values);
                ObjectIterator it(scope, object, ObjectIterator::WithProtoChain|ObjectIterator::EnumerableOnly);
                ScopedValue key(scope);
                QV4::ScopedValue val(scope);
                while (1) {
                    key = it.nextPropertyName(val);
                    if (key->isNull())
                        break;
                    QVariant v = engine->toVariant(val, -1);
                    if (key->isString()) {
                        query.bindValue(key->stringValue()->toQString(), v);
                    } else {
                        assert(key->isInteger());
                        query.bindValue(key->integerValue(), v);
                    }
                }
            } else {
                query.bindValue(0, engine->toVariant(values, -1));
            }
        }
        if (query.exec()) {
            QV4::Scoped<QQmlSqlDatabaseWrapper> rows(scope, QQmlSqlDatabaseWrapper::create(engine));
            QV4::ScopedObject p(scope, databaseData(engine)->rowsProto.value());
            rows->setPrototype(p.getPointer());
            rows->d()->type = QQmlSqlDatabaseWrapper::Rows;
            rows->d()->database = db;
            rows->d()->sqlQuery = query;

            Scoped<Object> resultObject(scope, scope.engine->newObject());
            result = resultObject.asReturnedValue();
            // XXX optimize
            ScopedString s(scope);
            ScopedValue v(scope);
            resultObject->put((s = scope.engine->newIdentifier("rowsAffected")).getPointer(), (v = Primitive::fromInt32(query.numRowsAffected())));
            resultObject->put((s = scope.engine->newIdentifier("insertId")).getPointer(), (v = engine->toString(query.lastInsertId().toString())));
            resultObject->put((s = scope.engine->newIdentifier("rows")).getPointer(), rows);
        } else {
            err = true;
        }
    } else {
        err = true;
    }
    if (err)
        V4THROW_SQL(SQLEXCEPTION_DATABASE_ERR,query.lastError().text());

    return result.asReturnedValue();
}

struct TransactionRollback {
    QSqlDatabase *db;
    bool *inTransactionFlag;

    TransactionRollback(QSqlDatabase *database, bool *transactionFlag)
        : db(database)
        , inTransactionFlag(transactionFlag)
    {
        if (inTransactionFlag)
            *inTransactionFlag = true;
    }

    ~TransactionRollback()
    {
        if (inTransactionFlag)
            *inTransactionFlag = false;
        if (db)
            db->rollback();
    }

    void clear() {
        db = 0;
        if (inTransactionFlag)
            *inTransactionFlag = false;
        inTransactionFlag = 0;
    }
};


static ReturnedValue qmlsqldatabase_changeVersion(CallContext *ctx)
{
    if (ctx->d()->callData->argc < 2)
        return Encode::undefined();

    Scope scope(ctx);

    Scoped<QQmlSqlDatabaseWrapper> r(scope, ctx->d()->callData->thisObject);
    if (!r || r->d()->type != QQmlSqlDatabaseWrapper::Database)
        V4THROW_REFERENCE("Not a SQLDatabase object");

    QV8Engine *engine = scope.engine->v8Engine;

    QSqlDatabase db = r->d()->database;
    QString from_version = ctx->d()->callData->args[0].toQString();
    QString to_version = ctx->d()->callData->args[1].toQString();
    Scoped<FunctionObject> callback(scope, ctx->argument(2));

    if (from_version != r->d()->version)
        V4THROW_SQL(SQLEXCEPTION_VERSION_ERR, QQmlEngine::tr("Version mismatch: expected %1, found %2").arg(from_version).arg(r->d()->version));

    Scoped<QQmlSqlDatabaseWrapper> w(scope, QQmlSqlDatabaseWrapper::create(engine));
    ScopedObject p(scope, databaseData(engine)->queryProto.value());
    w->setPrototype(p.getPointer());
    w->d()->type = QQmlSqlDatabaseWrapper::Query;
    w->d()->database = db;
    w->d()->version = r->d()->version;

    bool ok = true;
    if (!!callback) {
        ok = false;
        db.transaction();

        ScopedCallData callData(scope, 1);
        callData->thisObject = engine->global();
        callData->args[0] = w;

        TransactionRollback rollbackOnException(&db, &w->d()->inTransaction);
        callback->call(callData);
        rollbackOnException.clear();
        if (!db.commit()) {
            db.rollback();
            V4THROW_SQL(SQLEXCEPTION_UNKNOWN_ERR,QQmlEngine::tr("SQL transaction failed"));
        } else {
            ok = true;
        }
    }

    if (ok) {
        w->d()->version = to_version;
#ifndef QT_NO_SETTINGS
        QSettings ini(qmlsqldatabase_databaseFile(db.connectionName(),engine) + QLatin1String(".ini"), QSettings::IniFormat);
        ini.setValue(QLatin1String("Version"), to_version);
#endif
    }

    return Encode::undefined();
}

static ReturnedValue qmlsqldatabase_transaction_shared(CallContext *ctx, bool readOnly)
{
    QV4::Scope scope(ctx);
    QV4::Scoped<QQmlSqlDatabaseWrapper> r(scope, ctx->d()->callData->thisObject.as<QQmlSqlDatabaseWrapper>());
    if (!r || r->d()->type != QQmlSqlDatabaseWrapper::Database)
        V4THROW_REFERENCE("Not a SQLDatabase object");

    QV8Engine *engine = scope.engine->v8Engine;

    FunctionObject *callback = ctx->d()->callData->argc ? ctx->d()->callData->args[0].asFunctionObject() : 0;
    if (!callback)
        V4THROW_SQL(SQLEXCEPTION_UNKNOWN_ERR, QQmlEngine::tr("transaction: missing callback"));

    QSqlDatabase db = r->d()->database;

    Scoped<QQmlSqlDatabaseWrapper> w(scope, QQmlSqlDatabaseWrapper::create(engine));
    QV4::ScopedObject p(scope, databaseData(engine)->queryProto.value());
    w->setPrototype(p.getPointer());
    w->d()->type = QQmlSqlDatabaseWrapper::Query;
    w->d()->database = db;
    w->d()->version = r->d()->version;
    w->d()->readonly = readOnly;

    db.transaction();
    if (callback) {
        ScopedCallData callData(scope, 1);
        callData->thisObject = engine->global();
        callData->args[0] = w;
        TransactionRollback rollbackOnException(&db, &w->d()->inTransaction);
        callback->call(callData);
        rollbackOnException.clear();

        if (!db.commit())
            db.rollback();
    }

    return Encode::undefined();
}

static ReturnedValue qmlsqldatabase_transaction(CallContext *ctx)
{
    return qmlsqldatabase_transaction_shared(ctx, false);
}

static ReturnedValue qmlsqldatabase_read_transaction(CallContext *ctx)
{
    return qmlsqldatabase_transaction_shared(ctx, true);
}

QQmlSqlDatabaseData::QQmlSqlDatabaseData(QV8Engine *engine)
{
    ExecutionEngine *v4 = QV8Engine::getV4(engine);
    Scope scope(v4);
    {
        Scoped<Object> proto(scope, v4->newObject());
        proto->defineDefaultProperty(QStringLiteral("transaction"), qmlsqldatabase_transaction);
        proto->defineDefaultProperty(QStringLiteral("readTransaction"), qmlsqldatabase_read_transaction);
        proto->defineAccessorProperty(QStringLiteral("version"), qmlsqldatabase_version, 0);
        proto->defineDefaultProperty(QStringLiteral("changeVersion"), qmlsqldatabase_changeVersion);
        databaseProto = proto;
    }

    {
        Scoped<Object> proto(scope, v4->newObject());
        proto->defineDefaultProperty(QStringLiteral("executeSql"), qmlsqldatabase_executeSql);
        queryProto = proto;
    }
    {
        Scoped<Object> proto(scope, v4->newObject());
        proto->defineDefaultProperty(QStringLiteral("item"), qmlsqldatabase_rows_item);
        proto->defineAccessorProperty(QStringLiteral("length"), qmlsqldatabase_rows_length, 0);
        proto->defineAccessorProperty(QStringLiteral("forwardOnly"),
                                      qmlsqldatabase_rows_forwardOnly, qmlsqldatabase_rows_setForwardOnly);
        rowsProto = proto;
    }
}

/*
HTML5 "spec" says "rs.rows[n]", but WebKit only impelments "rs.rows.item(n)". We do both (and property iterator).
We add a "forwardOnly" property that stops Qt caching results (code promises to only go forward
through the data.
*/


/*!
    \qmlmodule QtQuick.LocalStorage 2
    \title Qt Quick Local Storage QML Types
    \ingroup qmlmodules
    \brief Provides a JavaScript object singleton type for accessing a local
    SQLite database

    This is a singleton type for reading and writing to SQLite databases.


    \section1 Methods

    \list
    \li object \b{\l{#openDatabaseSync}{openDatabaseSync}}(string name, string version, string description, int estimated_size, jsobject callback(db))
    \endlist


    \section1 Detailed Description

    To use the types in this module, import the module and call the
    relevant functions using the \c LocalStorage type:

    \code
    import QtQuick.LocalStorage 2.0
    import QtQuick 2.0

    Item {
        Component.onCompleted: {
            var db = LocalStorage.openDatabaseSync(...)
        }
    }
    \endcode


These databases are user-specific and QML-specific, but accessible to all QML applications.
They are stored in the \c Databases subdirectory
of QQmlEngine::offlineStoragePath(), currently as SQLite databases.

Database connections are automatically closed during Javascript garbage collection.

The API can be used from JavaScript functions in your QML:

\snippet localstorage/localstorage/hello.qml 0

The API conforms to the Synchronous API of the HTML5 Web Database API,
\link http://www.w3.org/TR/2009/WD-webdatabase-20091029/ W3C Working Draft 29 October 2009\endlink.

The \l{Qt Quick Examples - Local Storage}{SQL Local Storage example} demonstrates the basics of
using the Offline Storage API.

\section3 Open or create a databaseData
\code
import QtQuick.LocalStorage 2.0 as Sql

db = Sql.openDatabaseSync(identifier, version, description, estimated_size, callback(db))
\endcode
The above code returns the database identified by \e identifier. If the database does not already exist, it
is created, and the function \e callback is called with the database as a parameter. \e description
and \e estimated_size are written to the INI file (described below), but are otherwise currently
unused.

May throw exception with code property SQLException.DATABASE_ERR, or SQLException.VERSION_ERR.

When a database is first created, an INI file is also created specifying its characteristics:

\table
\header \li \b {Key} \li \b {Value}
\row \li Name \li The name of the database passed to \c openDatabase()
\row \li Version \li The version of the database passed to \c openDatabase()
\row \li Description \li The description of the database passed to \c openDatabase()
\row \li EstimatedSize \li The estimated size (in bytes) of the database passed to \c openDatabase()
\row \li Driver \li Currently "QSQLITE"
\endtable

This data can be used by application tools.

\section3 db.changeVersion(from, to, callback(tx))

This method allows you to perform a \e{Scheme Upgrade}.

If the current version of \e db is not \e from, then an exception is thrown.

Otherwise, a database transaction is created and passed to \e callback. In this function,
you can call \e executeSql on \e tx to upgrade the database.

May throw exception with code property SQLException.DATABASE_ERR or SQLException.UNKNOWN_ERR.

\section3 db.transaction(callback(tx))

This method creates a read/write transaction and passed to \e callback. In this function,
you can call \e executeSql on \e tx to read and modify the database.

If the callback throws exceptions, the transaction is rolled back.

\section3 db.readTransaction(callback(tx))

This method creates a read-only transaction and passed to \e callback. In this function,
you can call \e executeSql on \e tx to read the database (with SELECT statements).

\section3 results = tx.executeSql(statement, values)

This method executes a SQL \e statement, binding the list of \e values to SQL positional parameters ("?").

It returns a results object, with the following properties:

\table
\header \li \b {Type} \li \b {Property} \li \b {Value} \li \b {Applicability}
\row \li int \li rows.length \li The number of rows in the result \li SELECT
\row \li var \li rows.item(i) \li Function that returns row \e i of the result \li SELECT
\row \li int \li rowsAffected \li The number of rows affected by a modification \li UPDATE, DELETE
\row \li string \li insertId \li The id of the row inserted \li INSERT
\endtable

May throw exception with code property SQLException.DATABASE_ERR, SQLException.SYNTAX_ERR, or SQLException.UNKNOWN_ERR.


\section1 Method Documentation

\target openDatabaseSync
\code
object openDatabaseSync(string name, string version, string description, int estimated_size, jsobject callback(db))
\endcode

Opens or creates a local storage sql database by the given parameters.

\list
\li \c name is the database name
\li \c version is the database version
\li \c description is the database display name
\li \c estimated_size is the database's estimated size, in bytes
\li \c callback is an optional parameter, which is invoked if the database has not yet been created.
\endlist

Returns the created database object.

*/
class QQuickLocalStorage : public QObject
{
    Q_OBJECT
public:
    QQuickLocalStorage(QObject *parent=0) : QObject(parent)
    {
    }
    ~QQuickLocalStorage() {
    }

   Q_INVOKABLE void openDatabaseSync(QQmlV4Function* args);
};

void QQuickLocalStorage::openDatabaseSync(QQmlV4Function *args)
{
#ifndef QT_NO_SETTINGS
    QV8Engine *engine = args->engine();
    QV4::ExecutionContext *ctx = args->v4engine()->currentContext();
    QV4::Scope scope(ctx);
    if (engine->engine()->offlineStoragePath().isEmpty())
        V4THROW_SQL2(SQLEXCEPTION_DATABASE_ERR, QQmlEngine::tr("SQL: can't create database, offline storage is disabled."));

    qmlsqldatabase_initDatabasesPath(engine);

    QSqlDatabase database;

    QV4::ScopedValue v(scope);
    QString dbname = (v = (*args)[0])->toQStringNoThrow();
    QString dbversion = (v = (*args)[1])->toQStringNoThrow();
    QString dbdescription = (v = (*args)[2])->toQStringNoThrow();
    int dbestimatedsize = (v = (*args)[3])->toInt32();
    FunctionObject *dbcreationCallback = (v = (*args)[4])->asFunctionObject();

    QCryptographicHash md5(QCryptographicHash::Md5);
    md5.addData(dbname.toUtf8());
    QString dbid(QLatin1String(md5.result().toHex()));

    QString basename = qmlsqldatabase_databaseFile(dbid, engine);
    bool created = false;
    QString version = dbversion;

    {
        QSettings ini(basename+QLatin1String(".ini"),QSettings::IniFormat);

        if (QSqlDatabase::connectionNames().contains(dbid)) {
            database = QSqlDatabase::database(dbid);
            version = ini.value(QLatin1String("Version")).toString();
            if (version != dbversion && !dbversion.isEmpty() && !version.isEmpty())
                V4THROW_SQL2(SQLEXCEPTION_VERSION_ERR, QQmlEngine::tr("SQL: database version mismatch"));
        } else {
            created = !QFile::exists(basename+QLatin1String(".sqlite"));
            if (created) {
                ini.setValue(QLatin1String("Name"), dbname);
                if (dbcreationCallback)
                    version = QString();
                ini.setValue(QLatin1String("Version"), version);
                ini.setValue(QLatin1String("Description"), dbdescription);
                ini.setValue(QLatin1String("EstimatedSize"), dbestimatedsize);
                ini.setValue(QLatin1String("Driver"), QLatin1String("QSQLITE"));
            } else {
                if (!dbversion.isEmpty() && ini.value(QLatin1String("Version")) != dbversion) {
                    // Incompatible
                    V4THROW_SQL2(SQLEXCEPTION_VERSION_ERR,QQmlEngine::tr("SQL: database version mismatch"));
                }
                version = ini.value(QLatin1String("Version")).toString();
            }
            database = QSqlDatabase::addDatabase(QLatin1String("QSQLITE"), dbid);
            database.setDatabaseName(basename+QLatin1String(".sqlite"));
        }
        if (!database.isOpen())
            database.open();
    }

    QV4::Scoped<QQmlSqlDatabaseWrapper> db(scope, QQmlSqlDatabaseWrapper::create(engine));
    QV4::ScopedObject p(scope, databaseData(engine)->databaseProto.value());
    db->setPrototype(p.getPointer());
    db->d()->database = database;
    db->d()->version = version;

    if (created && dbcreationCallback) {
        Scope scope(ctx);
        ScopedCallData callData(scope, 1);
        callData->thisObject = engine->global();
        callData->args[0] = db;
        dbcreationCallback->call(callData);
    }

    args->setReturnValue(db.asReturnedValue());
#endif // QT_NO_SETTINGS
}

static QObject *module_api_factory(QQmlEngine *engine, QJSEngine *scriptEngine)
{
   Q_UNUSED(engine)
   Q_UNUSED(scriptEngine)
   QQuickLocalStorage *api = new QQuickLocalStorage();

   return api;
}

class QQmlLocalStoragePlugin : public QQmlExtensionPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QQmlExtensionInterface")

public:
    QQmlLocalStoragePlugin()
    {
    }

    void registerTypes(const char *uri)
    {
        Q_ASSERT(QLatin1String(uri) == "QtQuick.LocalStorage");
        qmlRegisterSingletonType<QQuickLocalStorage>(uri, 2, 0, "LocalStorage", module_api_factory);
    }
};

#include "plugin.moc"
