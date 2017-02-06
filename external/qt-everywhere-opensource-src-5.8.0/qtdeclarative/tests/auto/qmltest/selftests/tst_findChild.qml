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

import QtQuick 2.0
import QtTest 1.1

TestCase {
    name: "tst_findChild"

    QtObject {
        id: singleObject
    }

    Item {
        id: oneObjectChildItem

        QtObject {
            id: childObject
            objectName: "childObject"
        }
    }

    Item {
        id: oneItemChildItem

        Item {
            id: childItem
            objectName: "childItem"
        }
    }

    Item {
        id: nestedChildrenItem

        Item {
            id: nestedChildItem0
            objectName: "nestedChildItem0"

            Item {
                id: nestedChildItem1
                objectName: "nestedChildItem1"

                Item {
                    id: nestedChildItem2
                    objectName: "nestedChildItem2"
                }
            }
        }
    }

    property Component duplicateNestedChildItem2Component: Item {
        objectName: "nestedChildItem2"
    }

    Loader {
        id: loader

        sourceComponent: Item {
            id: loaderItem
            objectName: "loaderItem"

            Item {
                objectName: "nestedLoaderItem"
            }

            Repeater {
                model: 5
                delegate: Item {
                    objectName: "repeaterItem" + index
                }
            }

            ListView {
                model: 5
                delegate: Item {
                    objectName: "listViewItem" + index
                }
            }
        }
    }

    function test_findChild() {
        compare(findChild(null, ""), null);
        compare(findChild(undefined, ""), null);
        compare(findChild(singleObject, "doesNotExist"), null);
        compare(findChild(oneObjectChildItem, "childObject"), childObject);
        compare(findChild(oneItemChildItem, "childItem"), childItem);
        compare(findChild(nestedChildrenItem, "nestedChildItem0"), nestedChildItem0);
        compare(findChild(nestedChildrenItem, "nestedChildItem1"), nestedChildItem1);
        compare(findChild(nestedChildrenItem, "nestedChildItem2"), nestedChildItem2);

        // Shouldn't be found, since it's not the first in the list.
        duplicateNestedChildItem2Component.createObject(nestedChildItem1);
        compare(nestedChildItem1.children.length, 2);
        compare(findChild(nestedChildrenItem, "nestedChildItem2"), nestedChildItem2);

        var mostDirectChild = duplicateNestedChildItem2Component.createObject(nestedChildItem0);
        compare(nestedChildItem0.children.length, 2);
        compare(findChild(nestedChildrenItem, "nestedChildItem2"), mostDirectChild,
            "Dynamically created nested child items are found");

        compare(findChild(loader, "loaderItem"), loader.item);
        verify(findChild(loader, "nestedLoaderItem"));

        // These don't make their delegate items QObject children, only visual.
        verify(findChild(loader, "repeaterItem0"));
        verify(findChild(loader, "listViewItem0"));
    }
}
