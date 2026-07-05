import QtQuick
import Lomiri.Components
import USim

MainView {
    applicationName: "usim.thevancedgamer"
    visible: true

    Connections {
        target: USim
        onEsimsChanged: {
            print("Porno", esims);
        }
    }

    PageStack {
        id: pageStack
    }

    Component.onCompleted: {
        USim.getInstalledEsims()
        pageStack.push(Qt.resolvedUrl("qrc:/ui/USim.qml"))
    }
}
