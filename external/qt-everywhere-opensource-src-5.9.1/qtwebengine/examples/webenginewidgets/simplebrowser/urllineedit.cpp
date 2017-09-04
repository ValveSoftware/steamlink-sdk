/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "urllineedit.h"
#include <QToolButton>
#include <QUrl>

UrlLineEdit::UrlLineEdit(QWidget *parent)
    : QLineEdit(parent)
    , m_favButton(new QToolButton(this))
    , m_clearButton(new QToolButton(this))
{
    m_clearButton->setIcon(QIcon(QStringLiteral(":closetab.png")));
    m_clearButton->setVisible(false);
    m_clearButton->setCursor(Qt::ArrowCursor);
    QString style(QStringLiteral("QToolButton { border: none; padding: 1px; }"));
    m_clearButton->setStyleSheet(style);
    m_favButton->setStyleSheet(style);
    setStyleSheet(QStringLiteral("QLineEdit { padding-left: %1px; padding-right: %2px;  } ")
                  .arg(m_clearButton->sizeHint().width())
                  .arg(m_favButton->sizeHint().width()));
    int minIconHeight = qMax(m_favButton->sizeHint().height(), m_clearButton->sizeHint().height());
    setMinimumSize(minimumSizeHint().width() +
                   m_favButton->sizeHint().width() +
                   m_clearButton->sizeHint().width(),
                   qMax(minimumSizeHint().height(), minIconHeight));

    connect(m_clearButton, &QToolButton::clicked, this, &QLineEdit::clear);
    connect(this, &QLineEdit::textChanged, [this](const QString &text) {
        m_clearButton->setVisible(!text.isEmpty() && !isReadOnly());
    });
}

QUrl UrlLineEdit::url() const
{
    return QUrl::fromUserInput(text());
}

void UrlLineEdit::setUrl(const QUrl &url)
{
    setText(url.toString());
    setCursorPosition(0);
}

void UrlLineEdit::setFavIcon(const QIcon &icon)
{
    QPixmap pixmap = icon.pixmap(16, 16);
    m_favButton->setIcon(pixmap);
}

void UrlLineEdit::resizeEvent(QResizeEvent *event)
{
    QLineEdit::resizeEvent(event);
    QSize clearButtonSize = m_clearButton->sizeHint();
    m_clearButton->move(rect().right() - clearButtonSize.width(),
                        (rect().bottom() - clearButtonSize.height()) / 2);
    m_favButton->move(rect().left(), (rect().bottom() - m_favButton->sizeHint().height()) / 2);
}
