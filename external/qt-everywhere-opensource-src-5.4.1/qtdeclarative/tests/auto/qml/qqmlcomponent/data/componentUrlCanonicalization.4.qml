import QtQuick 2.0
import "SpecificComponent"
import "./SpecificComponent"
import "././SpecificComponent"
import "./././SpecificComponent"
import "SpecificComponent/."
import "SpecificComponent/./"
import "SpecificComponent/./."
import "SpecificComponent/././"
import "SpecificComponent/././."
import "SpecificComponent/./././"
import "../data/SpecificComponent"
import "./../data/SpecificComponent"
import ".././data/SpecificComponent"
import "SpecificComponent/nonexistent/../."
import "SpecificComponent/nonexistent/.././"
import "SpecificComponent/nonexistent/./.."
import "SpecificComponent/nonexistent/./../"
import "SpecificComponent/nonexistent/nonexistent/../.."
import "SpecificComponent/nonexistent/nonexistent/../../"
import "SpecificComponent/nonexistent/nonexistent/nonexistent/../../.."
import "SpecificComponent/nonexistent/nonexistent/nonexistent/../../../"
import "SpecificComponent/.././SpecificComponent"
import "SpecificComponent/./../SpecificComponent"

Item {
    property bool success : true
}
