Qt Quick Controls 2
===================

The Qt Quick Controls 2 module delivers the next generation user interface
controls based on Qt Quick. In comparison to the desktop-oriented Qt Quick
Controls 1, Qt Quick Controls 2 are an order of magnitude simpler, lighter and
faster, and are primarily targeted towards embedded and mobile platforms.

Qt Quick Controls 2 are based on a flexible template system that enables rapid
development of entire custom styles and user experiences. Qt Quick Controls 2
comes with a selection of built-in styles:

- Default style - a simple and minimal all-round style that offers the maximum performance
- Material style - a style based on the Google Material Design Guidelines
- Universal style - a style based on the Microsoft Universal Design Guidelines

More information can be found in the following blog posts:

- http://blog.qt.io/blog/2015/03/31/qt-quick-controls-for-embedded/
- http://blog.qt.io/blog/2015/11/23/qt-quick-controls-re-engineered-status-update/

## Help

If you have problems or questions, don't hesitate to:

- ask on the Qt Interest mailing list http://lists.qt-project.org/mailman/listinfo/interest
- ask on the Qt Forum http://forum.qt.io/category/12/qt-quick
- report issues to the Qt Bug Tracker https://bugreports.qt.io (component: *Qt Quick: Controls 2*)

## Installation

The MINIMUM REQUIREMENT for building this project is to use the same branch
of Qt 5. The dependencies are *qtbase*, *qtxmlpatterns* and *qtdeclarative*.

To install the controls into your Qt directory (```QTDIR/qml```):

    qmake
    make
    make install

If you are compiling against a system Qt on Linux, you might have to use
```sudo make install``` to install the project.

## Usage

Please refer to the "Getting Started with Qt Quick Controls 2" documentation.
