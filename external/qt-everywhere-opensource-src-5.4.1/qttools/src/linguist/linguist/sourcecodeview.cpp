/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Linguist of the Qt Toolkit.
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

#include "sourcecodeview.h"

#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QTextStream>

#include <QtGui/QTextCharFormat>
#include <QtGui/QTextBlock>
#include <QtGui/QTextCursor>

QT_BEGIN_NAMESPACE

SourceCodeView::SourceCodeView(QWidget *parent)
  : QPlainTextEdit(parent),
    m_isActive(true),
    m_lineNumToLoad(0)
{
    setReadOnly(true);
}

void SourceCodeView::setSourceContext(const QString &fileName, const int lineNum)
{
    m_fileToLoad.clear();
    setToolTip(fileName);

    if (fileName.isEmpty()) {
        clear();
        m_currentFileName.clear();
        appendHtml(tr("<i>Source code not available</i>"));
        return;
    }

    if (m_isActive) {
        showSourceCode(fileName, lineNum);
    } else {
        m_fileToLoad = fileName;
        m_lineNumToLoad = lineNum;
    }
}

void SourceCodeView::setActivated(bool activated)
{
    m_isActive = activated;
    if (activated && !m_fileToLoad.isEmpty()) {
        showSourceCode(m_fileToLoad, m_lineNumToLoad);
        m_fileToLoad.clear();
    }
}

void SourceCodeView::showSourceCode(const QString &absFileName, const int lineNum)
{
    QString fileText = fileHash.value(absFileName);

    if (fileText.isNull()) { // File not in hash
        m_currentFileName.clear();

        // Assume fileName is relative to directory
        QFile file(absFileName);

        if (!file.exists()) {
            clear();
            appendHtml(tr("<i>File %1 not available</i>").arg(absFileName));
            return;
        }
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            clear();
            appendHtml(tr("<i>File %1 not readable</i>").arg(absFileName));
            return;
        }
        fileText = QString::fromUtf8(file.readAll());
        fileHash.insert(absFileName, fileText);
    }


    if (m_currentFileName != absFileName) {
        setPlainText(fileText);
        m_currentFileName = absFileName;
    }

    QTextCursor cursor = textCursor();
    cursor.setPosition(document()->findBlockByNumber(lineNum - 1).position());
    setTextCursor(cursor);
    centerCursor();
    cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
    cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor);

    QTextEdit::ExtraSelection selectedLine;
    selectedLine.cursor = cursor;

    // Define custom color for line selection
    const QColor fg = palette().color(QPalette::Highlight);
    const QColor bg = palette().color(QPalette::Base);
    QColor col;
    const qreal ratio = 0.25;
    col.setRedF(fg.redF() * ratio + bg.redF() * (1 - ratio));
    col.setGreenF(fg.greenF() * ratio + bg.greenF() * (1 - ratio));
    col.setBlueF(fg.blueF() * ratio + bg.blueF() * (1 - ratio));

    selectedLine.format.setBackground(col);
    selectedLine.format.setProperty(QTextFormat::FullWidthSelection, true);
    setExtraSelections(QList<QTextEdit::ExtraSelection>() << selectedLine);
}

QT_END_NAMESPACE
