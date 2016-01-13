import QtQuick 1.0
import "../shared" 1.0

Rectangle {
    width: 450
    height: 250

    TestTextEdit {
        anchors.fill: parent
        anchors { leftMargin: 10; rightMargin: 10; topMargin:10; bottomMargin: 10 }
        wrapMode: Text.Wrap
        horizontalAlignment: Text.AlignJustify

        text: "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Proin a aliquet massa. Integer id velit a nibh imperdiet sagittis. Cras fringilla enim non nulla porta bibendum. Integer risus urna, hendrerit non interdum ut, dapibus id velit. Nullam fermentum viverra pellentesque. In molestie scelerisque lorem molestie ultrices. Curabitur dolor arcu, tristique in sodales in, varius sed diam. Quisque magna velit, tincidunt sed ullamcorper sit amet, ornare adipiscing ligula. In hac habitasse platea dictumst. Ut tincidunt urna vel mauris fermentum ornare quis a ligula. Suspendisse cursus volutpat sapien eget cursus."

        Rectangle {
            anchors.fill:  parent
            color: "transparent"
            border.color:  "red"
        }
    }
}
