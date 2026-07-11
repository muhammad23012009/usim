import QtQuick 2.15
import Lomiri.Components 1.3
import Lomiri.Components.Popups 1.3
import USim 1.0

Page {
    id: usimPage
    visible: true
    anchors.fill: parent

    header: PageHeader {
        id: pageHeader
        title: "USim"

        trailingActionBar.actions: [
            Action {
                iconName: "add"
                text: "Add eSIM"
                onTriggered: pageStack.push(Qt.resolvedUrl("qrc:/ui/QRScanPage.qml"))
            },
            Action {
                iconName: "delete"
                text: "Destroy eUICC"
                onTriggered: PopupUtils.open(superScaryDialog)
            }
        ]
    }

    Connections {
        target: USim
        onEsimsChanged: (esims) => columnMeow.model = esims
        onBusyChanged: (busy) => {
            if (busy) {
                PopupUtils.open(busyDialog)
            }
        }

        onShowManualSimDialog: () => {
            PopupUtils.open(addSimDialog)
        }
    }

    ListView {
        id: columnMeow
        anchors {
            top: pageHeader.bottom
            bottom: parent.bottom
            right: parent.right
            left: parent.left
        }

        model: USim.esims

        delegate: EsimDelegate {
            eSIMInfo: modelData

            onClicked: pageStack.push(Qt.resolvedUrl("qrc:/ui/ProfileInfo.qml"), {eSIMInfo: modelData})
        }
    }

    Component {
        id: busyDialog

        Dialog {
            id: meowDialog
            title: "Busy"
            anchorToKeyboard: false

            Connections {
                target: USim

                onBusyChanged: (busy) => {
                    if (!busy) {
                        PopupUtils.close(meowDialog)
                    }
                }

                onStateChanged: {
                    var text = ""
                    print("State changed", USim.state, USimEnums.STARTING)

                    if (USim.state == USimEnums.STARTING) {
                        text = "Starting"
                    } else if (USim.state == USimEnums.GETTING_CHALLENGE) {
                        text = "Getting challenge"
                    } else if (USim.state == USimEnums.INIT_AUTH) {
                        text = "Starting authentication"
                    } else if (USim.state == USimEnums.AUTH_SERVER) {
                        text = "Authenticating with server"
                    } else if (USim.state == USimEnums.AUTH_CLIENT) {
                        text = "Authenticating with client"
                    } else if (USim.state == USimEnums.METADATA_PARSING) {
                        text = "Confirm eSIM install"
                    } else if (USim.state == USimEnums.PREPARE_DOWNLOAD) {
                        text = "Preparing download"
                    } else if (USim.state == USimEnums.GET_BOUND_PACKAGE) {
                        text = "Downloading eSIM"
                    } else if (USim.state == USimEnums.DOWNLOAD_PACKAGE) {
                        text = "Installing eSIM"
                    } else if (USim.state == USimEnums.PROCESS_AND_FINISH) {
                        text = "Power-cycling the eUICC"
                    } else if (USim.state == USimEnums.ENABLING) {
                        text = "Enabling eSIM"
                    } else if (USim.state == USimEnums.DISABLING) {
                        text = "Disabling eSIM"
                    } else if (USim.state == USimEnums.REMOVING) {
                        text = "Removing eSIM"
                    } else {
                        text = "Busy"
                    }

                    meowDialog.title = text
                }
            }

            Label {
                text: "Are you sure you want to install the eSIM with the following details?\n\n" +
                      "Profile Name: " + USim.installingEsim.name + "\n" +
                      "Service Provider: " + USim.installingEsim.providerName + "\n" +
                      "ICCID: " + USim.installingEsim.iccid

                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
                visible: USim.state === USimEnums.METADATA_PARSING
            }

            Button {
                text: "Yes"
                onClicked: {
                    USim.confirmEsimInstall(true)
                }
                visible: USim.state === USimEnums.METADATA_PARSING
            }

            Button {
                text: "No"
                onClicked: {
                    USim.confirmEsimInstall(false)
                }
                visible: USim.state === USimEnums.METADATA_PARSING
            }
        }
    }

    Component {
        id: addSimDialog

        Dialog {
            id: dialog
            title: "Add an eSIM"

            TextField {
                id: textField
                placeholderText: "LPA:1$..."
            }

            Button {
                text: "Add"
                onClicked: {
                    PopupUtils.close(dialog)
                    USim.processLpa(textField.text)
                }
            }

            Button {
                text: "Cancel"
                onClicked: PopupUtils.close(dialog)
            }
        }
    }

    Component {
        id: superScaryDialog

        Dialog {
            id: destroyDialog
            title: "Destroy eUICC memory?"

            Label {
                text: "Are you sure you want to destroy the eUICC memory? This will remove all eSIMs and cannot be done. This action is irreversible. Removed eSIMs cannot be reinstalled without provider intervention."
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
            }

            Button {
                text: "Yes"
                color: LomiriColors.red
                onClicked: {
                    PopupUtils.close(destroyDialog)
                    USim.destroyEuicc()
                }
            }

            Button {
                text: "No"
                onClicked: PopupUtils.close(destroyDialog)
            }
        }
    }
}
