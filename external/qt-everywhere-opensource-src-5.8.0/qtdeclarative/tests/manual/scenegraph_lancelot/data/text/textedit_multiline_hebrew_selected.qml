import QtQuick 2.0

Item {
    width: 320
    height: 480

    TextEdit {
        id: textEdit
        anchors.centerIn: parent
        font.family: "Arial"
        font.pixelSize: 16
        width: 200
        wrapMode: TextEdit.WrapAtWordBoundaryOrAnywhere
        text: "הבהרה לערכים אחד אל, אל הספרות אירועים ותשובות שתי, סדר בה עסקים העזרה אגרונומיה. את והוא מוסיקה סטטיסטיקה זכר. בקר העמוד לחיבור של, ויש את הקהילה ממונרכיה פסיכולוגיה. שער המזנון לויקיפדיה את, אתה סרבול ערכים בלשנות או, ריקוד האנציקלופדיה כדי דת. אל לתרום משחקים וספציפיים אחד, או מיזמי המקובל כלל. אם הקהילה משופרות אחד, בקר רפואה מדויקים של, פנאי וספציפיים כלל מה. לעתים וספציפיים סוציולוגיה עזה אל, לשון אקראי דת אחד, רקטות לויקיפדים אחד את."

        Component.onCompleted: {
            textEdit.select(50, 80)
        }
    }

}
