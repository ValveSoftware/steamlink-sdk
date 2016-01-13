/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the V4VM module of the Qt Toolkit.
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

#include "private/qv4object_p.h"
#include "private/qv4runtime_p.h"
#include "private/qv4functionobject_p.h"
#include "private/qv4errorobject_p.h"
#include "private/qv4globalobject_p.h"
#include "private/qv4codegen_p.h"
#include "private/qv4isel_moth_p.h"
#include "private/qv4vme_moth_p.h"
#include "private/qv4objectproto_p.h"
#include "private/qv4isel_p.h"
#include "private/qv4mm_p.h"
#include "private/qv4context_p.h"
#include "private/qv4script_p.h"

#ifdef V4_ENABLE_JIT
#  include "private/qv4isel_masm_p.h"
#endif // V4_ENABLE_JIT

#include <QtCore/QCoreApplication>
#include <QtCore/QFile>
#include <private/qqmljsengine_p.h>
#include <private/qqmljslexer_p.h>
#include <private/qqmljsparser_p.h>
#include <private/qqmljsast_p.h>

#include <iostream>

namespace builtins {

using namespace QV4;

struct Print: FunctionObject
{
    struct Data : FunctionObject::Data {
        Data(ExecutionContext *scope)
            : FunctionObject::Data(scope, QStringLiteral("print")) {
            setVTable(staticVTable());
        }
    };
    V4_OBJECT(FunctionObject)

    static ReturnedValue call(Managed *, CallData *callData)
    {
        for (int i = 0; i < callData->argc; ++i) {
            QString s = callData->args[i].toQStringNoThrow();
            if (i)
                std::cout << ' ';
            std::cout << qPrintable(s);
        }
        std::cout << std::endl;
        return Encode::undefined();
    }
};

DEFINE_OBJECT_VTABLE(Print);

struct GC: public FunctionObject
{
    struct Data : FunctionObject::Data {
        Data(ExecutionContext *scope)
            : FunctionObject::Data(scope, QStringLiteral("gc"))
        {
            setVTable(staticVTable());
        }

    };
    V4_OBJECT(FunctionObject)

    static ReturnedValue call(Managed *m, CallData *)
    {
        m->engine()->memoryManager->runGC();
        return Encode::undefined();
    }
};

DEFINE_OBJECT_VTABLE(GC);

} // builtins

static void showException(QV4::ExecutionContext *ctx, const QV4::ValueRef exception, const QV4::StackTrace &trace)
{
    QV4::Scope scope(ctx);
    QV4::ScopedValue ex(scope, *exception);
    QV4::ErrorObject *e = ex->asErrorObject();
    if (!e) {
        std::cerr << "Uncaught exception: " << qPrintable(ex->toString(ctx)->toQString()) << std::endl;
    } else {
        QV4::ScopedString m(scope, scope.engine->newString(QStringLiteral("message")));
        QV4::ScopedValue message(scope, e->get(m.getPointer()));
        std::cerr << "Uncaught exception: " << qPrintable(message->toQStringNoThrow()) << std::endl;
    }

    foreach (const QV4::StackFrame &frame, trace) {
        std::cerr << "    at " << qPrintable(frame.function) << " (" << qPrintable(frame.source);
        if (frame.line >= 0)
            std::cerr << ":" << frame.line;
        std::cerr << ")" << std::endl;
    }
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QStringList args = app.arguments();
    args.removeFirst();

    enum {
        use_masm,
        use_moth
    } mode;
#ifdef V4_ENABLE_JIT
    mode = use_masm;
#else
    mode = use_moth;
#endif

    bool runAsQml = false;

    if (!args.isEmpty()) {
        if (args.first() == QLatin1String("--jit")) {
            mode = use_masm;
            args.removeFirst();
        }

        if (args.first() == QLatin1String("--interpret")) {
            mode = use_moth;
            args.removeFirst();
        }

        if (args.first() == QLatin1String("--qml")) {
            runAsQml = true;
            args.removeFirst();
        }

        if (args.first() == QLatin1String("--help")) {
            std::cerr << "Usage: qmljs [|--jit|--interpret|--qml] file..." << std::endl;
            return EXIT_SUCCESS;
        }
    }

    switch (mode) {
    case use_masm:
    case use_moth: {
        QV4::EvalISelFactory* iSelFactory = 0;
        if (mode == use_moth) {
            iSelFactory = new QV4::Moth::ISelFactory;
#ifdef V4_ENABLE_JIT
        } else {
            iSelFactory = new QV4::JIT::ISelFactory;
#endif // V4_ENABLE_JIT
        }

        QV4::ExecutionEngine vm(iSelFactory);

        QV4::ExecutionContext *ctx = vm.rootContext;
        QV4::Scope scope(ctx);

        QV4::ScopedObject globalObject(scope, vm.globalObject);
        QV4::ScopedObject print(scope, vm.memoryManager->alloc<builtins::Print>(ctx));
        globalObject->put(QV4::ScopedString(scope, vm.newIdentifier(QStringLiteral("print"))).getPointer(), print);
        QV4::ScopedObject gc(scope, vm.memoryManager->alloc<builtins::GC>(ctx));
        globalObject->put(QV4::ScopedString(scope, vm.newIdentifier(QStringLiteral("gc"))).getPointer(), gc);

        foreach (const QString &fn, args) {
            QFile file(fn);
            if (file.open(QFile::ReadOnly)) {
                const QString code = QString::fromUtf8(file.readAll());
                file.close();

                QV4::ScopedValue result(scope);
                QV4::Script script(ctx, code, fn);
                script.parseAsBinding = runAsQml;
                script.parse();
                if (!scope.engine->hasException)
                    result = script.run();
                if (scope.engine->hasException) {
                    QV4::StackTrace trace;
                    QV4::ScopedValue ex(scope, ctx->catchException(&trace));
                    showException(ctx, ex, trace);
                    return EXIT_FAILURE;
                }
                if (!result->isUndefined()) {
                    if (! qgetenv("SHOW_EXIT_VALUE").isEmpty())
                        std::cout << "exit value: " << qPrintable(result->toString(ctx)->toQString()) << std::endl;
                }
            } else {
                std::cerr << "Error: cannot open file " << fn.toUtf8().constData() << std::endl;
                return EXIT_FAILURE;
            }
        }

        vm.memoryManager->dumpStats();
    } return EXIT_SUCCESS;
    } // switch (mode)
}
