import QtQuick 1.0

Rectangle{
    width: 280
    height: 140
    Column {
        MultilineEdit {
            text: 'I am the very model of a modern major general. I\'ve information vegetable, animal and mineral. I know the kings of england and I quote the fights historical - from Marathon to Waterloo in order categorical.';
            width: 182; height: 60;
        }
        Rectangle {height: 20; width: 20;}//Spacer
        MultilineEdit {text: 'Hello world'}
    }
}
