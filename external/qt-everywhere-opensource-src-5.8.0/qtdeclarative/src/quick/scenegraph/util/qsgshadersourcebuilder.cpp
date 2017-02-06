/****************************************************************************
**
** Copyright (C) 2016 Klaralvdalens Datakonsult AB (KDAB).
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQuick module of the Qt Toolkit.
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

#include "qsgshadersourcebuilder_p.h"

#include <QtGui/qopenglcontext.h>
#include <QtGui/qopenglshaderprogram.h>

#include <QtCore/qdebug.h>
#include <QtCore/qfile.h>

QT_BEGIN_NAMESPACE

namespace QSGShaderParser {

struct Tokenizer {

    enum Token {
        Token_Invalid,
        Token_Void,
        Token_OpenBrace,
        Token_CloseBrace,
        Token_SemiColon,
        Token_Identifier,
        Token_Macro,
        Token_Version,
        Token_Extension,
        Token_SingleLineComment,
        Token_MultiLineCommentStart,
        Token_MultiLineCommentEnd,
        Token_NewLine,
        Token_Unspecified,
        Token_EOF
    };

    static const char *NAMES[];

    void initialize(const char *input);
    Token next();

    const char *stream;
    const char *pos;
    const char *identifier;
};

const char *Tokenizer::NAMES[] = {
    "Invalid",
    "Void",
    "OpenBrace",
    "CloseBrace",
    "SemiColon",
    "Identifier",
    "Macro",
    "Version",
    "Extension",
    "SingleLineComment",
    "MultiLineCommentStart",
    "MultiLineCommentEnd",
    "NewLine",
    "Unspecified",
    "EOF"
};

void Tokenizer::initialize(const char *input)
{
    stream = input;
    pos = input;
    identifier = input;
}

Tokenizer::Token Tokenizer::next()
{
    while (*pos != 0) {
        char c = *pos++;
        switch (c) {
        case '/':
            if (*pos == '/') {
                // '//' comment
                return Token_SingleLineComment;
            } else if (*pos == '*') {
                // /* */ comment
                return Token_MultiLineCommentStart;
            }
            break;

        case '*':
            if (*pos == '/')
                return Token_MultiLineCommentEnd;

        case '\n':
            return Token_NewLine;

        case '\r':
            if (*pos == '\n')
                return Token_NewLine;

        case '#': {
            if (*pos == 'v' && pos[1] == 'e' && pos[2] == 'r' && pos[3] == 's'
                && pos[4] == 'i' && pos[5] == 'o' && pos[6] == 'n') {
                return Token_Version;
            } else if (*pos == 'e' && pos[1] == 'x' && pos[2] == 't' && pos[3] == 'e'
                       && pos[4] == 'n' && pos[5] == 's' && pos[6] == 'i'&& pos[7] == 'o'
                       && pos[8] == 'n') {
                return Token_Extension;
            } else {
                while (*pos != 0) {
                    if (*pos == '\n') {
                        ++pos;
                        break;
                    } else if (*pos == '\\') {
                        ++pos;
                        while (*pos != 0 && (*pos == ' ' || *pos == '\t'))
                            ++pos;
                        if (*pos != 0 && (*pos == '\n' || (*pos == '\r' && pos[1] == '\n')))
                            pos+=2;
                    } else {
                        ++pos;
                    }
                }
            }
            break;
        }

        case ';':
            return Token_SemiColon;

        case 0:
            return Token_EOF;

        case '{':
            return Token_OpenBrace;

        case '}':
            return Token_CloseBrace;

        case ' ':
            break;

        case 'v': {
            if (*pos == 'o' && pos[1] == 'i' && pos[2] == 'd') {
                pos += 3;
                return Token_Void;
            }
            // Fall-thru
        }
        default:
            // Identifier...
            if ((c >= 'a' && c <= 'z' ) || (c >= 'A' && c <= 'Z' ) || c == '_') {
                identifier = pos - 1;
                while (*pos != 0 && ((*pos >= 'a' && *pos <= 'z')
                                     || (*pos >= 'A' && *pos <= 'Z')
                                     || *pos == '_'
                                     || (*pos >= '0' && *pos <= '9'))) {
                    ++pos;
                }
                return Token_Identifier;
            } else {
                return Token_Unspecified;
            }
        }
    }

    return Token_Invalid;
}

} // namespace QSGShaderParser

using namespace QSGShaderParser;

QSGShaderSourceBuilder::QSGShaderSourceBuilder()
{
}

void QSGShaderSourceBuilder::initializeProgramFromFiles(QOpenGLShaderProgram *program,
                                                        const QString &vertexShader,
                                                        const QString &fragmentShader)
{
    Q_ASSERT(program);
    program->removeAllShaders();

    QSGShaderSourceBuilder builder;

    builder.appendSourceFile(vertexShader);
    program->addShaderFromSourceCode(QOpenGLShader::Vertex, builder.source());
    builder.clear();

    builder.appendSourceFile(fragmentShader);
    program->addShaderFromSourceCode(QOpenGLShader::Fragment, builder.source());
}

QByteArray QSGShaderSourceBuilder::source() const
{
    return m_source;
}

void QSGShaderSourceBuilder::clear()
{
    m_source.clear();
}

void QSGShaderSourceBuilder::appendSource(const QByteArray &source)
{
    m_source += source;
}

void QSGShaderSourceBuilder::appendSourceFile(const QString &fileName)
{
    const QString resolvedFileName = resolveShaderPath(fileName);
    QFile f(resolvedFileName);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Failed to find shader" << resolvedFileName;
        return;
    }
    m_source += f.readAll();
}

