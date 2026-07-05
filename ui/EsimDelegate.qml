import QtQuick
import Lomiri.Components
import QtQuick.Layouts
import USim

ListItem {
    property var eSIMInfo: null

    Component.onCompleted: {
        print("Creating delegate with props:", eSIMInfo.name, eSIMInfo.providerName, eSIMInfo.iccid);
    }

    height: layout.height + (divider.visible ? divider.height : 0)

    leadingActions: ListItemActions {
        actions: [
            Action {
                iconName: "delete"
                onTriggered: {
                    USim.removeEsim(eSIMInfo.iccid);
                }
            }
        ]
    }

    ListItemLayout {
        id: layout
        title.text: eSIMInfo.name
        subtitle.text: eSIMInfo.providerName

        Switch {
            checked: eSIMInfo ? eSIMInfo.enabled : false
            onCheckedChanged: {
                if (eSIMInfo.enabled == checked)
                    return;

                if (checked) {
                    USim.enableEsim(eSIMInfo.iccid);
                } else {
                    USim.disableEsim(eSIMInfo.iccid);
                }
            }
        }
    }
}
