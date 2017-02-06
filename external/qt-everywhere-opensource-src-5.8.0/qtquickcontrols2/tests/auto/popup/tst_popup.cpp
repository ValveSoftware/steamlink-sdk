/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
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
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QtTest/qtest.h>
#include <QtTest/qsignalspy.h>
#include "../shared/util.h"
#include "../shared/visualtestutil.h"

#include <QtQuickTemplates2/private/qquickapplicationwindow_p.h>
#include <QtQuickTemplates2/private/qquickoverlay_p.h>
#include <QtQuickTemplates2/private/qquickpopup_p.h>
#include <QtQuickTemplates2/private/qquickbutton_p.h>
#include <QtQuickTemplates2/private/qquickslider_p.h>

using namespace QQuickVisualTestUtil;

class tst_popup : public QQmlDataTest
{
    Q_OBJECT

private slots:
    void visible_data();
    void visible();
    void state();
    void overlay_data();
    void overlay();
    void zOrder_data();
    void zOrder();
    void windowChange();
    void closePolicy_data();
    void closePolicy();
    void activeFocusOnClose1();
    void activeFocusOnClose2();
    void activeFocusOnClose3();
    void hover_data();
    void hover();
    void wheel_data();
    void wheel();
    void parentDestroyed();
    void nested();
    void grabber();
};

void tst_popup::visible_data()
{
    QTest::addColumn<QString>("source");
    QTest::newRow("Window") << "window.qml";
    QTest::newRow("ApplicationWindow") << "applicationwindow.qml";
}

void tst_popup::visible()
{
    QFETCH(QString, source);
    QQuickApplicationHelper helper(this, source);

    QQuickWindow *window = helper.window;
    window->show();
    window->requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(window));

    QQuickPopup *popup = window->property("popup").value<QQuickPopup*>();
    QVERIFY(popup);
    QQuickItem *popupItem = popup->popupItem();

    popup->open();
    QVERIFY(popup->isVisible());

    QQuickOverlay *overlay = QQuickOverlay::overlay(window);
    QVERIFY(overlay);
    QVERIFY(overlay->childItems().contains(popupItem));

    popup->close();
    QVERIFY(!popup->isVisible());
    QVERIFY(!overlay->childItems().contains(popupItem));

    popup->setVisible(true);
    QVERIFY(popup->isVisible());
    QVERIFY(overlay->childItems().contains(popupItem));

    popup->setVisible(false);
    QVERIFY(!popup->isVisible());
    QVERIFY(!overlay->childItems().contains(popupItem));
}

void tst_popup::state()
{
    QQuickApplicationHelper helper(this, "applicationwindow.qml");

    QQuickWindow *window = helper.window;
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window));

    QQuickPopup *popup = window->property("popup").value<QQuickPopup*>();
    QVERIFY(popup);

    QCOMPARE(popup->isVisible(), false);

    QSignalSpy visibleChangedSpy(popup, SIGNAL(visibleChanged()));
    QSignalSpy aboutToShowSpy(popup, SIGNAL(aboutToShow()));
    QSignalSpy aboutToHideSpy(popup, SIGNAL(aboutToHide()));
    QSignalSpy openedSpy(popup, SIGNAL(opened()));
    QSignalSpy closedSpy(popup, SIGNAL(closed()));

    QVERIFY(visibleChangedSpy.isValid());
    QVERIFY(aboutToShowSpy.isValid());
    QVERIFY(aboutToHideSpy.isValid());
    QVERIFY(openedSpy.isValid());
    QVERIFY(closedSpy.isValid());

    popup->open();
    QCOMPARE(visibleChangedSpy.count(), 1);
    QCOMPARE(aboutToShowSpy.count(), 1);
    QCOMPARE(aboutToHideSpy.count(), 0);
    QTRY_COMPARE(openedSpy.count(), 1);
    QCOMPARE(closedSpy.count(), 0);

    popup->close();
    QCOMPARE(visibleChangedSpy.count(), 2);
    QCOMPARE(aboutToShowSpy.count(), 1);
    QCOMPARE(aboutToHideSpy.count(), 1);
    QCOMPARE(openedSpy.count(), 1);
    QTRY_COMPARE(closedSpy.count(), 1);
}