void QSGShaderSourceBuilder::addDefinition(const QByteArray &definition)
{
    if (definition.isEmpty())
        return;

    Tokenizer tok;
    const char *input = m_source.constData();
    tok.initialize(input);

    // First find #version, #extension's and "void main() { ... "
    const char *versionPos = 0;
    const char *extensionPos = 0;
    bool inSingleLineComment = false;
    bool inMultiLineComment = false;
    bool foundVersionStart = false;
    bool foundExtensionStart = false;

    Tokenizer::Token lt = Tokenizer::Token_Unspecified;
    Tokenizer::Token t = tok.next();
    while (t != Tokenizer::Token_EOF) {
        // Handle comment blocks
        if (t == Tokenizer::Token_MultiLineCommentStart )
            inMultiLineComment = true;
        if (t == Tokenizer::Token_MultiLineCommentEnd)
            inMultiLineComment = false;
        if (t == Tokenizer::Token_SingleLineComment)
            inSingleLineComment = true;
        if (t == Tokenizer::Token_NewLine && inSingleLineComment && !inMultiLineComment)
            inSingleLineComment = false;

        // Have we found #version, #extension or void main()?
        if (t == Tokenizer::Token_Version && !inSingleLineComment && !inMultiLineComment)
            foundVersionStart = true;

        if (t == Tokenizer::Token_Extension && !inSingleLineComment && !inMultiLineComment)
            foundExtensionStart = true;

        if (foundVersionStart && t == Tokenizer::Token_NewLine) {
            versionPos = tok.pos;
            foundVersionStart = false;
        } else if (foundExtensionStart && t == Tokenizer::Token_NewLine) {
            extensionPos = tok.pos;
            foundExtensionStart = false;
        } else if (lt == Tokenizer::Token_Void && t == Tokenizer::Token_Identifier) {
            if (qstrncmp("main", tok.identifier, 4) == 0)
                break;
        }

        // Scan to next token
        lt = t;
        t = tok.next();
    }

    // Determine where to insert the definition.
    // If we found #extension directives, insert after last one,
    // else, if we found #version insert after #version
    // otherwise, insert at beginning.
    const char *insertionPos = extensionPos ? extensionPos : (versionPos ? versionPos : input);

    // Construct a new shader string, inserting the definition
    QByteArray newSource;
    newSource.reserve(m_source.size() + definition.size() + 9);
    newSource += QByteArray::fromRawData(input, insertionPos - input);
    newSource += QByteArrayLiteral("#define ") + definition + QByteArrayLiteral("\n");
    newSource += QByteArray::fromRawData(insertionPos, m_source.size() - (insertionPos - input));

    m_source = newSource;
}

void QSGShaderSourceBuilder::removeVersion()
{
    Tokenizer tok;
    const char *input = m_source.constData();
    tok.initialize(input);

    // First find #version beginning and end (if present)
    const char *versionStartPos = 0;
    const char *versionEndPos = 0;
    bool inSingleLineComment = false;
    bool inMultiLineComment = false;
    bool foundVersionStart = false;

    Tokenizer::Token lt = Tokenizer::Token_Unspecified;
    Tokenizer::Token t = tok.next();
    while (t != Tokenizer::Token_EOF) {
        // Handle comment blocks
        if (t == Tokenizer::Token_MultiLineCommentStart )
            inMultiLineComment = true;
        if (t == Tokenizer::Token_MultiLineCommentEnd)
            inMultiLineComment = false;
        if (t == Tokenizer::Token_SingleLineComment)
            inSingleLineComment = true;
        if (t == Tokenizer::Token_NewLine && inSingleLineComment && !inMultiLineComment)
            inSingleLineComment = false;

        // Have we found #version, #extension or void main()?
        if (t == Tokenizer::Token_Version && !inSingleLineComment && !inMultiLineComment) {
            versionStartPos = tok.pos - 1;
            foundVersionStart = true;
        } else if (foundVersionStart && t == Tokenizer::Token_NewLine) {
            versionEndPos = tok.pos;
            break;
        } else if (lt == Tokenizer::Token_Void && t == Tokenizer::Token_Identifier) {
            if (qstrncmp("main", tok.identifier, 4) == 0)
                break;
        }

        // Scan to next token
        lt = t;
        t = tok.next();
    }

    if (versionStartPos == 0)
        return;

    // Construct a new shader string, inserting the definition
    QByteArray newSource;
    newSource.reserve(m_source.size() - (versionEndPos - versionStartPos));
    newSource += QByteArray::fromRawData(input, versionStartPos - input);
    newSource += QByteArray::fromRawData(versionEndPos, m_source.size() - (versionEndPos - versionStartPos));

    m_source = newSource;
}

QString QSGShaderSourceBuilder::resolveShaderPath(const QString &path) const
{
    if (contextProfile() != QSurfaceFormat::CoreProfile) {
        return path;
    } else {
        int idx = path.lastIndexOf(QLatin1Char('.'));
        QString resolvedPath;
        if (idx != -1)
            resolvedPath = path.leftRef(idx)
                         + QLatin1String("_core")
                         + path.rightRef(path.length() - idx);
        return resolvedPath;
    }
}

QSurfaceFormat::OpenGLContextProfile QSGShaderSourceBuilder::contextProfile() const
{
    QOpenGLContext *context = QOpenGLContext::currentContext();
    QSurfaceFormat::OpenGLContextProfile profile = QSurfaceFormat::NoProfile;
    if (context)
        profile = context->format().profile();
    return profile;
}

QT_END_NAMESPACE
