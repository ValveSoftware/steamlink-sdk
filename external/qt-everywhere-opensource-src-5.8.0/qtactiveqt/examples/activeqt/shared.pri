# This .pri file suppresses the registration of the examples in the
# Qt Continuous Integration infrastructure.
QT_CI_JENKINS_HOME=$$(JENKINS_HOME)
!isEmpty(QT_CI_JENKINS_HOME) {
    message("Qt CI environment detected, suppressing example registration")
    CONFIG += qaxserver_no_postlink
}