void tst_popup::overlay_data()
{
    QTest::addColumn<QString>("source");
    QTest::newRow("Window") << "window.qml";
    QTest::newRow("ApplicationWindow") << "applicationwindow.qml";
}

void tst_popup::overlay()
{
    QFETCH(QString, source);
    QQuickApplicationHelper helper(this, source);

    QQuickWindow *window = helper.window;
    window->show();
    window->requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(window));

    QQuickOverlay *overlay = QQuickOverlay::overlay(window);
    QVERIFY(overlay);

    QSignalSpy overlayPressedSignal(overlay, SIGNAL(pressed()));
    QSignalSpy overlayReleasedSignal(overlay, SIGNAL(released()));
    QVERIFY(overlayPressedSignal.isValid());
    QVERIFY(overlayReleasedSignal.isValid());

    QVERIFY(!overlay->isVisible()); // no popups open

    QTest::mouseClick(window, Qt::LeftButton);
    QCOMPARE(overlayPressedSignal.count(), 0);
    QCOMPARE(overlayReleasedSignal.count(), 0);

    QQuickPopup *popup = window->property("popup").value<QQuickPopup*>();
    QVERIFY(popup);

    QQuickButton *button = window->property("button").value<QQuickButton*>();
    QVERIFY(button);

    popup->open();
    QVERIFY(popup->isVisible());
    QVERIFY(overlay->isVisible());

    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, QPoint(1, 1));
    QCOMPARE(overlayPressedSignal.count(), 1);
    QCOMPARE(overlayReleasedSignal.count(), 0);

    QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, QPoint(1, 1));
    QCOMPARE(overlayPressedSignal.count(), 1);
    QCOMPARE(overlayReleasedSignal.count(), 0); // no modal-popups open

    popup->close();
    QVERIFY(!popup->isVisible());
    QVERIFY(!overlay->isVisible());

    popup->setModal(true);
    popup->setClosePolicy(QQuickPopup::CloseOnReleaseOutside);

    popup->open();
    QVERIFY(popup->isVisible());
    QVERIFY(overlay->isVisible());

    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, QPoint(1, 1));
    QCOMPARE(overlayPressedSignal.count(), 2);
    QCOMPARE(overlayReleasedSignal.count(), 0);

    QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, QPoint(1, 1));
    QCOMPARE(overlayPressedSignal.count(), 2);
    QCOMPARE(overlayReleasedSignal.count(), 1);

    QVERIFY(!popup->isVisible());
    QVERIFY(!overlay->isVisible());
}

void tst_popup::zOrder_data()
{
    QTest::addColumn<QString>("source");
    QTest::newRow("Window") << "window.qml";
    QTest::newRow("ApplicationWindow") << "applicationwindow.qml";
}

void tst_popup::zOrder()
{
    QFETCH(QString, source);
    QQuickApplicationHelper helper(this, source);

    QQuickWindow *window = helper.window;
    window->show();
    window->requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(window));

    QQuickPopup *popup = window->property("popup").value<QQuickPopup*>();
    QVERIFY(popup);
    popup->setModal(true);

    QQuickPopup *popup2 = window->property("popup2").value<QQuickPopup*>();
    QVERIFY(popup2);
    popup2->setModal(true);

    // show popups in reverse order. popup2 has higher z-order so it appears
    // on top and must be closed first, even if the other popup was opened last
    popup2->open();
    popup->open();
    QVERIFY(popup2->isVisible());
    QVERIFY(popup->isVisible());

    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, QPoint(1, 1));
    QVERIFY(!popup2->isVisible());
    QVERIFY(popup->isVisible());

    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, QPoint(1, 1));
    QVERIFY(!popup2->isVisible());
    QVERIFY(!popup->isVisible());
}

