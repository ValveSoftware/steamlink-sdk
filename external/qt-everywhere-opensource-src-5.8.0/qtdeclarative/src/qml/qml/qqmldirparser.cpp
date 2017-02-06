/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQml module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qqmldirparser_p.h"
#include "qqmlerror.h"

#include <QtCore/QtDebug>

QT_BEGIN_NAMESPACE

static int parseInt(const QStringRef &str, bool *ok)
{
    int pos = 0;
    int number = 0;
    while (pos < str.length() && str.at(pos).isDigit()) {
        if (pos != 0)
            number *= 10;
        number += str.at(pos).unicode() - '0';
        ++pos;
    }
    if (pos != str.length())
        *ok = false;
    else
        *ok = true;
    return number;
}

static bool parseVersion(const QString &str, int *major, int *minor)
{
    const int dotIndex = str.indexOf(QLatin1Char('.'));
    if (dotIndex != -1 && str.indexOf(QLatin1Char('.'), dotIndex + 1) == -1) {
        bool ok = false;
        *major = parseInt(QStringRef(&str, 0, dotIndex), &ok);
        if (ok)
            *minor = parseInt(QStringRef(&str, dotIndex + 1, str.length() - dotIndex - 1), &ok);
        return ok;
    }
    return false;
}

QQmlDirParser::QQmlDirParser() : _designerSupported(false)
{
}

QQmlDirParser::~QQmlDirParser()
{
}

inline static void scanSpace(const QChar *&ch) {
    while (ch->isSpace() && !ch->isNull() && *ch != QLatin1Char('\n'))
        ++ch;
}

inline static void scanToEnd(const QChar *&ch) {
    while (*ch != QLatin1Char('\n') && !ch->isNull())
        ++ch;
}

inline static void scanWord(const QChar *&ch) {
    while (!ch->isSpace() && !ch->isNull())
        ++ch;
}

/*!
\a url is used for generating errors.
*/
bool QQmlDirParser::parse(const QString &source)
{
    _errors.clear();
    _plugins.clear();
    _components.clear();
    _scripts.clear();
    _designerSupported = false;

    quint16 lineNumber = 0;
    bool firstLine = true;

    const QChar *ch = source.constData();
    while (!ch->isNull()) {
        ++lineNumber;

        bool invalidLine = false;
        const QChar *lineStart = ch;

        scanSpace(ch);
        if (*ch == QLatin1Char('\n')) {
            ++ch;
            continue;
        }
        if (ch->isNull())
            break;

        QString sections[4];
        int sectionCount = 0;

        do {
            if (*ch == QLatin1Char('#')) {
                scanToEnd(ch);
                break;
            }
            const QChar *start = ch;
            scanWord(ch);
            if (sectionCount < 4) {
                sections[sectionCount++] = source.mid(start-source.constData(), ch-start);
            } else {
                reportError(lineNumber, start-lineStart, QLatin1String("unexpected token"));
                scanToEnd(ch);
                invalidLine = true;
                break;
            }
            scanSpace(ch);
        } while (*ch != QLatin1Char('\n') && !ch->isNull());

        if (!ch->isNull())
            ++ch;

        if (invalidLine) {
            reportError(lineNumber, 0,
                        QStringLiteral("invalid qmldir directive contains too many tokens"));
            continue;
        } else if (sectionCount == 0) {
            continue; // no sections, no party.

        } else if (sections[0] == QLatin1String("module")) {
            if (sectionCount != 2) {
                reportError(lineNumber, 0,
                            QStringLiteral("module identifier directive requires one argument, but %1 were provided").arg(sectionCount - 1));
                continue;
            }
            if (!_typeNamespace.isEmpty()) {
                reportError(lineNumber, 0,
                            QStringLiteral("only one module identifier directive may be defined in a qmldir file"));
                continue;
            }
            if (!firstLine) {
                reportError(lineNumber, 0,
                            QStringLiteral("module identifier directive must be the first directive in a qmldir file"));
                continue;
            }

            _typeNamespace = sections[1];

        } else if (sections[0] == QLatin1String("plugin")) {
            if (sectionCount < 2 || sectionCount > 3) {
                reportError(lineNumber, 0,
                            QStringLiteral("plugin directive requires one or two arguments, but %1 were provided").arg(sectionCount - 1));

                continue;
            }

            const Plugin entry(sections[1], sections[2]);

            _plugins.append(entry);

        } else if (sections[0] == QLatin1String("internal")) {
            if (sectionCount != 3) {
                reportError(lineNumber, 0,
                            QStringLiteral("internal types require 2 arguments, but %1 were provided").arg(sectionCount - 1));
                continue;
            }
            Component entry(sections[1], sections[2], -1, -1);
            entry.internal = true;
            _components.insertMulti(entry.typeName, entry);
        } else if (sections[0] == QLatin1String("singleton")) {
            if (sectionCount < 3 || sectionCount > 4) {
                reportError(lineNumber, 0,
                            QStringLiteral("singleton types require 2 or 3 arguments, but %1 were provided").arg(sectionCount - 1));
                continue;
            } else if (sectionCount == 3) {
                // handle qmldir directory listing case where singleton is defined in the following pattern:
                // singleton TestSingletonType TestSingletonType.qml
                Component entry(sections[1], sections[2], -1, -1);
                entry.singleton = true;
                _components.insertMulti(entry.typeName, entry);
            } else {
                // handle qmldir module listing case where singleton is defined in the following pattern:
                // singleton TestSingletonType 2.0 TestSingletonType20.qml
                int major, minor;
                if (parseVersion(sections[2], &major, &minor)) {
                    const QString &fileName = sections[3];
                    Component entry(sections[1], fileName, major, minor);
                    entry.singleton = true;
                    _components.insertMulti(entry.typeName, entry);
                } else {
                    reportError(lineNumber, 0, QStringLiteral("invalid version %1, expected <major>.<minor>").arg(sections[2]));
                }
            }
        } else if (sections[0] == QLatin1String("typeinfo")) {
            if (sectionCount != 2) {
                reportError(lineNumber, 0,
                            QStringLiteral("typeinfo requires 1 argument, but %1 were provided").arg(sectionCount - 1));
                continue;
            }
#ifdef QT_CREATOR
            TypeInfo typeInfo(sections[1]);
            _typeInfos.append(typeInfo);
#endif

        } else if (sections[0] == QLatin1String("designersupported")) {
            if (sectionCount != 1)
                reportError(lineNumber, 0, QStringLiteral("designersupported does not expect any argument"));
            else
                _designerSupported = true;
        } else if (sections[0] == QLatin1String("depends")) {
            if (sectionCount != 3) {
                reportError(lineNumber, 0,
                            QStringLiteral("depends requires 2 arguments, but %1 were provided").arg(sectionCount - 1));
                continue;
            }

            int major, minor;
            if (parseVersion(sections[2], &major, &minor)) {
                Component entry(sections[1], QString(), major, minor);
                entry.internal = true;
                _dependencies.insert(entry.typeName, entry);
            } else {
                reportError(lineNumber, 0, QStringLiteral("invalid version %1, expected <major>.<minor>").arg(sections[2]));
            }
        } else if (sectionCount == 2) {
            // No version specified (should only be used for relative qmldir files)
            const Component entry(sections[0], sections[1], -1, -1);
            _components.insertMulti(entry.typeName, entry);
        } else if (sectionCount == 3) {
            int major, minor;
            if (parseVersion(sections[1], &major, &minor)) {
                const QString &fileName = sections[2];

                if (fileName.endsWith(QLatin1String(".js"))) {
                    // A 'js' extension indicates a namespaced script import
                    const Script entry(sections[0], fileName, major, minor);
                    _scripts.append(entry);
                } else {
                    const Component entry(sections[0], fileName, major, minor);
                    _components.insertMulti(entry.typeName, entry);
                }
            } else {
                reportError(lineNumber, 0, QStringLiteral("invalid version %1, expected <major>.<minor>").arg(sections[1]));
            }
        } else {
            reportError(lineNumber, 0,
                        QStringLiteral("a component declaration requires two or three arguments, but %1 were provided").arg(sectionCount));
        }

        firstLine = false;
    }

    return hasError();
}

