import QtQuick 2.15
import QtQuick.Layouts 1.15
import Lomiri.Components 1.3
import QZXing 3.3
import QtMultimedia 5.15
import USim 1.0

Page {
    header: PageHeader {
        title: "Scan QR code to connect"
    }

    Component.onCompleted: {
        print("Starting camera");
        camera.start();
    }

    Component.onDestruction: {
        print("Destroying QR scan page");
        camera.stop();
        viewfinder.source = null;
    }

    function clear() {
        viewfinder.source = null;
        camera.stop();
        pageStack.pop();
    }

    QZXingFilter {
        id: scanner
        active: true
        orientation: viewfinder.orientation

        captureRect: {
            var s = viewfinder.sourceRect;
            var c = viewfinder.contentRect;
            if (s.width <= 0 || c.width <= 0)
                return Qt.rect(0, 0, 0, 0);
            return viewfinder.mapRectToSource(viewfinder.mapFromItem(scanBox, 0, 0, scanBox.width, scanBox.height));
        }

        decoder {
            enabledDecoders: QZXing.DecoderFormat_QR_CODE
            imageSourceFilter: QZXing.SourceFilter_ImageNormal | QZXing.SourceFilter_ImageInverted
            tryHarder: true 

            onTagFound: {
                if (tag.startsWith("LPA:")) {
                    clear();
                    USim.processLpa(tag);
                } else {
                    console.warn("Invalid QR code scanned:", tag);
                }
            }
        }
    }

    property Camera camera: Camera {
        id: camera
        captureMode: Camera.CaptureViewfinder
        
        // Ensure continuous auto-focus is running so codes aren't blurry
        focus {
            focusMode: Camera.FocusContinuous
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: units.gu(0.75)

        Rectangle {
            id: uiRect
            Layout.alignment: Qt.AlignTop
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            VideoOutput {
                id: viewfinder
                anchors.fill: parent
                source: camera
                autoOrientation: true
                focus: true
                fillMode: VideoOutput.PreserveAspectCrop
                flushMode: VideoOutput.EmptyFrame

                filters: [ scanner ]
            }

            Rectangle {
                id: scanBox
                property bool scanSuccess: false

                width: units.gu(30)
                height: units.gu(30)
                anchors.centerIn: parent
                color: "transparent"
                border.color: "white"
                border.width: units.gu(0.4)
            }

            Canvas {
                id: dimOverlay
                anchors.fill: parent

                onPaint: {
                    var ctx = getContext("2d");
                    ctx.save();
                    ctx.reset();

                    ctx.fillStyle = "rgba(0, 0, 0, 0.75)";
                    ctx.fillRect(0, 0, width, height);

                    ctx.clearRect(scanBox.x, scanBox.y, scanBox.width, scanBox.height);

                    ctx.restore();
                }
            }
        }

        Label {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            text: "Or alternatively, enter a code from your provider"
            Layout.alignment: Qt.AlignCenter
            textSize: Label.Medium
        }

        Button {
            Layout.alignment: Qt.AlignCenter
            Layout.bottomMargin: units.gu(1.5)
            text: "Enter code manually"
            onClicked: {
                clear();
                USim.openManualDialog();
            }
        }
    }
}
