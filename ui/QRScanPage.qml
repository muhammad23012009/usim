import QtQuick
import Lomiri.Components
import QZXing

Page {
    header: PageHeader {
        title: "Scan QR code"
    }

    QZXingFilter {
        id: scanner
        active: true
        decoder {
            enabledDecoders: QZXing.DecoderFormat_QR_CODE
            imageSourceFilter: QZXing.SourceFilter_ImageNormal |
                               QZXing.SourceFilter_ImageInverted
        }
    }

    Camera {
        id: camera
    }

    VideoOutput {
        anchors.fill: parent
        source: camera
        autoOrientation: true
        focus : CameraFocus { focusMode: CameraFocus.FocusContinuous }

        filters: [ scanner ]
    }
}