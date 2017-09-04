CONFIG += testcase
TARGET = tst_qquickanimations
SOURCES += tst_qquickanimations.cpp

include (../../shared/util.pri)

macx:CONFIG -= app_bundle

TESTDATA = data/*

QT += core-private gui-private  qml-private quick-private testlib
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0

OTHER_FILES += \
    data/attached.qml \
    data/badproperty1.qml \
    data/badproperty2.qml \
    data/badtype1.qml \
    data/badtype2.qml \
    data/badtype3.qml \
    data/badtype4.qml \
    data/disabledTransition.qml \
    data/dontAutoStart.qml \
    data/dontStart.qml \
    data/dontStart2.qml \
    data/dotproperty.qml \
    data/Double.qml \
    data/doubleRegistrationBug.qml \
    data/looping.qml \
    data/mixedtype1.qml \
    data/mixedtype2.qml \
    data/nonTransitionBug.qml \
    data/parallelAnimationNullChildBug.qml \
    data/pathAnimation.qml \
    data/pathAnimation2.qml \
    data/pathAnimationInOutBackCrash.qml \
    data/pathAnimationNoStart.qml \
    data/pathInterpolator.qml \
    data/pathInterpolatorBack.qml \
    data/pathInterpolatorBack2.qml \
    data/pathTransition.qml \
    data/pauseBindingBug.qml \
    data/pauseBug.qml \
    data/properties.qml \
    data/properties2.qml \
    data/properties3.qml \
    data/properties4.qml \
    data/properties5.qml \
    data/propertiesTransition.qml \
    data/propertiesTransition2.qml \
    data/propertiesTransition3.qml \
    data/propertiesTransition4.qml \
    data/propertiesTransition5.qml \
    data/propertiesTransition6.qml \
    data/propertiesTransition7.qml \
    data/reanchor.qml \
    data/registrationBug.qml \
    data/reparent.qml \
    data/rotation.qml \
    data/runningTrueBug.qml \
    data/scriptActionBug.qml \
    data/scriptActionCrash.qml \
    data/sequentialAnimationNullChildBug.qml \
    data/signals.qml \
    data/transitionAssignmentBug.qml \
    data/valuesource.qml \
    data/valuesource2.qml