void tst_popup::windowChange()
{
    QQuickPopup popup;
    QSignalSpy spy(&popup, SIGNAL(windowChanged(QQuickWindow*)));
    QVERIFY(spy.isValid());

    QQuickItem item;
    popup.setParentItem(&item);
    QVERIFY(!popup.window());
    QCOMPARE(spy.count(), 0);

    QQuickWindow window;
    item.setParentItem(window.contentItem());
    QCOMPARE(popup.window(), &window);
    QCOMPARE(spy.count(), 1);

    item.setParentItem(nullptr);
    QVERIFY(!popup.window());
    QCOMPARE(spy.count(), 2);

    popup.setParentItem(window.contentItem());
    QCOMPARE(popup.window(), &window);
    QCOMPARE(spy.count(), 3);
}

Q_DECLARE_METATYPE(QQuickPopup::ClosePolicy)

void tst_popup::closePolicy_data()
{
    qRegisterMetaType<QQuickPopup::ClosePolicy>();

    QTest::addColumn<QString>("source");
    QTest::addColumn<QQuickPopup::ClosePolicy>("closePolicy");

    QTest::newRow("Window:NoAutoClose") << "window.qml"<< static_cast<QQuickPopup::ClosePolicy>(QQuickPopup::NoAutoClose);
    QTest::newRow("Window:CloseOnPressOutside") << "window.qml"<< static_cast<QQuickPopup::ClosePolicy>(QQuickPopup::CloseOnPressOutside);
    QTest::newRow("Window:CloseOnPressOutsideParent") << "window.qml"<< static_cast<QQuickPopup::ClosePolicy>(QQuickPopup::CloseOnPressOutsideParent);
    QTest::newRow("Window:CloseOnPressOutside|Parent") << "window.qml"<< static_cast<QQuickPopup::ClosePolicy>(QQuickPopup::CloseOnPressOutside | QQuickPopup::CloseOnPressOutsideParent);
    QTest::newRow("Window:CloseOnReleaseOutside") << "window.qml"<< static_cast<QQuickPopup::ClosePolicy>(QQuickPopup::CloseOnReleaseOutside);
    QTest::newRow("Window:CloseOnReleaseOutside|Parent") << "window.qml"<< static_cast<QQuickPopup::ClosePolicy>(QQuickPopup::CloseOnReleaseOutside | QQuickPopup::CloseOnReleaseOutsideParent);
    QTest::newRow("Window:CloseOnEscape") << "window.qml"<< static_cast<QQuickPopup::ClosePolicy>(QQuickPopup::CloseOnEscape);

    QTest::newRow("ApplicationWindow:NoAutoClose") << "applicationwindow.qml"<< static_cast<QQuickPopup::ClosePolicy>(QQuickPopup::NoAutoClose);
    QTest::newRow("ApplicationWindow:CloseOnPressOutside") << "applicationwindow.qml"<< static_cast<QQuickPopup::ClosePolicy>(QQuickPopup::CloseOnPressOutside);
    QTest::newRow("ApplicationWindow:CloseOnPressOutsideParent") << "applicationwindow.qml"<< static_cast<QQuickPopup::ClosePolicy>(QQuickPopup::CloseOnPressOutsideParent);
    QTest::newRow("ApplicationWindow:CloseOnPressOutside|Parent") << "applicationwindow.qml"<< static_cast<QQuickPopup::ClosePolicy>(QQuickPopup::CloseOnPressOutside | QQuickPopup::CloseOnPressOutsideParent);
    QTest::newRow("ApplicationWindow:CloseOnReleaseOutside") << "applicationwindow.qml"<< static_cast<QQuickPopup::ClosePolicy>(QQuickPopup::CloseOnReleaseOutside);
    QTest::newRow("ApplicationWindow:CloseOnReleaseOutside|Parent") << "applicationwindow.qml"<< static_cast<QQuickPopup::ClosePolicy>(QQuickPopup::CloseOnReleaseOutside | QQuickPopup::CloseOnReleaseOutsideParent);
    QTest::newRow("ApplicationWindow:CloseOnEscape") << "applicationwindow.qml"<< static_cast<QQuickPopup::ClosePolicy>(QQuickPopup::CloseOnEscape);
}

