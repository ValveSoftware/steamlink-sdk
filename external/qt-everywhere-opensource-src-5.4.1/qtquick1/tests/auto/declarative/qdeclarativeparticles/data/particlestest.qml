import QtQuick 1.0
import Qt.labs.particles 1.0

Rectangle{
    width: 100
    height: 100
    color: "black"
    objectName: "rect"
    Particles { id: particles
        objectName: "particles"
        width:1; height:1; anchors.centerIn: parent; opacity: 1
        lifeSpan: 100; lifeSpanDeviation: 20; count:1000;
        fadeInDuration: 20; fadeOutDuration: 20; emissionRate: 1000
        angle: 0; angleDeviation: 360; velocity: 500; velocityDeviation:30
        source: "particle.png"
    }
}
