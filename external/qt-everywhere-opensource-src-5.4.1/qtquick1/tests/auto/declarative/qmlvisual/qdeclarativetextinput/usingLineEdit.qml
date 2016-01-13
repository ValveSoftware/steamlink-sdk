import QtQuick 1.0

Rectangle{
    width: 600
    height: 200
    Column {
        LineEdit {text: 'Hello world'}
        LineEdit {text: 'Hello underwhelmingly verbose world'; width: 80; height: 24;}
    }
}