void tst_popup::closePolicy()
{
    QFETCH(QString, source);
    QFETCH(QQuickPopup::ClosePolicy, closePolicy);

    QQuickApplicationHelper helper(this, source);

    QQuickWindow *window = helper.window;
    window->show();
    window->requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(window));

    QQuickPopup *popup = window->property("popup").value<QQuickPopup*>();
    QVERIFY(popup);

    QQuickButton *button = window->property("button").value<QQuickButton*>();
    QVERIFY(button);

    popup->setModal(true);
    popup->setFocus(true);
    popup->setClosePolicy(closePolicy);

    popup->open();
    QVERIFY(popup->isVisible());

    // press outside popup and its parent
    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, QPoint(1, 1));
    if (closePolicy.testFlag(QQuickPopup::CloseOnPressOutside) || closePolicy.testFlag(QQuickPopup::CloseOnPressOutsideParent))
        QVERIFY(!popup->isVisible());
    else
        QVERIFY(popup->isVisible());

    popup->open();
    QVERIFY(popup->isVisible());

    // release outside popup and its parent
    QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, QPoint(1, 1));
    if (closePolicy.testFlag(QQuickPopup::CloseOnReleaseOutside))
        QVERIFY(!popup->isVisible());
    else
        QVERIFY(popup->isVisible());

    popup->open();
    QVERIFY(popup->isVisible());

    // press outside popup but inside its parent
    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, QPoint(button->x(), button->y()));
    if (closePolicy.testFlag(QQuickPopup::CloseOnPressOutside) && !closePolicy.testFlag(QQuickPopup::CloseOnPressOutsideParent))
        QVERIFY(!popup->isVisible());
    else
        QVERIFY(popup->isVisible());

    popup->open();
    QVERIFY(popup->isVisible());

    // release outside popup but inside its parent
    QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, QPoint(button->x(), button->y()));
    if (closePolicy.testFlag(QQuickPopup::CloseOnReleaseOutside) && !closePolicy.testFlag(QQuickPopup::CloseOnReleaseOutsideParent))
        QVERIFY(!popup->isVisible());
    else
        QVERIFY(popup->isVisible());

    popup->open();
    QVERIFY(popup->isVisible());

    // escape
    QTest::keyClick(window, Qt::Key_Escape);
    if (closePolicy.testFlag(QQuickPopup::CloseOnEscape))
        QVERIFY(!popup->isVisible());
    else
        QVERIFY(popup->isVisible());
}

void tst_popup::activeFocusOnClose1()
{
    // Test that a popup that never sets focus: true (e.g. ToolTip) doesn't affect
    // the active focus item when it closes.
    QQuickApplicationHelper helper(this, QStringLiteral("activeFocusOnClose1.qml"));
    QQuickApplicationWindow *window = helper.appWindow;
    window->show();
    window->requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(window));

    QQuickPopup *focusedPopup = helper.appWindow->property("focusedPopup").value<QQuickPopup*>();
    QVERIFY(focusedPopup);

    QQuickPopup *nonFocusedPopup = helper.appWindow->property("nonFocusedPopup").value<QQuickPopup*>();
    QVERIFY(nonFocusedPopup);

    focusedPopup->open();
    QVERIFY(focusedPopup->isVisible());
    QVERIFY(focusedPopup->hasActiveFocus());

    nonFocusedPopup->open();
    QVERIFY(nonFocusedPopup->isVisible());
    QVERIFY(focusedPopup->hasActiveFocus());

    nonFocusedPopup->close();
    QVERIFY(!nonFocusedPopup->isVisible());
    QVERIFY(focusedPopup->hasActiveFocus());
}

