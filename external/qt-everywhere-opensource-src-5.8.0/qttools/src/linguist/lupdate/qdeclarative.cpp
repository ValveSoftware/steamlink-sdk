/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Linguist of the Qt Toolkit.
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

#include "lupdate.h"

#include <translator.h>

#include <QtCore/QDebug>
#include <QtCore/QFile>
#include <QtCore/QString>

#include <private/qqmljsengine_p.h>
#include <private/qqmljsparser_p.h>
#include <private/qqmljslexer_p.h>
#include <private/qqmljsastvisitor_p.h>
#include <private/qqmljsast_p.h>

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QtDebug>
#include <QStringList>

#include <iostream>
#include <cstdlib>
#include <cctype>

QT_BEGIN_NAMESPACE

class LU {
    Q_DECLARE_TR_FUNCTIONS(LUpdate)
};

using namespace QQmlJS;

static QString MagicComment(QLatin1String("TRANSLATOR"));

class FindTrCalls: protected AST::Visitor
{
public:
    FindTrCalls(Engine *engine, ConversionData &cd)
        : engine(engine)
        , m_cd(cd)
    {
    }

    void operator()(Translator *translator, const QString &fileName, AST::Node *node)
    {
        m_todo = engine->comments();
        m_translator = translator;
        m_fileName = fileName;
        m_component = QFileInfo(fileName).completeBaseName();
        accept(node);

        // process the trailing comments
        processComments(0, /*flush*/ true);
    }

protected:
    using AST::Visitor::visit;
    using AST::Visitor::endVisit;

    void accept(AST::Node *node)
    { AST::Node::acceptChild(node, this); }

    void endVisit(AST::CallExpression *node)
    {
        if (AST::IdentifierExpression *idExpr = AST::cast<AST::IdentifierExpression *>(node->base)) {
            processComments(idExpr->identifierToken.begin());

            const QString name = idExpr->name.toString();
            const int identLineNo = idExpr->identifierToken.startLine;
            switch (trFunctionAliasManager.trFunctionByName(name)) {
            case TrFunctionAliasManager::Function_qsTr:
            case TrFunctionAliasManager::Function_QT_TR_NOOP: {
                if (!node->arguments) {
                    yyMsg(identLineNo) << qPrintable(LU::tr("%1() requires at least one argument.\n").arg(name));
                    return;
                }

                QString source;
                if (!createString(node->arguments->expression, &source))
                    return;

                QString comment;
                bool plural = false;
                if (AST::ArgumentList *commentNode = node->arguments->next) {
                    if (!createString(commentNode->expression, &comment)) {
                        comment.clear(); // clear possible invalid comments
                    }
                    if (commentNode->next)
                        plural = true;
                }

                if (!sourcetext.isEmpty())
                    yyMsg(identLineNo) << qPrintable(LU::tr("//% cannot be used with %1(). Ignoring\n").arg(name));

                TranslatorMessage msg(m_component, source,
                    comment, QString(), m_fileName,
                    node->firstSourceLocation().startLine, QStringList(),
                    TranslatorMessage::Unfinished, plural);
                msg.setExtraComment(extracomment.simplified());
                msg.setId(msgid);
                msg.setExtras(extra);
                m_translator->extend(msg, m_cd);
                consumeComment();
                break; }
            case TrFunctionAliasManager::Function_qsTranslate:
            case TrFunctionAliasManager::Function_QT_TRANSLATE_NOOP: {
                if (! (node->arguments && node->arguments->next)) {
                    yyMsg(identLineNo) << qPrintable(LU::tr("%1() requires at least two arguments.\n").arg(name));
                    return;
                }

                QString context;
                if (!createString(node->arguments->expression, &context))
                    return;

                AST::ArgumentList *sourceNode = node->arguments->next; // we know that it is a valid pointer.

                QString source;
                if (!createString(sourceNode->expression, &source))
                    return;

                if (!sourcetext.isEmpty())
                    yyMsg(identLineNo) << qPrintable(LU::tr("//% cannot be used with %1(). Ignoring\n").arg(name));

                QString comment;
                bool plural = false;
                if (AST::ArgumentList *commentNode = sourceNode->next) {
                    if (!createString(commentNode->expression, &comment)) {
                        comment.clear(); // clear possible invalid comments
                    }

                    if (commentNode->next)
                        plural = true;
                }

                TranslatorMessage msg(context, source,
                    comment, QString(), m_fileName,
                    node->firstSourceLocation().startLine, QStringList(),
                    TranslatorMessage::Unfinished, plural);
                msg.setExtraComment(extracomment.simplified());
                msg.setId(msgid);
                msg.setExtras(extra);
                m_translator->extend(msg, m_cd);
                consumeComment();
                break; }
            case TrFunctionAliasManager::Function_qsTrId:
            case TrFunctionAliasManager::Function_QT_TRID_NOOP: {
                if (!node->arguments) {
                    yyMsg(identLineNo) << qPrintable(LU::tr("%1() requires at least one argument.\n").arg(name));
                    return;
                }

                QString id;
                if (!createString(node->arguments->expression, &id))
                    return;

                if (!msgid.isEmpty()) {
                    yyMsg(identLineNo) << qPrintable(LU::tr("//= cannot be used with %1(). Ignoring\n").arg(name));
                    return;
                }

                bool plural = node->arguments->next;

                TranslatorMessage msg(QString(), sourcetext,
                    QString(), QString(), m_fileName,
                    node->firstSourceLocation().startLine, QStringList(),
                    TranslatorMessage::Unfinished, plural);
                msg.setExtraComment(extracomment.simplified());
                msg.setId(id);
                msg.setExtras(extra);
                m_translator->extend(msg, m_cd);
                consumeComment();
                break; }
            }
        }
    }

