import QtQuick 2.15
import QtQuick.Layouts 1.15
import Lomiri.Components 1.3
import Lomiri.Components.ListItems 1.3
import USim 1.0

Page {
    property var eSIMInfo: null

    header: PageHeader {
        id: pageHeader
        title: eSIMInfo.name
    }

    ListModel {
        id: items

        Component.onCompleted: {
            items.clear();
            items.append({title: "Profile name", value: eSIMInfo.name});
            items.append({title: "ICCID", value: eSIMInfo.iccid});
            items.append({title: "ISD-P AID", value: eSIMInfo.isdpAid});
            items.append({title: "Service provider", value: eSIMInfo.providerName});
            items.append({title: "MCC/MNC", value: eSIMInfo.plmn});
            items.append({title: "Profile state", value: eSIMInfo.enabled ? "Enabled" : "Disabled"});
            items.append({title: "Profile class", value: eSIMInfo.profileClass});
        }
    }

    ListView {
        id: listView
        clip: true

        anchors {
            top: pageHeader.bottom
            bottom: parent.bottom
            right: parent.right
            left: parent.left
        }

        // this is so hacky lol
        header: Rectangle {
            color: "transparent"
            width: parent.width
            height: layout.implicitHeight + units.gu(1)

            ColumnLayout {
                id: layout
                anchors.fill: parent

                Label {
                    Layout.alignment: Qt.AlignLeft
                    Layout.leftMargin: units.gu(1)
                    Layout.topMargin: units.gu(1)
                    text: "Nickname"
                    font.bold: true
                }

                TextField {
                    id: nicknameField

                    Layout.fillWidth: true
                    Layout.leftMargin: units.gu(1)
                    Layout.rightMargin: units.gu(1)
                    Layout.topMargin: units.gu(0.5)

                    text: eSIMInfo.nickname
                }

                Button {
                    Layout.fillWidth: true
                    Layout.leftMargin: units.gu(1)
                    Layout.rightMargin: units.gu(1)
                    Layout.topMargin: units.gu(0.5)
                    Layout.bottomMargin: units.gu(1)
                    text: "Set nickname"

                    onClicked: {
                        USim.renameEsim(eSIMInfo.iccid, nicknameField.text);
                    }
                }

                Divider {}

                Label {
                    Layout.alignment: Qt.AlignLeft
                    Layout.margins: units.gu(1)
                    text: "Profile information"
                    font.bold: true
                }
            }
        }

        model: items
        delegate: ListItem {
            height: layout.implicitHeight + (divider.visible ? divider.height : 0)

            ListItemLayout {
                id: layout

                title.text: model.title
                subtitle.text: model.value
            }
        }
    }
}