void tst_popup::activeFocusOnClose2()
{
    // Test that a popup that sets focus: true but relinquishes focus (e.g. by
    // calling forceActiveFocus() on another item) before it closes doesn't
    // affect the active focus item when it closes.
    QQuickApplicationHelper helper(this, QStringLiteral("activeFocusOnClose2.qml"));
    QQuickApplicationWindow *window = helper.appWindow;
    window->show();
    window->requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(window));

    QQuickPopup *popup1 = helper.appWindow->property("popup1").value<QQuickPopup*>();
    QVERIFY(popup1);

    QQuickPopup *popup2 = helper.appWindow->property("popup2").value<QQuickPopup*>();
    QVERIFY(popup2);

    QQuickButton *closePopup2Button = helper.appWindow->property("closePopup2Button").value<QQuickButton*>();
    QVERIFY(closePopup2Button);

    popup1->open();
    QVERIFY(popup1->isVisible());
    QVERIFY(popup1->hasActiveFocus());

    popup2->open();
    QVERIFY(popup2->isVisible());
    QVERIFY(popup2->hasActiveFocus());

    // Causes popup1.contentItem.forceActiveFocus() to be called, then closes popup2.
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier,
        closePopup2Button->mapToScene(QPointF(closePopup2Button->width() / 2, closePopup2Button->height() / 2)).toPoint());
    QVERIFY(!popup2->isVisible());
    QVERIFY(popup1->hasActiveFocus());
}

void tst_popup::activeFocusOnClose3()
{
    // Test that a closing popup that had focus doesn't steal focus from
    // another popup that the focus was transferred to.
    QQuickApplicationHelper helper(this, QStringLiteral("activeFocusOnClose3.qml"));
    QQuickApplicationWindow *window = helper.appWindow;
    window->show();
    window->requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(window));

    QQuickPopup *popup1 = helper.appWindow->property("popup1").value<QQuickPopup*>();
    QVERIFY(popup1);

    QQuickPopup *popup2 = helper.appWindow->property("popup2").value<QQuickPopup*>();
    QVERIFY(popup2);

    popup1->open();
    QVERIFY(popup1->isVisible());
    QTRY_VERIFY(popup1->hasActiveFocus());

    popup2->open();
    popup1->close();

    QSignalSpy closedSpy(popup1, SIGNAL(closed()));
    QVERIFY(closedSpy.isValid());
    QVERIFY(closedSpy.wait());

    QVERIFY(!popup1->isVisible());
    QVERIFY(popup2->isVisible());
    QTRY_VERIFY(popup2->hasActiveFocus());
}

void tst_popup::hover_data()
{
    QTest::addColumn<QString>("source");
    QTest::addColumn<bool>("modal");

    QTest::newRow("Window:modal") << "window-hover.qml" << true;
    QTest::newRow("Window:modeless") << "window-hover.qml" << false;
    QTest::newRow("ApplicationWindow:modal") << "applicationwindow-hover.qml" << true;
    QTest::newRow("ApplicationWindow:modeless") << "applicationwindow-hover.qml" << false;
}

