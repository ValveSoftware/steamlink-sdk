/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQml module of the Qt Toolkit.
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

#include <private/qqmljsengine_p.h>
#include <private/qqmljslexer_p.h>
#include <private/qqmljsparser_p.h>
#include <QtCore/QCoreApplication>
#include <QtCore/QStringList>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QDir>
#include <iostream>
#include <cstdlib>

QT_BEGIN_NAMESPACE

//
// QML/JS minifier
//
namespace QQmlJS {

enum RegExpFlag {
    Global     = 0x01,
    IgnoreCase = 0x02,
    Multiline  = 0x04
};


class QmlminLexer: protected Lexer, public Directives
{
    QQmlJS::Engine _engine;
    QString _fileName;
    QString _directives;

public:
    QmlminLexer(): Lexer(&_engine) {}
    virtual ~QmlminLexer() {}

    QString fileName() const { return _fileName; }

    bool operator()(const QString &fileName, const QString &code)
    {
        int startToken = T_FEED_JS_PROGRAM;
        const QFileInfo fileInfo(fileName);
        if (fileInfo.suffix().toLower() == QLatin1String("qml"))
            startToken = T_FEED_UI_PROGRAM;
        setCode(code, /*line = */ 1, /*qmlMode = */ startToken == T_FEED_UI_PROGRAM);
        _fileName = fileName;
        _directives.clear();
        return parse(startToken);
    }

    QString directives()
    {
        return _directives;
    }

    //
    // Handle the .pragma/.import directives
    //
    virtual void pragmaLibrary()
    {
        _directives += QLatin1String(".pragma library\n");
    }

    virtual void importFile(const QString &jsfile, const QString &module, int line, int column)
    {
        _directives += QLatin1String(".import");
        _directives += QLatin1Char('"');
        _directives += quote(jsfile);
        _directives += QLatin1Char('"');
        _directives += QLatin1String("as ");
        _directives += module;
        _directives += QLatin1Char('\n');
        Q_UNUSED(line);
        Q_UNUSED(column);
    }

    virtual void importModule(const QString &uri, const QString &version, const QString &module, int line, int column)
    {
        _directives += QLatin1String(".import ");
        _directives += uri;
        _directives += QLatin1Char(' ');
        _directives += version;
        _directives += QLatin1String(" as ");
        _directives += module;
        _directives += QLatin1Char('\n');
        Q_UNUSED(line);
        Q_UNUSED(column);
    }

protected:
    virtual bool parse(int startToken) = 0;

    static QString quote(const QString &string)
    {
        QString quotedString;
        for (const QChar &ch : string) {
            if (ch == QLatin1Char('"'))
                quotedString += QLatin1String("\\\"");
            else {
                if (ch == QLatin1Char('\\')) quotedString += QLatin1String("\\\\");
                else if (ch == QLatin1Char('\"')) quotedString += QLatin1String("\\\"");
                else if (ch == QLatin1Char('\b')) quotedString += QLatin1String("\\b");
                else if (ch == QLatin1Char('\f')) quotedString += QLatin1String("\\f");
                else if (ch == QLatin1Char('\n')) quotedString += QLatin1String("\\n");
                else if (ch == QLatin1Char('\r')) quotedString += QLatin1String("\\r");
                else if (ch == QLatin1Char('\t')) quotedString += QLatin1String("\\t");
                else if (ch == QLatin1Char('\v')) quotedString += QLatin1String("\\v");
                else if (ch == QLatin1Char('\0')) quotedString += QLatin1String("\\0");
                else quotedString += ch;
            }
        }
        return quotedString;
    }

    bool isIdentChar(const QChar &ch) const
    {
        if (ch.isLetterOrNumber())
            return true;
        else if (ch == QLatin1Char('_') || ch == QLatin1Char('$'))
            return true;
        return false;
    }

    bool isRegExpRule(int ruleno) const
    {
        return ruleno == J_SCRIPT_REGEXPLITERAL_RULE1 ||
                ruleno == J_SCRIPT_REGEXPLITERAL_RULE2;
    }

