import QtQuick 2.15
import Lomiri.Components 1.3
import QtQuick.Layouts 1.15
import USim 1.0

ListItem {
    property var eSIMInfo: null

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
        title.text: eSIMInfo.nickname !== "" ? (eSIMInfo.nickname + " (" + eSIMInfo.name + ")") : eSIMInfo.name
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
