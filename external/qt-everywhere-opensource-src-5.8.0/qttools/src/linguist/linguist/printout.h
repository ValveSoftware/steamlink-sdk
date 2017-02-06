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

#ifndef PRINTOUT_H
#define PRINTOUT_H

#include <QFont>
#include <QPainter>
#include <QRect>
#include <QTextOption>
#include <QList>
#include <QDateTime>

QT_BEGIN_NAMESPACE

class QPrinter;
class QFontMetrics;

class PrintOut
{
public:
    enum Rule { NoRule, ThinRule, ThickRule };
    enum Style { Normal, Strong, Emphasis };

    PrintOut(QPrinter *printer);
    ~PrintOut();

    void setRule(Rule rule);
    void setGuide(const QString &guide);
    void vskip();
    void flushLine(bool mayBreak = false);
    void addBox(int percent, const QString &text = QString(),
                Style style = Normal,
                Qt::Alignment halign = Qt::AlignLeft);

    int pageNum() const { return page; }

    struct Box
    {
        QRect rect;
        QString text;
        QFont font;
        QTextOption options;

        Box( const QRect& r, const QString& t, const QFont& f, const QTextOption &o )
            : rect( r ), text( t ), font( f ), options( o ) { }
    };

private:
    void breakPage(bool init = false);
    void drawRule( Rule rule );

    struct Paragraph {
        QRect rect;
        QList<Box> boxes;

        Paragraph() { }
        Paragraph( QPoint p ) : rect( p, QSize(0, 0) ) { }
    };

    QPrinter *pr;
    QPainter p;
    QFont f8;
    QFont f10;
    QFontMetrics *fmetrics;
    Rule nextRule;
    Paragraph cp;
    int page;
    bool firstParagraph;
    QString g;
    QDateTime dateTime;

    int hmargin;
    int vmargin;
    int voffset;
    int hsize;
    int vsize;
};

QT_END_NAMESPACE

#endif
