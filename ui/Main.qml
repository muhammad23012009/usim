import QtQuick 2.15
import Lomiri.Components 1.3
import USim 1.0

MainView {
    applicationName: "usim.thevancedgamer"
    visible: true
    anchorToKeyboard: true

    PageStack {
        id: pageStack
    }

    Component.onCompleted: {
        USim.getInstalledEsims()
        pageStack.push(Qt.resolvedUrl("qrc:/ui/USim.qml"))
    }
}
