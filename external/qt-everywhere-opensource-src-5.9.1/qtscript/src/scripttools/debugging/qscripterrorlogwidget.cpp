/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtSCriptTools module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
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
** As a special exception, The Qt Company gives you certain additional
** rights. These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qscripterrorlogwidget_p.h"
#include "qscripterrorlogwidgetinterface_p_p.h"

#include <QtCore/qdatetime.h>
#include <QtWidgets/qboxlayout.h>
#include <QtWidgets/qtextedit.h>
#include <QtWidgets/qscrollbar.h>
#include <QtCore/qdebug.h>

QT_BEGIN_NAMESPACE

namespace {

class QScriptErrorLogWidgetOutputEdit : public QTextEdit
{
public:
    QScriptErrorLogWidgetOutputEdit(QWidget *parent = 0)
        : QTextEdit(parent)
    {
        setReadOnly(true);
//        setFocusPolicy(Qt::NoFocus);
        document()->setMaximumBlockCount(255);
    }

    void scrollToBottom()
    {
        QScrollBar *bar = verticalScrollBar();
        bar->setValue(bar->maximum());
    }
};

} // namespace

class QScriptErrorLogWidgetPrivate
    : public QScriptErrorLogWidgetInterfacePrivate
{
    Q_DECLARE_PUBLIC(QScriptErrorLogWidget)
public:
    QScriptErrorLogWidgetPrivate();
    ~QScriptErrorLogWidgetPrivate();

    QScriptErrorLogWidgetOutputEdit *outputEdit;
};

QScriptErrorLogWidgetPrivate::QScriptErrorLogWidgetPrivate()
{
}

QScriptErrorLogWidgetPrivate::~QScriptErrorLogWidgetPrivate()
{
}

QScriptErrorLogWidget::QScriptErrorLogWidget(QWidget *parent)
    : QScriptErrorLogWidgetInterface(*new QScriptErrorLogWidgetPrivate, parent, 0)
{
    Q_D(QScriptErrorLogWidget);
    d->outputEdit = new QScriptErrorLogWidgetOutputEdit();
    QVBoxLayout *vbox = new QVBoxLayout(this);
    vbox->setMargin(0);
    vbox->setSpacing(0);
    vbox->addWidget(d->outputEdit);

//    QString sheet = QString::fromLatin1("font-size: 14px; font-family: \"Monospace\";");
//    setStyleSheet(sheet);
}

QScriptErrorLogWidget::~QScriptErrorLogWidget()
{
}

void QScriptErrorLogWidget::message(
    QtMsgType type, const QString &text, const QString &fileName,
    int lineNumber, int columnNumber, const QVariant &/*data*/)
{
    // ### we need the error message rather than Error.toString()
    Q_UNUSED(type);
    Q_UNUSED(fileName);
    Q_UNUSED(lineNumber);
    Q_UNUSED(columnNumber);
    Q_D(QScriptErrorLogWidget);
    QString html;
    html.append(QString::fromLatin1("<b>%0</b> %1<br>")
                .arg(QDateTime::currentDateTime().toString()).arg(text.toHtmlEscaped()));
    d->outputEdit->insertHtml(html);
    d->outputEdit->scrollToBottom();
}

void QScriptErrorLogWidget::clear()
{
    Q_D(QScriptErrorLogWidget);
    d->outputEdit->clear();
}

QT_END_NAMESPACE
