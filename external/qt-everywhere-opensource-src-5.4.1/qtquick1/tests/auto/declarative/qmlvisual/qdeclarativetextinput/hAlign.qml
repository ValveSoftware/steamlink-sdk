import QtQuick 1.0
import "../shared" 1.0

Item{
    width:600;
    height:300;
    Column {
        //Because they have auto width, these three should look the same
        TestTextInput {
            text: "Jackdaws love my big sphinx of quartz";
            horizontalAlignment: TextInput.AlignLeft;
        }
        TestTextInput {
            text: "Jackdaws love my big sphinx of quartz";
            horizontalAlignment: TextInput.AlignHCenter;
        }
        TestTextInput {
            text: "Jackdaws love my big sphinx of quartz";
            horizontalAlignment: TextInput.AlignRight;
        }
        Rectangle{ width: 600; height: 10; color: "pink" }
        TestTextInput {
            height: 30;
            width: 600;
            text: "Jackdaws love my big sphinx of quartz";
            horizontalAlignment: TextInput.AlignLeft;
        }
        TestTextInput {
            height: 30;
            width: 600;
            text: "Jackdaws love my big sphinx of quartz";
            horizontalAlignment: TextInput.AlignHCenter;
        }
        TestTextInput {
            height: 30;
            width: 600;
            text: "Jackdaws love my big sphinx of quartz";
            horizontalAlignment: TextInput.AlignRight;
        }
    }
}
