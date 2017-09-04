/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the tools applications of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

/*
  jscodemarker.cpp
*/

#include "jscodemarker.h"

#include "atom.h"
#include "node.h"
#include "qmlmarkupvisitor.h"
#include "text.h"
#include "tree.h"
#include "generator.h"

#ifndef QT_NO_DECLARATIVE
#include <private/qqmljsast_p.h>
#include <private/qqmljsengine_p.h>
#include <private/qqmljslexer_p.h>
#include <private/qqmljsparser_p.h>
#endif

QT_BEGIN_NAMESPACE

JsCodeMarker::JsCodeMarker()
{
}

JsCodeMarker::~JsCodeMarker()
{
}

/*!
  Returns \c true if the \a code is recognized by the parser.
 */
bool JsCodeMarker::recognizeCode(const QString &code)
{
#ifndef QT_NO_DECLARATIVE
    QQmlJS::Engine engine;
    QQmlJS::Lexer lexer(&engine);
    QQmlJS::Parser parser(&engine);

    QString newCode = code;
    QList<QQmlJS::AST::SourceLocation> pragmas = extractPragmas(newCode);
    lexer.setCode(newCode, 1);

    return parser.parseProgram();
#else
    return false;
#endif
}

/*!
  Returns \c true if \a ext is any of a list of file extensions
  for the QML language.
 */
bool JsCodeMarker::recognizeExtension(const QString &ext)
{
    return ext == "js" || ext == "json";
}

/*!
  Returns \c true if the \a language is recognized. We recognize JavaScript,
  ECMAScript and JSON.
 */
bool JsCodeMarker::recognizeLanguage(const QString &language)
{
    return language == "JavaScript" || language == "ECMAScript" || language == "JSON";
}

/*!
  Returns the type of atom used to represent JavaScript code in the documentation.
*/
Atom::AtomType JsCodeMarker::atomType() const
{
    return Atom::JavaScript;
}

QString JsCodeMarker::markedUpCode(const QString &code,
                                   const Node *relative,
                                   const Location &location)
{
    return addMarkUp(code, relative, location);
}

QString JsCodeMarker::addMarkUp(const QString &code,
                                const Node * /* relative */,
                                const Location &location)
{
#ifndef QT_NO_DECLARATIVE
    QQmlJS::Engine engine;
    QQmlJS::Lexer lexer(&engine);

    QString newCode = code;
    QList<QQmlJS::AST::SourceLocation> pragmas = extractPragmas(newCode);
    lexer.setCode(newCode, 1);

    QQmlJS::Parser parser(&engine);
    QString output;

    if (parser.parseProgram()) {
        QQmlJS::AST::Node *ast = parser.rootNode();
        // Pass the unmodified code to the visitor so that pragmas and other
        // unhandled source text can be output.
        QmlMarkupVisitor visitor(code, pragmas, &engine);
        QQmlJS::AST::Node::accept(ast, &visitor);
        output = visitor.markedUpCode();
    } else {
        location.warning(location.fileName() +
                         tr("Unable to parse JavaScript: \"%1\" at line %2, column %3").arg(
                             parser.errorMessage()).arg(parser.errorLineNumber()).arg(
                             parser.errorColumnNumber()));
        output = protect(code);
    }
    return output;
#else
    location.warning("QtDeclarative not installed; cannot parse QML or JS.");
    return QString();
#endif
}

QT_END_NAMESPACE