void QQmlDirParser::reportError(quint16 line, quint16 column, const QString &description)
{
    QQmlJS::DiagnosticMessage error;
    error.loc.startLine = line;
    error.loc.startColumn = column;
    error.message = description;
    _errors.append(error);
}

bool QQmlDirParser::hasError() const
{
    if (! _errors.isEmpty())
        return true;

    return false;
}

void QQmlDirParser::setError(const QQmlError &e)
{
    _errors.clear();
    reportError(e.line(), e.column(), e.description());
}

QList<QQmlError> QQmlDirParser::errors(const QString &uri) const
{
    QUrl url(uri);
    QList<QQmlError> errors;
    const int numErrors = _errors.size();
    errors.reserve(numErrors);
    for (int i = 0; i < numErrors; ++i) {
        const QQmlJS::DiagnosticMessage &msg = _errors.at(i);
        QQmlError e;
        QString description = msg.message;
        description.replace(QLatin1String("$$URI$$"), uri);
        e.setDescription(description);
        e.setUrl(url);
        e.setLine(msg.loc.startLine);
        e.setColumn(msg.loc.startColumn);
        errors << e;
    }
    return errors;
}

QString QQmlDirParser::typeNamespace() const
{
    return _typeNamespace;
}

void QQmlDirParser::setTypeNamespace(const QString &s)
{
    _typeNamespace = s;
}

QList<QQmlDirParser::Plugin> QQmlDirParser::plugins() const
{
    return _plugins;
}

QHash<QString, QQmlDirParser::Component> QQmlDirParser::components() const
{
    return _components;
}

QHash<QString, QQmlDirParser::Component> QQmlDirParser::dependencies() const
{
    return _dependencies;
}

QList<QQmlDirParser::Script> QQmlDirParser::scripts() const
{
    return _scripts;
}

#ifdef QT_CREATOR
QList<QQmlDirParser::TypeInfo> QQmlDirParser::typeInfos() const
{
    return _typeInfos;
}
#endif

bool QQmlDirParser::designerSupported() const
{
    return _designerSupported;
}

QDebug &operator<< (QDebug &debug, const QQmlDirParser::Component &component)
{
    const QString output = QStringLiteral("{%1 %2.%3}").
        arg(component.typeName).arg(component.majorVersion).arg(component.minorVersion);
    return debug << qPrintable(output);
}

QDebug &operator<< (QDebug &debug, const QQmlDirParser::Script &script)
{
    const QString output = QStringLiteral("{%1 %2.%3}").
        arg(script.nameSpace).arg(script.majorVersion).arg(script.minorVersion);
    return debug << qPrintable(output);
}

QT_END_NAMESPACE
