/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the examples of the Qt Toolkit.
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

#include "BrowserWindow.h"

#include <QtCore>
#include <QtWidgets>
#include <QPropertyAnimation>
#include <QResizeEvent>

#include "BrowserView.h"
#include "HomeView.h"

BrowserWindow::BrowserWindow()
    : m_slidingSurface(new QWidget(this))
    , m_homeView(new HomeView(m_slidingSurface))
    , m_browserView(new BrowserView(m_slidingSurface))
    , m_animation(new QPropertyAnimation(this, "slideValue"))
{
    m_slidingSurface->setAutoFillBackground(true);

    m_homeView->resize(size());

    m_browserView->resize(size());

    connect(m_homeView, SIGNAL(addressEntered(QString)), SLOT(gotoAddress(QString)));
    connect(m_homeView, SIGNAL(urlActivated(QUrl)), SLOT(navigate(QUrl)));

    connect(m_browserView, SIGNAL(menuButtonClicked()), SLOT(showHomeView()));

    m_animation->setDuration(200);
    connect(m_animation, SIGNAL(finished()), SLOT(animationFinished()));

    setSlideValue(0.0f);
}

void BrowserWindow::gotoAddress(const QString &address)
{
    m_browserView->navigate(QUrl::fromUserInput(address));
    showBrowserView();
}

void BrowserWindow::animationFinished()
{
    m_animation->setDirection(QAbstractAnimation::Forward);
}

void BrowserWindow::navigate(const QUrl &url)
{
    m_browserView->navigate(url);
    showBrowserView();
}

void BrowserWindow::setSlideValue(qreal slideRatio)
{
    // we use a ratio to handle resize corectly
    const int pos = -qRound(slideRatio * width());
    m_slidingSurface->scroll(pos - m_homeView->x(), 0);

    if (qFuzzyCompare(slideRatio, static_cast<qreal>(1.0f))) {
        m_browserView->show();
        m_homeView->hide();
    } else if (qFuzzyCompare(slideRatio, static_cast<qreal>(0.0f))) {
        m_homeView->show();
        m_browserView->hide();
    } else {
        m_browserView->show();
        m_homeView->show();
    }
}

qreal BrowserWindow::slideValue() const
{
    Q_ASSERT(m_slidingSurface->x() < width());
    return static_cast<qreal>(qAbs(m_homeView->x())) / width();
}

void BrowserWindow::showHomeView()
{
    m_animation->setStartValue(slideValue());
    m_animation->setEndValue(0.0f);
    m_animation->start();
    m_homeView->setFocus();
}

void BrowserWindow::showBrowserView()
{
    m_animation->setStartValue(slideValue());
    m_animation->setEndValue(1.0f);
    m_animation->start();

    m_browserView->setFocus();
}

void BrowserWindow::keyReleaseEvent(QKeyEvent *event)
{
    QWidget::keyReleaseEvent(event);

    if (event->key() == Qt::Key_F3) {
        if (m_animation->state() == QAbstractAnimation::Running) {
            const QAbstractAnimation::Direction direction =  m_animation->direction() == QAbstractAnimation::Forward
                                                             ? QAbstractAnimation::Forward
                                                                 : QAbstractAnimation::Backward;
            m_animation->setDirection(direction);
        } else if (qFuzzyCompare(slideValue(), static_cast<qreal>(1.0f)))
            showHomeView();
        else
            showBrowserView();
        event->accept();
    }
}

void BrowserWindow::resizeEvent(QResizeEvent *event)
{
    const QSize oldSize = event->oldSize();
    const qreal oldSlidingRatio = static_cast<qreal>(qAbs(m_homeView->x())) / oldSize.width();

    const QSize newSize = event->size();
    m_slidingSurface->resize(newSize.width() * 2, newSize.height());

    m_homeView->resize(newSize);
    m_homeView->move(0, 0);

    m_browserView->resize(newSize);
    m_browserView->move(newSize.width(), 0);

    setSlideValue(oldSlidingRatio);
}
