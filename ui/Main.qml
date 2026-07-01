import QtQuick
import QtQuick.Controls
import USim

ApplicationWindow {
    width: 640
    height: 480
    visible: true

    TextField {
        onAccepted: {
            USim.processLpa(text)
        }
    }

    Component.onCompleted: {
        USim.getInstalledEsims()
        USim.removeEsim("8944476500008875622")
    }
}