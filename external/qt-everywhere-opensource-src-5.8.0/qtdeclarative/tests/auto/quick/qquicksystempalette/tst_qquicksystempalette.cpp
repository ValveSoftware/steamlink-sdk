/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
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

#include <qtest.h>
#include <QDebug>
#include <QtQml/qqmlengine.h>
#include <QtQml/qqmlcomponent.h>
#include <QtQuick/private/qquicksystempalette_p.h>
#include <qpalette.h>

class tst_qquicksystempalette : public QObject
{
    Q_OBJECT
public:
    tst_qquicksystempalette();

private slots:
    void activePalette();
    void inactivePalette();
    void disabledPalette();
#ifndef QT_NO_WIDGETS
    void paletteChanged();
#endif

private:
    QQmlEngine engine;
};

tst_qquicksystempalette::tst_qquicksystempalette()
{
}

void tst_qquicksystempalette::activePalette()
{
    QString componentStr = "import QtQuick 2.0\nSystemPalette { }";
    QQmlComponent component(&engine);
    component.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
    QQuickSystemPalette *object = qobject_cast<QQuickSystemPalette*>(component.create());

    QVERIFY(object != 0);

    QPalette palette;
    palette.setCurrentColorGroup(QPalette::Active);
    QCOMPARE(palette.window().color(), object->window());
    QCOMPARE(palette.windowText().color(), object->windowText());
    QCOMPARE(palette.base().color(), object->base());
    QCOMPARE(palette.text().color(), object->text());
    QCOMPARE(palette.alternateBase().color(), object->alternateBase());
    QCOMPARE(palette.button().color(), object->button());
    QCOMPARE(palette.buttonText().color(), object->buttonText());
    QCOMPARE(palette.light().color(), object->light());
    QCOMPARE(palette.midlight().color(), object->midlight());
    QCOMPARE(palette.dark().color(), object->dark());
    QCOMPARE(palette.mid().color(), object->mid());
    QCOMPARE(palette.shadow().color(), object->shadow());
    QCOMPARE(palette.highlight().color(), object->highlight());
    QCOMPARE(palette.highlightedText().color(), object->highlightedText());

    delete object;
}

void tst_qquicksystempalette::inactivePalette()
{
    QString componentStr = "import QtQuick 2.0\nSystemPalette { colorGroup: SystemPalette.Inactive }";
    QQmlComponent component(&engine);
    component.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
    QQuickSystemPalette *object = qobject_cast<QQuickSystemPalette*>(component.create());

    QVERIFY(object != 0);
    QCOMPARE(object->colorGroup(), QQuickSystemPalette::Inactive);

    QPalette palette;
    palette.setCurrentColorGroup(QPalette::Inactive);
    QCOMPARE(palette.window().color(), object->window());
    QCOMPARE(palette.windowText().color(), object->windowText());
    QCOMPARE(palette.base().color(), object->base());
    QCOMPARE(palette.text().color(), object->text());
    QCOMPARE(palette.alternateBase().color(), object->alternateBase());
    QCOMPARE(palette.button().color(), object->button());
    QCOMPARE(palette.buttonText().color(), object->buttonText());
    QCOMPARE(palette.light().color(), object->light());
    QCOMPARE(palette.midlight().color(), object->midlight());
    QCOMPARE(palette.dark().color(), object->dark());
    QCOMPARE(palette.mid().color(), object->mid());
    QCOMPARE(palette.shadow().color(), object->shadow());
    QCOMPARE(palette.highlight().color(), object->highlight());
    QCOMPARE(palette.highlightedText().color(), object->highlightedText());

    delete object;
}

void tst_qquicksystempalette::disabledPalette()
{
    QString componentStr = "import QtQuick 2.0\nSystemPalette { colorGroup: SystemPalette.Disabled }";
    QQmlComponent component(&engine);
    component.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
    QQuickSystemPalette *object = qobject_cast<QQuickSystemPalette*>(component.create());

    QVERIFY(object != 0);
    QCOMPARE(object->colorGroup(), QQuickSystemPalette::Disabled);

    QPalette palette;
    palette.setCurrentColorGroup(QPalette::Disabled);
    QCOMPARE(palette.window().color(), object->window());
    QCOMPARE(palette.windowText().color(), object->windowText());
    QCOMPARE(palette.base().color(), object->base());
    QCOMPARE(palette.text().color(), object->text());
    QCOMPARE(palette.alternateBase().color(), object->alternateBase());
    QCOMPARE(palette.button().color(), object->button());
    QCOMPARE(palette.buttonText().color(), object->buttonText());
    QCOMPARE(palette.light().color(), object->light());
    QCOMPARE(palette.midlight().color(), object->midlight());
    QCOMPARE(palette.dark().color(), object->dark());
    QCOMPARE(palette.mid().color(), object->mid());
    QCOMPARE(palette.shadow().color(), object->shadow());
    QCOMPARE(palette.highlight().color(), object->highlight());
    QCOMPARE(palette.highlightedText().color(), object->highlightedText());

    delete object;
}

#ifndef QT_NO_WIDGETS
void tst_qquicksystempalette::paletteChanged()
{
    QString componentStr = "import QtQuick 2.0\nSystemPalette { }";
    QQmlComponent component(&engine);
    component.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
    QQuickSystemPalette *object = qobject_cast<QQuickSystemPalette*>(component.create());

    QVERIFY(object != 0);

    QPalette p;
    p.setCurrentColorGroup(QPalette::Active);
    p.setColor(QPalette::Active, QPalette::Text, QColor("red"));
    p.setColor(QPalette::Active, QPalette::ButtonText, QColor("green"));
    p.setColor(QPalette::Active, QPalette::WindowText, QColor("blue"));

    qApp->setPalette(p);

    object->setColorGroup(QQuickSystemPalette::Active);
    QTRY_COMPARE(QColor("red"), object->text());
    QTRY_COMPARE(QColor("green"), object->buttonText());
    QTRY_COMPARE(QColor("blue"), object->windowText());

    delete object;
}
#endif

QTEST_MAIN(tst_qquicksystempalette)

#include "tst_qquicksystempalette.moc"