void tst_popup::hover()
{
    QFETCH(QString, source);
    QFETCH(bool, modal);

    QQuickApplicationHelper helper(this, source);
    QQuickWindow *window = helper.window;
    window->show();
    window->requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(window));

    QQuickPopup *popup = window->property("popup").value<QQuickPopup*>();
    QVERIFY(popup);
    popup->setModal(modal);

    QQuickButton *parentButton = window->property("parentButton").value<QQuickButton*>();
    QVERIFY(parentButton);
    parentButton->setHoverEnabled(true);

    QQuickButton *childButton = window->property("childButton").value<QQuickButton*>();
    QVERIFY(childButton);
    childButton->setHoverEnabled(true);

    QSignalSpy openedSpy(popup, SIGNAL(opened()));
    QVERIFY(openedSpy.isValid());
    popup->open();
    QVERIFY(openedSpy.count() == 1 || openedSpy.wait());

    // hover the parent button outside the popup
    QTest::mouseMove(window, QPoint(window->width() - 1, window->height() - 1));
    QCOMPARE(parentButton->isHovered(), !modal);
    QVERIFY(!childButton->isHovered());

    // hover the popup background
    QTest::mouseMove(window, QPoint(1, 1));
    QVERIFY(!parentButton->isHovered());
    QVERIFY(!childButton->isHovered());

    // hover the child button in a popup
    QTest::mouseMove(window, QPoint(2, 2));
    QVERIFY(!parentButton->isHovered());
    QVERIFY(childButton->isHovered());

    QSignalSpy closedSpy(popup, SIGNAL(closed()));
    QVERIFY(closedSpy.isValid());
    popup->close();
    QVERIFY(closedSpy.count() == 1 || closedSpy.wait());

    // hover the parent button after closing the popup
    QTest::mouseMove(window, QPoint(window->width() / 2, window->height() / 2));
    QVERIFY(parentButton->isHovered());
}

void tst_popup::wheel_data()
{
    QTest::addColumn<QString>("source");
    QTest::addColumn<bool>("modal");

    QTest::newRow("Window:modal") << "window-wheel.qml" << true;
    QTest::newRow("Window:modeless") << "window-wheel.qml" << false;
    QTest::newRow("ApplicationWindow:modal") << "applicationwindow-wheel.qml" << true;
    QTest::newRow("ApplicationWindow:modeless") << "applicationwindow-wheel.qml" << false;
}

static bool sendWheelEvent(QQuickItem *item, const QPoint &localPos, int degrees)
{
    QQuickWindow *window = item->window();
    QWheelEvent wheelEvent(localPos, item->window()->mapToGlobal(localPos), QPoint(0, 0), QPoint(0, 8 * degrees), 0, Qt::Vertical, Qt::NoButton, 0);
    QSpontaneKeyEvent::setSpontaneous(&wheelEvent);
    return qGuiApp->notify(window, &wheelEvent);
}

void tst_popup::wheel()
{
    QFETCH(QString, source);
    QFETCH(bool, modal);

    QQuickApplicationHelper helper(this, source);
    QQuickWindow *window = helper.window;
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window));

    QQuickSlider *contentSlider = window->property("contentSlider").value<QQuickSlider*>();
    QVERIFY(contentSlider);

    QQuickPopup *popup = window->property("popup").value<QQuickPopup*>();
    QVERIFY(popup && popup->contentItem());
    popup->setModal(modal);

    QQuickSlider *popupSlider = window->property("popupSlider").value<QQuickSlider*>();
    QVERIFY(popupSlider);

    {
        // wheel over the content
        qreal oldContentValue = contentSlider->value();
        qreal oldPopupValue = popupSlider->value();

        QVERIFY(sendWheelEvent(contentSlider, QPoint(contentSlider->width() / 2, contentSlider->height() / 2), 15));

        QVERIFY(!qFuzzyCompare(contentSlider->value(), oldContentValue)); // must have moved
        QVERIFY(qFuzzyCompare(popupSlider->value(), oldPopupValue)); // must not have moved
    }

    QSignalSpy openedSpy(popup, SIGNAL(opened()));
    QVERIFY(openedSpy.isValid());
    popup->open();
    QVERIFY(openedSpy.count() == 1 || openedSpy.wait());

    {
        // wheel over the popup content
        qreal oldContentValue = contentSlider->value();
        qreal oldPopupValue = popupSlider->value();

        QVERIFY(sendWheelEvent(popupSlider, QPoint(popupSlider->width() / 2, popupSlider->height() / 2), 15));

        QVERIFY(qFuzzyCompare(contentSlider->value(), oldContentValue)); // must not have moved
        QVERIFY(!qFuzzyCompare(popupSlider->value(), oldPopupValue)); // must have moved
    }

    {
        // wheel over the overlay
        qreal oldContentValue = contentSlider->value();
        qreal oldPopupValue = popupSlider->value();

        QVERIFY(sendWheelEvent(QQuickOverlay::overlay(window), QPoint(0, 0), 15));

        if (modal) {
            // the content below a modal overlay must not move
            QVERIFY(qFuzzyCompare(contentSlider->value(), oldContentValue));
        } else {
            // the content below a modeless overlay must move
            QVERIFY(!qFuzzyCompare(contentSlider->value(), oldContentValue));
        }
        QVERIFY(qFuzzyCompare(popupSlider->value(), oldPopupValue)); // must not have moved
    }
}

