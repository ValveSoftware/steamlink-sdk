/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtWebEngine module of the Qt Toolkit.
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

#include <QtTest/QtTest>

#include <qdir.h>
#if defined(QWEBENGINEINSPECTOR)
#include <qwebengineinspector.h>
#endif
#include <qwebenginepage.h>
#if defined(QWEBENGINESETTINGS)
#include <qwebenginesettings.h>
#endif

class tst_QWebEngineInspector : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void attachAndDestroyPageFirst();
    void attachAndDestroyInspectorFirst();
    void attachAndDestroyInternalInspector();
};

void tst_QWebEngineInspector::attachAndDestroyPageFirst()
{
#if !defined(QWEBENGINEINSPECTOR)
    QSKIP("QWEBENGINEINSPECTOR");
#else
    // External inspector + manual destruction of page first
    QWebEnginePage* page = new QWebEnginePage();
    page->settings()->setAttribute(QWebEngineSettings::DeveloperExtrasEnabled, true);
    QWebEngineInspector* inspector = new QWebEngineInspector();
    inspector->setPage(page);
    page->updatePositionDependentActions(QPoint(0, 0));
    page->triggerAction(QWebEnginePage::InspectElement);

    delete page;
    delete inspector;
#endif
}

void tst_QWebEngineInspector::attachAndDestroyInspectorFirst()
{
#if !defined(QWEBENGINEINSPECTOR)
    QSKIP("QWEBENGINEINSPECTOR");
#else
    // External inspector + manual destruction of inspector first
    QWebEnginePage* page = new QWebEnginePage();
    page->settings()->setAttribute(QWebEngineSettings::DeveloperExtrasEnabled, true);
    QWebEngineInspector* inspector = new QWebEngineInspector();
    inspector->setPage(page);
    page->updatePositionDependentActions(QPoint(0, 0));
    page->triggerAction(QWebEnginePage::InspectElement);

    delete inspector;
    delete page;
#endif
}

void tst_QWebEngineInspector::attachAndDestroyInternalInspector()
{
#if !defined(QWEBENGINEINSPECTOR)
    QSKIP("QWEBENGINEINSPECTOR");
#else
    // Internal inspector
    QWebEnginePage page;
    page.settings()->setAttribute(QWebEngineSettings::DeveloperExtrasEnabled, true);
    page.updatePositionDependentActions(QPoint(0, 0));
    page.triggerAction(QWebEnginePage::InspectElement);
#endif
}

QTEST_MAIN(tst_QWebEngineInspector)

#include "tst_qwebengineinspector.moc"