    bool scanRestOfRegExp(int ruleno, QString *restOfRegExp)
    {
        if (! scanRegExp(ruleno == J_SCRIPT_REGEXPLITERAL_RULE1 ? Lexer::NoPrefix : Lexer::EqualPrefix))
            return false;

        *restOfRegExp = regExpPattern();
        if (ruleno == J_SCRIPT_REGEXPLITERAL_RULE2) {
            Q_ASSERT(! restOfRegExp->isEmpty());
            Q_ASSERT(restOfRegExp->at(0) == QLatin1Char('='));
            *restOfRegExp = restOfRegExp->mid(1); // strip the prefix
        }
        *restOfRegExp += QLatin1Char('/');
        const RegExpFlag flags = (RegExpFlag) regExpFlags();
        if (flags & Global)
            *restOfRegExp += QLatin1Char('g');
        if (flags & IgnoreCase)
            *restOfRegExp += QLatin1Char('i');
        if (flags & Multiline)
            *restOfRegExp += QLatin1Char('m');

        if (regExpFlags() == 0) {
            // Add an extra space after the regexp literal delimiter (aka '/').
            // This will avoid possible problems when pasting tokens like `instanceof'
            // after the regexp literal.
            *restOfRegExp += QLatin1Char(' ');
        }
        return true;
    }
};


class Minify: public QmlminLexer
{
    QVector<int> _stateStack;
    QList<int> _tokens;
    QList<QString> _tokenStrings;
    QString _minifiedCode;
    int _maxWidth;
    int _width;

public:
    Minify(int maxWidth);