void tst_popup::parentDestroyed()
{
    QQuickPopup popup;
    popup.setParentItem(new QQuickItem);
    delete popup.parentItem();
    QVERIFY(!popup.parentItem());
}

void tst_popup::nested()
{
    QQuickApplicationHelper helper(this, QStringLiteral("nested.qml"));
    QQuickWindow *window = helper.window;
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window));

    QQuickPopup *modalPopup = window->property("modalPopup").value<QQuickPopup *>();
    QVERIFY(modalPopup);

    QQuickPopup *modelessPopup = window->property("modelessPopup").value<QQuickPopup *>();
    QVERIFY(modelessPopup);

    modalPopup->open();
    QCOMPARE(modalPopup->isVisible(), true);

    modelessPopup->open();
    QCOMPARE(modelessPopup->isVisible(), true);

    // click outside the modeless popup on the top, but inside the modal popup below
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, QPoint(150, 150));

    QTRY_COMPARE(modelessPopup->isVisible(), false);
    QCOMPARE(modalPopup->isVisible(), true);
}

// QTBUG-56697
void tst_popup::grabber()
{
    QQuickApplicationHelper helper(this, QStringLiteral("grabber.qml"));
    QQuickWindow *window = helper.window;
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window));

    QQuickPopup *menu = window->property("menu").value<QQuickPopup *>();
    QVERIFY(menu);

    QQuickPopup *popup = window->property("popup").value<QQuickPopup *>();
    QVERIFY(popup);

    QQuickPopup *combo = window->property("combo").value<QQuickPopup *>();
    QVERIFY(combo);

    menu->open();
    QCOMPARE(menu->isVisible(), true);
    QCOMPARE(popup->isVisible(), false);
    QCOMPARE(combo->isVisible(), false);

    // click a menu item to open the popup
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, QPoint(menu->width() / 2, menu->height() / 2));
    QCOMPARE(menu->isVisible(), false);
    QCOMPARE(popup->isVisible(), true);
    QCOMPARE(combo->isVisible(), false);

    combo->open();
    QCOMPARE(menu->isVisible(), false);
    QCOMPARE(popup->isVisible(), true);
    QCOMPARE(combo->isVisible(), true);

    // click outside to close both the combo popup and the parent popup
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, QPoint(window->width() - 1, window->height() - 1));
    QCOMPARE(menu->isVisible(), false);
    QCOMPARE(popup->isVisible(), false);
    QCOMPARE(combo->isVisible(), false);

    menu->open();
    QCOMPARE(menu->isVisible(), true);
    QCOMPARE(popup->isVisible(), false);
    QCOMPARE(combo->isVisible(), false);

    // click outside the menu to close it (QTBUG-56697)
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, QPoint(window->width() - 1, window->height() - 1));
    QCOMPARE(menu->isVisible(), false);
    QCOMPARE(popup->isVisible(), false);
    QCOMPARE(combo->isVisible(), false);
}

QTEST_MAIN(tst_popup)

#include "tst_popup.moc"
