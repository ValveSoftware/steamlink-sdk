/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Assistant of the Qt Toolkit.
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
#include "tracer.h"
#include "findwidget.h"

#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
#include <QtGui/QHideEvent>
#include <QtGui/QKeyEvent>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLayout>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QToolButton>

QT_BEGIN_NAMESPACE

FindWidget::FindWidget(QWidget *parent)
    : QWidget(parent)
    , appPalette(qApp->palette())
{
    TRACE_OBJ
    installEventFilter(this);
    QHBoxLayout *hboxLayout = new QHBoxLayout(this);
    QString resourcePath = QLatin1String(":/qt-project.org/assistant/images/");

#ifndef Q_OS_MAC
    hboxLayout->setMargin(0);
    hboxLayout->setSpacing(6);
    resourcePath.append(QLatin1String("win"));
#else
    resourcePath.append(QLatin1String("mac"));
#endif

    toolClose = setupToolButton(QLatin1String(""),
        resourcePath + QLatin1String("/closetab.png"));
    hboxLayout->addWidget(toolClose);
    connect(toolClose, SIGNAL(clicked()), SLOT(hide()));

    editFind = new QLineEdit(this);
    hboxLayout->addWidget(editFind);
    editFind->setMinimumSize(QSize(150, 0));
    connect(editFind, SIGNAL(textChanged(QString)), this,
        SLOT(textChanged(QString)));
    connect(editFind, SIGNAL(returnPressed()), this, SIGNAL(findNext()));
    connect(editFind, SIGNAL(textChanged(QString)), this, SLOT(updateButtons()));

    toolPrevious = setupToolButton(tr("Previous"),
        resourcePath + QLatin1String("/previous.png"));
    connect(toolPrevious, SIGNAL(clicked()), this, SIGNAL(findPrevious()));

    hboxLayout->addWidget(toolPrevious);

    toolNext = setupToolButton(tr("Next"),
        resourcePath + QLatin1String("/next.png"));
    hboxLayout->addWidget(toolNext);
    connect(toolNext, SIGNAL(clicked()), this, SIGNAL(findNext()));

    checkCase = new QCheckBox(tr("Case Sensitive"), this);
    hboxLayout->addWidget(checkCase);

    labelWrapped = new QLabel(this);
    labelWrapped->setScaledContents(true);
    labelWrapped->setTextFormat(Qt::RichText);
    labelWrapped->setMinimumSize(QSize(0, 20));
    labelWrapped->setMaximumSize(QSize(105, 20));
    labelWrapped->setAlignment(Qt::AlignLeading | Qt::AlignLeft | Qt::AlignVCenter);
    labelWrapped->setText(tr("<img src=\":/qt-project.org/assistant/images/wrap.png\""
        ">&nbsp;Search wrapped"));
    hboxLayout->addWidget(labelWrapped);

    QSpacerItem *spacerItem = new QSpacerItem(20, 20, QSizePolicy::Expanding,
        QSizePolicy::Minimum);
    hboxLayout->addItem(spacerItem);
    setMinimumWidth(minimumSizeHint().width());
    labelWrapped->hide();

    updateButtons();
}

FindWidget::~FindWidget()
{
    TRACE_OBJ
}

void FindWidget::show()
{
    TRACE_OBJ
    QWidget::show();
    editFind->selectAll();
    editFind->setFocus(Qt::ShortcutFocusReason);
}

void FindWidget::showAndClear()
{
    TRACE_OBJ
    show();
    editFind->clear();
}

QString FindWidget::text() const
{
    TRACE_OBJ
    return editFind->text();
}

bool FindWidget::caseSensitive() const
{
    TRACE_OBJ
    return checkCase->isChecked();
}

void FindWidget::setPalette(bool found)
{
    TRACE_OBJ
    QPalette palette = editFind->palette();
    palette.setColor(QPalette::Active, QPalette::Base, found ? Qt::white
        : QColor(255, 102, 102));
    editFind->setPalette(palette);
}

void FindWidget::setTextWrappedVisible(bool visible)
{
    TRACE_OBJ
    labelWrapped->setVisible(visible);
}

void FindWidget::hideEvent(QHideEvent* event)
{
    TRACE_OBJ
#if defined(BROWSER_QTWEBKIT)
    // TODO: remove this once webkit supports setting the palette
    if (!event->spontaneous())
        qApp->setPalette(appPalette);
#else // BROWSER_QTWEBKIT
    Q_UNUSED(event);
#endif
}

void FindWidget::showEvent(QShowEvent* event)
{
    TRACE_OBJ
#if defined(BROWSER_QTWEBKIT)
    // TODO: remove this once webkit supports setting the palette
    if (!event->spontaneous()) {
        QPalette p = appPalette;
        p.setColor(QPalette::Inactive, QPalette::Highlight,
            p.color(QPalette::Active, QPalette::Highlight));
        p.setColor(QPalette::Inactive, QPalette::HighlightedText,
            p.color(QPalette::Active, QPalette::HighlightedText));
        qApp->setPalette(p);
    }
#else // BROWSER_QTWEBKIT
    Q_UNUSED(event);
#endif
}

void FindWidget::updateButtons()
{
    TRACE_OBJ
    const bool enable = !editFind->text().isEmpty();
    toolNext->setEnabled(enable);
    toolPrevious->setEnabled(enable);
}

void FindWidget::textChanged(const QString &text)
{
    TRACE_OBJ
    emit find(text, true, true);
}

bool FindWidget::eventFilter(QObject *object, QEvent *e)
{
    TRACE_OBJ
    if (e->type() == QEvent::KeyPress) {
        if ((static_cast<QKeyEvent*>(e))->key() == Qt::Key_Escape) {
            hide();
            emit escapePressed();
        }
    }
    return QWidget::eventFilter(object, e);
}

QToolButton* FindWidget::setupToolButton(const QString &text, const QString &icon)
{
    TRACE_OBJ
    QToolButton *toolButton = new QToolButton(this);

    toolButton->setText(text);
    toolButton->setAutoRaise(true);
    toolButton->setIcon(QIcon(icon));
    toolButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

    return toolButton;
}

QT_END_NAMESPACE