    QString minifiedCode() const;

protected:
    void append(const QString &s);
    bool parse(int startToken);
    void escape(const QChar &ch, QString *out);
};

Minify::Minify(int maxWidth)
    : _stateStack(128), _maxWidth(maxWidth), _width(0)
{
}

QString Minify::minifiedCode() const
{
    return _minifiedCode;
}

void Minify::append(const QString &s)
{
    if (!s.isEmpty()) {
        if (_maxWidth) {
            // Prefer not to exceed the maximum chars per line (but don't break up segments)
            int segmentLength = s.count();
            if (_width && ((_width + segmentLength) > _maxWidth)) {
                _minifiedCode.append(QLatin1Char('\n'));
                _width = 0;
            }

            _width += segmentLength;
        }

        _minifiedCode.append(s);
    }
}

void Minify::escape(const QChar &ch, QString *out)
{
    out->append(QLatin1String("\\u"));
    const QString hx = QString::number(ch.unicode(), 16);
    switch (hx.length()) {
    case 1: out->append(QLatin1String("000")); break;
    case 2: out->append(QLatin1String("00")); break;
    case 3: out->append(QLatin1Char('0')); break;
    case 4: break;
    default: Q_ASSERT(!"unreachable");
    }
    out->append(hx);
}

bool Minify::parse(int startToken)
{
    int yyaction = 0;
    int yytoken = -1;
    int yytos = -1;
    QString yytokentext;
    QString assembled;

    _minifiedCode.clear();
    _tokens.append(startToken);
    _tokenStrings.append(QString());

    if (startToken == T_FEED_JS_PROGRAM) {
        // parse optional pragma directive
        DiagnosticMessage error;
        if (scanDirectives(this, &error)) {
            // append the scanned directives to the minifier code.
            append(directives());

            _tokens.append(tokenKind());
            _tokenStrings.append(tokenText());
        } else {
            std::cerr << qPrintable(fileName()) << ':' << tokenStartLine() << ':'
                << tokenStartColumn() << ": syntax error" << std::endl;
            return false;
        }
    }

    do {
        if (++yytos == _stateStack.size())
            _stateStack.resize(_stateStack.size() * 2);

        _stateStack[yytos] = yyaction;

    again:
        if (yytoken == -1 && action_index[yyaction] != -TERMINAL_COUNT) {
            if (_tokens.isEmpty()) {
                _tokens.append(lex());
                _tokenStrings.append(tokenText());
            }

            yytoken = _tokens.takeFirst();
            yytokentext = _tokenStrings.takeFirst();
        }

        yyaction = t_action(yyaction, yytoken);
        if (yyaction > 0) {
            if (yyaction == ACCEPT_STATE) {
                --yytos;
                if (!assembled.isEmpty())
                    append(assembled);
                return true;
            }

            const QChar lastChar = assembled.isEmpty() ? (_minifiedCode.isEmpty() ? QChar()
                                                                                  : _minifiedCode.at(_minifiedCode.length() - 1))
                                                       : assembled.at(assembled.length() - 1);

            if (yytoken == T_SEMICOLON) {
                assembled += QLatin1Char(';');

                append(assembled);
                assembled.clear();

            } else if (yytoken == T_PLUS || yytoken == T_MINUS || yytoken == T_PLUS_PLUS || yytoken == T_MINUS_MINUS) {
                if (lastChar == QLatin1Char(spell[yytoken][0])) {
                    // don't merge unary signs, additive expressions and postfix/prefix increments.
                    assembled += QLatin1Char(' ');
                }

                assembled += QLatin1String(spell[yytoken]);

            } else if (yytoken == T_NUMERIC_LITERAL) {
                if (isIdentChar(lastChar))
                    assembled += QLatin1Char(' ');

                if (yytokentext.startsWith('.'))
                    assembled += QLatin1Char('0');

                assembled += yytokentext;

                if (assembled.endsWith(QLatin1Char('.')))
                    assembled += QLatin1Char('0');

            } else if (yytoken == T_IDENTIFIER) {
                QString identifier = yytokentext;

                if (classify(identifier.constData(), identifier.size(), qmlMode()) != T_IDENTIFIER) {
                    // the unescaped identifier is a keyword. In this case just replace
                    // the last character of the identifier with it escape sequence.
                    const QChar ch = identifier.at(identifier.length() - 1);
                    identifier.chop(1);
                    escape(ch, &identifier);
                }

                if (isIdentChar(lastChar))
                    assembled += QLatin1Char(' ');

                assembled += identifier;

            } else if (yytoken == T_STRING_LITERAL || yytoken == T_MULTILINE_STRING_LITERAL) {
                assembled += QLatin1Char('"');
                assembled += quote(yytokentext);
                assembled += QLatin1Char('"');
            } else {
                if (isIdentChar(lastChar)) {
                    if (! yytokentext.isEmpty()) {
                        const QChar ch = yytokentext.at(0);
                        if (isIdentChar(ch))
                            assembled += QLatin1Char(' ');
                    }
                }
                assembled += yytokentext;
            }
            yytoken = -1;
        } else if (yyaction < 0) {
            const int ruleno = -yyaction - 1;
            yytos -= rhs[ruleno];

            if (isRegExpRule(ruleno)) {
                QString restOfRegExp;

                if (! scanRestOfRegExp(ruleno, &restOfRegExp))
                    break; // break the loop, it wil report a syntax error

                assembled += restOfRegExp;
            }
            yyaction = nt_action(_stateStack[yytos], lhs[ruleno] - TERMINAL_COUNT);
        }
    } while (yyaction);

    const int yyerrorstate = _stateStack[yytos];

    // automatic insertion of `;'
    if (yytoken != -1 && ((t_action(yyerrorstate, T_AUTOMATIC_SEMICOLON) && canInsertAutomaticSemicolon(yytoken))
                          || t_action(yyerrorstate, T_COMPATIBILITY_SEMICOLON))) {
        _tokens.prepend(yytoken);
        _tokenStrings.prepend(yytokentext);
        yyaction = yyerrorstate;
        yytoken = T_SEMICOLON;
        goto again;
    }

    std::cerr << qPrintable(fileName()) << ':' << tokenStartLine() << ':' << tokenStartColumn()
         << ": syntax error" << std::endl;
    return false;
}


class Tokenize: public QmlminLexer
{
    QVector<int> _stateStack;
    QList<int> _tokens;
    QList<QString> _tokenStrings;
    QStringList _minifiedCode;

public:
    Tokenize();

