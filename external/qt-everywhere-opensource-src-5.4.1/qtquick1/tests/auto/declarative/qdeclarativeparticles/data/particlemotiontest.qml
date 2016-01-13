import QtQuick 1.0
import Qt.labs.particles 1.0

Rectangle {
    width: 240
    height: 320
    color: "black"
    Particles {
        objectName: "particles"
        anchors.fill: parent
        width: 1
        height: 1
        source: "particle.png"
        lifeSpan: 5000
        count: 200
        angle: 270
        angleDeviation: 45
        velocity: 50
        velocityDeviation: 30
        ParticleMotionGravity {
            objectName: "motionGravity"
            yattractor: 1000
            xattractor: 0
            acceleration: 25
        }
    }
    resources: [
        ParticleMotionWander {
            objectName: "motionWander"
            xvariance: 30
            yvariance: 30
            pace: 100
        }
    ]
}
