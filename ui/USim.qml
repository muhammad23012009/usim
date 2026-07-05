import QtQuick
import Lomiri.Components
import Lomiri.Components.Popups
import USim

Page {
    visible: true
    anchors.fill: parent

    header: PageHeader {
        id: pageHeader
        title: "USim"

        trailingActionBar.actions: [
            Action {
                iconName: "add"
                text: "Add eSIM"
                onTriggered: PopupUtils.open(addSimDialog)
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
        }
    }

    Component {
        id: busyDialog

        Dialog {
            id: meowDialog
            title: "Busy"

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

            Text {
                text: "Are you sure you want to install the eSIM with the following details?\n\n" +
                      "Profile Name: " + USim.installingEsim.name + "\n" +
                      "Service Provider: " + USim.installingEsim.providerName + "\n" +
                      "ICCID: " + USim.installingEsim.iccid

                wrapMode: Text.WordWrap
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
}
