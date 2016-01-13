import QtQuick 2.0

Rectangle {
    id: topRect
    width: 320
    height: 480

    Row{
        id: row_0000
        anchors.top: parent.top
        Row{
            Rectangle{
                id: rect_0001
                width: topRect.width / 4
                height: topRect.height / 4
                color: "red"
                anchors.top: parent.top
            }
            Rectangle{
                id: rect_0002
                width: topRect.width / 4
                height: topRect.height / 4
                color: "blue"
                anchors.top: rect_0001.Center
                opacity: 0.1
                Text{
                    width: parent.width
                    text: "opac. 0.1"
                    anchors.top: parent.top
                }
            }
        }
        Row{
            Rectangle{
                id: rect_0003
                width: topRect.width / 4
                height: topRect.height / 4
                color: "yellow"
                anchors.top: parent.top
            }
            Rectangle{
                id: rect_0004
                width: topRect.width / 4
                height: topRect.height / 4
                color: "green"
                anchors.top: rect_0003.Center
                opacity: 0.2
                Text{
                    width: parent.width
                    text: "opac. 0.2"
                    anchors.top: parent.top
                }
            }
        }
    }
    Row{
        id: row_0001
        anchors.top: row_0000.bottom
        Row{
            Rectangle{
                id: rect_0005
                width: topRect.width / 4
                height: topRect.height / 4
                color: "black"
                anchors.top: parent.top
            }
            Rectangle{
                id: rect_0006
                width: topRect.width / 4
                height: topRect.height / 4
                color: "white"
                anchors.top: rect_0005.Center
                opacity: 0.3
                Text{
                    width: parent.width
                    text: "opac. 0.3"
                    anchors.top: parent.top
                }
            }
        }
        Row{
            Rectangle{
                id: rect_0007
                width: topRect.width / 4
                height: topRect.height / 4
                color: "violet"
                anchors.top: parent.top
            }
            Rectangle{
                id: rect_0008
                width: topRect.width / 4
                height: topRect.height / 4
                color: "indigo"
                anchors.top: rect_0007.Center
                opacity: 0.4
                Text{
                    text: "opac. 0.4"
                    anchors.top: parent.top
                }
            }
        }
    }
    Row{
        id: row_0002
        anchors.top: row_0001.bottom
        Row{
            Rectangle{
                id: rect_0009
                width: topRect.width / 4
                height: topRect.height / 4
                color: "light grey"
                anchors.top: parent.top
            }
            Rectangle{
                id: rect_0010
                width: topRect.width / 4
                height: topRect.height / 4
                color: "cyan"
                anchors.top: rect_0009.Center
                opacity: 0.5
                Text{
                    text: "opac. 0.5"
                    anchors.top: parent.top
                }
            }
        }
        Row{
            Rectangle{
                id: rect_0011
                width: topRect.width / 4
                height: topRect.height / 4
                color: "orange"
                anchors.top: parent.top
            }
            Rectangle{
                id: rect_0012
                width: topRect.width / 4
                height: topRect.height / 4
                color: "pink"
                anchors.top: rect_0011.Center
                opacity: 0.6
                Text{
                    text: "opac. 0.6"
                    anchors.top: parent.top
                }
            }
        }
    }
    Row{
        id: row_0003
        anchors.top: row_0002.bottom
        Rectangle{
            id: rect_0013
            width: topRect.width / 4
            height: topRect.height / 4
            color: "brown"
            anchors.top: parent.top
        }
        Rectangle{
            id: rect_0014
            width: topRect.width / 4
            height: topRect.height / 4
            color: "light green"
            anchors.top: rect_0013.Center
            opacity: 0.7
            Text{
                text: "opac. 0.7"
                anchors.top: parent.top
            }
        }
        Rectangle{
            id: rect_0015
            width: topRect.width / 4
            height: topRect.height / 4
            color: "dark blue"
            anchors.top: parent.top
        }
        Rectangle{
            id: rect_0016
            width: topRect.width / 4
            height: topRect.height / 4
            color: "light blue"
            anchors.top: rect_0015.Center
            opacity: 0.8
            Text{
                text: "opac. 0.8"
                anchors.top: parent.top
            }
        }
    }
}
