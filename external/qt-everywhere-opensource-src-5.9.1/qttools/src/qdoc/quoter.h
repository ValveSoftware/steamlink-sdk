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
  quoter.h
*/

#ifndef QUOTER_H
#define QUOTER_H

#include <qhash.h>
#include <qstringlist.h>

#include "location.h"

QT_BEGIN_NAMESPACE

class Quoter
{
    Q_DECLARE_TR_FUNCTIONS(QDoc::Quoter)

public:
    Quoter();

    void reset();
    void quoteFromFile( const QString& userFriendlyFileName,
                        const QString& plainCode, const QString& markedCode );
    QString quoteLine( const Location& docLocation, const QString& command,
                       const QString& pattern );
    QString quoteTo( const Location& docLocation, const QString& command,
                     const QString& pattern );
    QString quoteUntil( const Location& docLocation, const QString& command,
                        const QString& pattern );
    QString quoteSnippet(const Location &docLocation, const QString &identifier);

    static QStringList splitLines(const QString &line);

private:
    QString getLine(int unindent = 0);
    void failedAtEnd( const Location& docLocation, const QString& command );
    bool match( const Location& docLocation, const QString& pattern,
                const QString& line );
    QString commentForCode() const;
    QString removeSpecialLines(const QString &line, const QString &comment,
                               int unindent = 0);

    bool silent;
    QStringList plainLines;
    QStringList markedLines;
    Location codeLocation;
    static QHash<QString,QString> commentHash;
};

QT_END_NAMESPACE

#endif
