import QtQml 2.0

QtObject {
    id: root

    property string tested
    signal testMe(int param1, string param2)

    property Connections c: Connections { target: root; onTestMe: root.tested = param2 + param1 }
}
