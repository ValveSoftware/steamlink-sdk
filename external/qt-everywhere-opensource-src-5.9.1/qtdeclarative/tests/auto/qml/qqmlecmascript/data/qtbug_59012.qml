import QtQuick 2.0

QtObject {
    Component.onCompleted: {
        var pieces = [[4,21],[6,22],[8,23],[12,24],[10,25],[8,26],[6,27],[4,28],[2,31],[2,32],[2,33],[2,35],[2,36],[2,37],[2,38],[2,54]]
        var i = pieces.length;
        var king = 10
        var val
        do {
            var p = pieces[--i];
            val = p[0]
        } while (val !== king);
    }
}
