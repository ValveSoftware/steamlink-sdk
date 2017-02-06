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

#ifndef LUPDATE_H
#define LUPDATE_H

#include "qglobal.h"

#include <QList>
#include <QString>
#include <QStringList>
#include <QHash>

QT_BEGIN_NAMESPACE

class ConversionData;
class QStringList;
class Translator;
class TranslatorMessage;

enum UpdateOption {
    Verbose = 1,
    NoObsolete = 2,
    PluralOnly = 4,
    NoSort = 8,
    HeuristicSameText = 16,
    HeuristicSimilarText = 32,
    HeuristicNumber = 64,
    AbsoluteLocations = 256,
    RelativeLocations = 512,
    NoLocations = 1024,
    NoUiLines = 2048,
    SourceIsUtf16 = 4096
};

Q_DECLARE_FLAGS(UpdateOptions, UpdateOption)
Q_DECLARE_OPERATORS_FOR_FLAGS(UpdateOptions)

Translator merge(
    const Translator &tor, const Translator &virginTor, const QList<Translator> &aliens,
    UpdateOptions options, QString &err);

void loadCPP(Translator &translator, const QStringList &filenames, ConversionData &cd);
bool loadJava(Translator &translator, const QString &filename, ConversionData &cd);
bool loadUI(Translator &translator, const QString &filename, ConversionData &cd);

#ifndef QT_NO_QML
bool loadQScript(Translator &translator, const QString &filename, ConversionData &cd);
bool loadQml(Translator &translator, const QString &filename, ConversionData &cd);
#endif

#define LUPDATE_FOR_EACH_TR_FUNCTION(UNARY_MACRO) \
    /* from cpp.cpp */ \
    UNARY_MACRO(Q_DECLARE_TR_FUNCTIONS) \
    UNARY_MACRO(QT_TR_NOOP) \
    UNARY_MACRO(QT_TRID_NOOP) \
    UNARY_MACRO(QT_TRANSLATE_NOOP) \
    UNARY_MACRO(QT_TRANSLATE_NOOP3) \
    UNARY_MACRO(QT_TR_NOOP_UTF8) \
    UNARY_MACRO(QT_TRANSLATE_NOOP_UTF8) \
    UNARY_MACRO(QT_TRANSLATE_NOOP3_UTF8) \
    UNARY_MACRO(findMessage) /* QTranslator::findMessage() has the same parameters as QApplication::translate() */ \
    UNARY_MACRO(qtTrId) \
    UNARY_MACRO(tr) \
    UNARY_MACRO(trUtf8) \
    UNARY_MACRO(translate) \
    /* from qdeclarative.cpp: */ \
    UNARY_MACRO(qsTr) \
    UNARY_MACRO(qsTrId) \
    UNARY_MACRO(qsTranslate) \
    /*end*/


class TrFunctionAliasManager {
public:
    TrFunctionAliasManager();
    ~TrFunctionAliasManager();

    enum TrFunction {
        // need to uglify names b/c most of the names are themselves macros:
#define MAKE_ENTRY(F) Function_##F,
        LUPDATE_FOR_EACH_TR_FUNCTION(MAKE_ENTRY)
#undef MAKE_ENTRY
        NumTrFunctions
    };

    enum Operation { AddAlias, SetAlias };

    int trFunctionByName(const QString &trFunctionName) const;

    void modifyAlias(int trFunction, const QString &alias, Operation op);

    bool isAliasFor(const QString &identifier, TrFunction trFunction) const
    { return m_trFunctionAliases[trFunction].contains(identifier); }

    QStringList availableFunctionsWithAliases() const;

private:
    void ensureTrFunctionHashUpdated() const;

private:
    QStringList m_trFunctionAliases[NumTrFunctions];
    mutable QHash<QString,TrFunction> m_nameToTrFunctionMap;
};

QT_END_NAMESPACE

extern QT_PREPEND_NAMESPACE(TrFunctionAliasManager) trFunctionAliasManager;

#endif