    QStringList tokenStream() const;

protected:
    virtual bool parse(int startToken);
};

Tokenize::Tokenize()
    : _stateStack(128)
{
}

QStringList Tokenize::tokenStream() const
{
    return _minifiedCode;
}

bool Tokenize::parse(int startToken)
{
    int yyaction = 0;
    int yytoken = -1;
    int yytos = -1;
    QString yytokentext;

    _minifiedCode.clear();
    _tokens.append(startToken);
    _tokenStrings.append(QString());

    if (startToken == T_FEED_JS_PROGRAM) {
        // parse optional pragma directive
        DiagnosticMessage error;
        if (scanDirectives(this, &error)) {
            // append the scanned directives as one token to
            // the token stream.
            _minifiedCode.append(directives());

            _tokens.append(tokenKind());
            _tokenStrings.append(tokenText());
        } else {
            std::cerr << qPrintable(fileName()) << ':' << tokenStartLine() << ':'
                << tokenStartColumn() << ": syntax error" << std::endl;
            return false;
        }
    }

    do {
        if (++yytos == _stateStack.size())
            _stateStack.resize(_stateStack.size() * 2);

        _stateStack[yytos] = yyaction;

    again:
        if (yytoken == -1 && action_index[yyaction] != -TERMINAL_COUNT) {
            if (_tokens.isEmpty()) {
                _tokens.append(lex());
                _tokenStrings.append(tokenText());
            }

            yytoken = _tokens.takeFirst();
            yytokentext = _tokenStrings.takeFirst();
        }

        yyaction = t_action(yyaction, yytoken);
        if (yyaction > 0) {
            if (yyaction == ACCEPT_STATE) {
                --yytos;
                return true;
            }

            if (yytoken == T_SEMICOLON)
                _minifiedCode += QLatin1String(";");
            else
                _minifiedCode += yytokentext;

            yytoken = -1;
        } else if (yyaction < 0) {
            const int ruleno = -yyaction - 1;
            yytos -= rhs[ruleno];

            if (isRegExpRule(ruleno)) {
                QString restOfRegExp;

                if (! scanRestOfRegExp(ruleno, &restOfRegExp))
                    break; // break the loop, it wil report a syntax error

                _minifiedCode.last().append(restOfRegExp);
            }

            yyaction = nt_action(_stateStack[yytos], lhs[ruleno] - TERMINAL_COUNT);
        }
    } while (yyaction);

    const int yyerrorstate = _stateStack[yytos];

    // automatic insertion of `;'
    if (yytoken != -1 && ((t_action(yyerrorstate, T_AUTOMATIC_SEMICOLON) && canInsertAutomaticSemicolon(yytoken))
                          || t_action(yyerrorstate, T_COMPATIBILITY_SEMICOLON))) {
        _tokens.prepend(yytoken);
        _tokenStrings.prepend(yytokentext);
        yyaction = yyerrorstate;
        yytoken = T_SEMICOLON;
        goto again;
    }

    std::cerr << qPrintable(fileName()) << ':' << tokenStartLine() << ':'
        << tokenStartColumn() << ": syntax error" << std::endl;
    return false;
}

} // end of QQmlJS namespace

static void usage(bool showHelp = false)
{
    std::cerr << "Usage: qmlmin [options] file" << std::endl;

    if (showHelp) {
        std::cerr << " Removes comments and layout characters" << std::endl
                  << " The options are:" << std::endl
                  << "  -o<file>                write output to file rather than stdout" << std::endl
                  << "  -v --verify-only        just run the verifier, no output" << std::endl
                  << "  -w<width>               restrict line characters to width" << std::endl
                  << "  -h                      display this output" << std::endl;
    }
}

