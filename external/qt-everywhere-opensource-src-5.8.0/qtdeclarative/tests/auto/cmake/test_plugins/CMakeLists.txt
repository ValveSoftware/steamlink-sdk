project(test_plugins)

cmake_minimum_required(VERSION 2.8)
if (POLICY CMP0056)
    cmake_policy(SET CMP0056 NEW)
endif()

find_package(Qt5Qml REQUIRED)

# See QTBUG-43438
if (NOT TARGET Qt5::QTcpServerConnectionFactory)
    message(SEND_ERROR "Qt5::QTcpServerConnectionFactory does not exist")
endif()
if (NOT TARGET Qt5::QLocalClientConnectionFactory)
    message(SEND_ERROR "Qt5::QLocalClientConnectionFactory does not exist")
endif()