    virtual void postVisit(AST::Node *node);

private:
    std::ostream &yyMsg(int line)
    {
        return std::cerr << qPrintable(m_fileName) << ':' << line << ": ";
    }

    void processComments(quint32 offset, bool flush = false);
    void processComment(const AST::SourceLocation &loc);
    void consumeComment();

    bool createString(AST::ExpressionNode *ast, QString *out)
    {
        if (AST::StringLiteral *literal = AST::cast<AST::StringLiteral *>(ast)) {
            out->append(literal->value);
            return true;
        } else if (AST::BinaryExpression *binop = AST::cast<AST::BinaryExpression *>(ast)) {
            if (binop->op == QSOperator::Add && createString(binop->left, out)) {
                if (createString(binop->right, out))
                    return true;
            }
        }

        return false;
    }

    Engine *engine;
    Translator *m_translator;
    ConversionData &m_cd;
    QString m_fileName;
    QString m_component;

    // comments
    QString extracomment;
    QString msgid;
    TranslatorMessage::ExtraData extra;
    QString sourcetext;
    QString trcontext;
    QList<AST::SourceLocation> m_todo;
};

QString createErrorString(const QString &filename, const QString &code, Parser &parser)
{
    // print out error
    QStringList lines = code.split(QLatin1Char('\n'));
    lines.append(QLatin1String("\n")); // sentinel.
    QString errorString;

    foreach (const DiagnosticMessage &m, parser.diagnosticMessages()) {

        if (m.isWarning())
            continue;

        QString error = filename + QLatin1Char(':') + QString::number(m.loc.startLine)
                        + QLatin1Char(':') + QString::number(m.loc.startColumn) + QLatin1String(": error: ")
                        + m.message + QLatin1Char('\n');

        int line = 0;
        if (m.loc.startLine > 0)
            line = m.loc.startLine - 1;

        const QString textLine = lines.at(line);

        error += textLine + QLatin1Char('\n');

        int column = m.loc.startColumn - 1;
        if (column < 0)
            column = 0;

        column = qMin(column, textLine.length());

        for (int i = 0; i < column; ++i) {
            const QChar ch = textLine.at(i);
            if (ch.isSpace())
                error += ch.unicode();
            else
                error += QLatin1Char(' ');
        }
        error += QLatin1String("^\n");
        errorString += error;
    }
    return errorString;
}

void FindTrCalls::postVisit(AST::Node *node)
{
    if (node->statementCast() != 0 || node->uiObjectMemberCast()) {
        processComments(node->lastSourceLocation().end());

        if (!sourcetext.isEmpty() || !extracomment.isEmpty() || !msgid.isEmpty() || !extra.isEmpty()) {
            yyMsg(node->lastSourceLocation().startLine) << qPrintable(LU::tr("Discarding unconsumed meta data\n"));
            consumeComment();
        }
    }
}

void FindTrCalls::processComments(quint32 offset, bool flush)
{
    for (; !m_todo.isEmpty(); m_todo.removeFirst()) {
        AST::SourceLocation loc = m_todo.first();
        if (! flush && (loc.begin() >= offset))
            break;

        processComment(loc);
    }
}

void FindTrCalls::consumeComment()
{
    // keep the current `trcontext'
    extracomment.clear();
    msgid.clear();
    extra.clear();
    sourcetext.clear();
}