int runQmlmin(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationVersion(QLatin1String(QT_VERSION_STR));

    const QStringList args = app.arguments();

    QString fileName;
    QString outputFile;
    bool verifyOnly = false;

    // By default ensure the output character width is less than 16-bits (pass 0 to disable)
    int width = USHRT_MAX;

    int index = 1;
    while (index < args.size()) {
        const QString arg = args.at(index++);
        const QString next = index < args.size() ? args.at(index) : QString();

        if (arg == QLatin1String("-h") || arg == QLatin1String("--help")) {
            usage(/*showHelp*/ true);
            return 0;
        } else if (arg == QLatin1String("-v") || arg == QLatin1String("--verify-only")) {
            verifyOnly = true;
        } else if (arg == QLatin1String("-o")) {
            if (next.isEmpty()) {
                std::cerr << "qmlmin: argument to '-o' is missing" << std::endl;
                return EXIT_FAILURE;
            } else {
                outputFile = next;
                ++index; // consume the next argument
            }
        } else if (arg.startsWith(QLatin1String("-o"))) {
            outputFile = arg.mid(2);

            if (outputFile.isEmpty()) {
                std::cerr << "qmlmin: argument to '-o' is missing" << std::endl;
                return EXIT_FAILURE;
            }
        } else if (arg == QLatin1String("-w")) {
            if (next.isEmpty()) {
                std::cerr << "qmlmin: argument to '-w' is missing" << std::endl;
                return EXIT_FAILURE;
            } else {
                bool ok;
                width = next.toInt(&ok);

                if (!ok) {
                    std::cerr << "qmlmin: argument to '-w' is invalid" << std::endl;
                    return EXIT_FAILURE;
                }

                ++index; // consume the next argument
            }
        } else if (arg.startsWith(QLatin1String("-w"))) {
            bool ok;
            width = arg.midRef(2).toInt(&ok);

            if (!ok) {
                std::cerr << "qmlmin: argument to '-w' is invalid" << std::endl;
                return EXIT_FAILURE;
            }
        } else {
            const bool isInvalidOpt = arg.startsWith(QLatin1Char('-'));
            if (! isInvalidOpt && fileName.isEmpty())
                fileName = arg;
            else {
                usage(/*show help*/ isInvalidOpt);
                if (isInvalidOpt)
                    std::cerr << "qmlmin: invalid option '" << qPrintable(arg) << '\'' << std::endl;
                else
                    std::cerr << "qmlmin: too many input files specified" << std::endl;
                return EXIT_FAILURE;
            }
        }
    }

    if (fileName.isEmpty()) {
        usage();
        return 0;
    }

    QFile file(fileName);
    if (! file.open(QFile::ReadOnly)) {
        std::cerr << "qmlmin: '" << qPrintable(fileName) << "' no such file or directory" << std::endl;
        return EXIT_FAILURE;
    }

    const QString code = QString::fromUtf8(file.readAll()); // QML files are UTF-8 encoded.
    file.close();

    QQmlJS::Minify minify(width);
    if (! minify(fileName, code)) {
        std::cerr << "qmlmin: cannot minify '" << qPrintable(fileName) << "' (not a valid QML/JS file)" << std::endl;
        return EXIT_FAILURE;
    }

    //
    // verify the output
    //
    QQmlJS::Minify secondMinify(width);
    if (! secondMinify(fileName, minify.minifiedCode()) || secondMinify.minifiedCode() != minify.minifiedCode()) {
        std::cerr << "qmlmin: cannot minify '" << qPrintable(fileName) << '\'' << std::endl;
        return EXIT_FAILURE;
    }

    QQmlJS::Tokenize originalTokens, minimizedTokens;
    originalTokens(fileName, code);
    minimizedTokens(fileName, minify.minifiedCode());

    if (originalTokens.tokenStream().size() != minimizedTokens.tokenStream().size()) {
        std::cerr << "qmlmin: cannot minify '" << qPrintable(fileName) << '\'' << std::endl;
        return EXIT_FAILURE;
    }

    if (! verifyOnly) {
        if (outputFile.isEmpty()) {
            const QByteArray chars = minify.minifiedCode().toUtf8();
            std::cout << chars.constData();
        } else {
            QFile file(outputFile);
            if (! file.open(QFile::WriteOnly)) {
                std::cerr << "qmlmin: cannot minify '" << qPrintable(fileName) << "' (permission denied)" << std::endl;
                return EXIT_FAILURE;
            }

            file.write(minify.minifiedCode().toUtf8());
            file.close();
        }
    }

    return 0;
}

QT_END_NAMESPACE

int main(int argc, char **argv)
{
    return QT_PREPEND_NAMESPACE(runQmlmin(argc, argv));
}
