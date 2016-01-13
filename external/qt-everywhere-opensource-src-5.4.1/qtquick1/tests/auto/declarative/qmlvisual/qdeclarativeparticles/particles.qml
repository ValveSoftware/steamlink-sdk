import QtQuick 1.0
import Qt.labs.particles 1.0

Rectangle {
    property string skip: "May contain random numbers"
    width: 640; height: 480; color: "black"

    Particles { id:particlesAneverEmitting
        y:60; width: 260; height:30; source: "star.png";
        lifeSpan:1000; count: 50; angle:70; angleDeviation:36;
        velocity:30; velocityDeviation:10; emissionRate: 0
        ParticleMotionWander { yvariance:5; xvariance:30; pace:100 }
    }
    Particles { id:particlesA //snowy particles from the top
        y:0; width: 260; height:30; source: "star.png";
        lifeSpan:1000; count: 50; angle:70; angleDeviation:36;
        velocity:30; velocityDeviation:10; emissionRate: 10
        ParticleMotionWander { yvariance:5; xvariance:30; pace:100 }
    }

    Particles { id:particlesB //particle fountain bursting every second
        y:280; x:180; width:1; height:1; lifeSpan:1000; source: "star.png"
        count: 100; angle:270; angleDeviation:45; velocity:50; velocityDeviation:30;
        emissionRate: 0
        ParticleMotionGravity { yattractor: 1000; xattractor:0; acceleration:25 }
    }

    Timer { running: true; interval: 1000; repeat: true; onTriggered: particlesB.burst(200, 2000); }

    Column{
        x: 340;
        Repeater{//emission variance test
            model: 5
            delegate: Component{
                Item{
                    width: 100; height: 100
                    Rectangle{
                        color: "blue"
                        width: 2; height: 2;
                        x: 49; y:49;
                    }
                    Particles{
                        x: 50; y:50; width: 0; height: 0;
                        fadeInDuration: 0; fadeOutDuration: 0
                        lifeSpan: 1000; lifeSpanDeviation:0;
                        source: "star.png"
                        count: -1; emissionRate: 120;
                        emissionVariance: index/2;
                        velocity: 250; velocityDeviation: 0;
                        angle: 0; angleDeviation: 0;
                    }
                }
            }
        }
    }
}