void FindTrCalls::processComment(const AST::SourceLocation &loc)
{
    if (!loc.length)
        return;

    const QStringRef commentStr = engine->midRef(loc.begin(), loc.length);
    const QChar *chars = commentStr.constData();
    const int length = commentStr.length();

    // Try to match the logic of the C++ parser.
    if (*chars == QLatin1Char(':') && chars[1].isSpace()) {
        if (!extracomment.isEmpty())
            extracomment += QLatin1Char(' ');
        extracomment += QString(chars+2, length-2);
    } else if (*chars == QLatin1Char('=') && chars[1].isSpace()) {
        msgid = QString(chars+2, length-2).simplified();
    } else if (*chars == QLatin1Char('~') && chars[1].isSpace()) {
        QString text = QString(chars+2, length-2).trimmed();
        int k = text.indexOf(QLatin1Char(' '));
        if (k > -1)
            extra.insert(text.left(k), text.mid(k + 1).trimmed());
    } else if (*chars == QLatin1Char('%') && chars[1].isSpace()) {
        sourcetext.reserve(sourcetext.length() + length-2);
        ushort *ptr = (ushort *)sourcetext.data() + sourcetext.length();
        int p = 2, c;
        forever {
            if (p >= length)
                break;
            c = chars[p++].unicode();
            if (std::isspace(c))
                continue;
            if (c != '"') {
                yyMsg(loc.startLine) << qPrintable(LU::tr("Unexpected character in meta string\n"));
                break;
            }
            forever {
                if (p >= length) {
                  whoops:
                    yyMsg(loc.startLine) << qPrintable(LU::tr("Unterminated meta string\n"));
                    break;
                }
                c = chars[p++].unicode();
                if (c == '"')
                    break;
                if (c == '\\') {
                    if (p >= length)
                        goto whoops;
                    c = chars[p++].unicode();
                    if (c == '\r' || c == '\n')
                        goto whoops;
                    *ptr++ = '\\';
                }
                *ptr++ = c;
            }
        }
        sourcetext.resize(ptr - (ushort *)sourcetext.data());
    } else {
        int idx = 0;
        ushort c;
        while ((c = chars[idx].unicode()) == ' ' || c == '\t' || c == '\r' || c == '\n')
            ++idx;
        if (!memcmp(chars + idx, MagicComment.unicode(), MagicComment.length() * 2)) {
            idx += MagicComment.length();
            QString comment = QString(chars + idx, length - idx).simplified();
            int k = comment.indexOf(QLatin1Char(' '));
            if (k == -1) {
                trcontext = comment;
            } else {
                trcontext = comment.left(k);
                comment.remove(0, k + 1);
                TranslatorMessage msg(
                        trcontext, QString(),
                        comment, QString(),
                        m_fileName, loc.startLine, QStringList(),
                        TranslatorMessage::Finished, /*plural=*/false);
                msg.setExtraComment(extracomment.simplified());
                extracomment.clear();
                m_translator->append(msg);
                m_translator->setExtras(extra);
                extra.clear();
            }

            m_component = trcontext;
        }
    }
}

class HasDirectives: public Directives
{
public:
    HasDirectives(Lexer *lexer)
        : lexer(lexer)
        , directives(0)
    {
    }

    bool operator()() const { return directives != 0; }
    int end() const { return lastOffset; }

    void pragmaLibrary() Q_DECL_OVERRIDE { consumeDirective(); }
    void importFile(const QString &, const QString &, int, int) Q_DECL_OVERRIDE { consumeDirective(); }
    void importModule(const QString &, const QString &, const QString &, int, int) Q_DECL_OVERRIDE { consumeDirective(); }

private:
    void consumeDirective()
    {
        ++directives;
        lastOffset = lexer->tokenOffset() + lexer->tokenLength();
    }

private:
    Lexer *lexer;
    int directives;
    int lastOffset;
};

static bool load(Translator &translator, const QString &filename, ConversionData &cd, bool qmlMode)
{
    cd.m_sourceFileName = filename;
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly)) {
        cd.appendError(LU::tr("Cannot open %1: %2").arg(filename, file.errorString()));
        return false;
    }

    QString code;
    if (!qmlMode) {
        code = QTextStream(&file).readAll();
    } else {
        QTextStream ts(&file);
        ts.setCodec("UTF-8");
        ts.setAutoDetectUnicode(true);
        code = ts.readAll();
    }

    Engine driver;
    Parser parser(&driver);

    Lexer lexer(&driver);
    lexer.setCode(code, /*line = */ 1, qmlMode);
    driver.setLexer(&lexer);

    if (qmlMode ? parser.parse() : parser.parseProgram()) {
        FindTrCalls trCalls(&driver, cd);

        //find all tr calls in the code
        trCalls(&translator, filename, parser.rootNode());
    } else {
        QString error = createErrorString(filename, code, parser);
        cd.appendError(error);
        return false;
    }
    return true;
}

bool loadQml(Translator &translator, const QString &filename, ConversionData &cd)
{
    return load(translator, filename, cd, /*qmlMode=*/ true);
}

bool loadQScript(Translator &translator, const QString &filename, ConversionData &cd)
{
    return load(translator, filename, cd, /*qmlMode=*/ false);
}

QT_END_NAMESPACE
