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
            },
            Action {
                iconName: "system-restart"
                text: "Restart ofono"
                onTriggered: PopupUtils.open(restartDialog)
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

        onShowManualSimDialog: {
            PopupUtils.open(addSimDialog)
        }

        onErrorOccured: {
            print("Error occured: " + USim.errorTitle + " - " + USim.errorMessage)
            PopupUtils.open(errorDialog)
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

    Label {
        text: "No eSIMs installed.\nInstall one by clicking the + button in the top right corner."
        width: parent.width

        anchors.centerIn: parent

        visible: USim.esims.length === 0
        wrapMode: Text.WordWrap
        horizontalAlignment: Text.AlignHCenter
        clip: true
    }

    Component {
        id: busyDialog

        Dialog {
            id: meowDialog
            title: {
                var state = USim.state;
                switch (state) {
                    case USimEnums.STARTING:
                        return "Starting"
                    case USimEnums.RESTARTING_OFONO:
                        return "Restarting ofono"
                    case USimEnums.GETTING_CHALLENGE:
                        return "Getting challenge"
                    case USimEnums.INIT_AUTH:
                        return "Starting authentication"
                    case USimEnums.AUTH_SERVER:
                        return "Authenticating with server"
                    case USimEnums.AUTH_CLIENT:
                        return "Authenticating with client"
                    case USimEnums.METADATA_PARSING:
                        return "Confirm eSIM install"
                    case USimEnums.PREPARE_DOWNLOAD:
                        return "Preparing download"
                    case USimEnums.GET_BOUND_PACKAGE:
                        return "Downloading eSIM"
                    case USimEnums.DOWNLOAD_PACKAGE:
                        return "Installing eSIM"
                    case USimEnums.PROCESS_AND_FINISH:
                        return "Processing transaction"
                    case USimEnums.ENABLING:
                        return "Enabling eSIM"
                    case USimEnums.DISABLING:
                        return "Disabling eSIM"
                    case USimEnums.REMOVING:
                        return "Removing eSIM"
                    default:
                        return "Busy"
                }
            }

            anchorToKeyboard: false

            Connections {
                target: USim

                onBusyChanged: (busy) => {
                    if (!busy) {
                        PopupUtils.close(meowDialog)
                    }
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
        id: errorDialog

        Dialog {
            id: theDialogOfError
            title: USim.errorTitle

            Label {
                text: USim.errorMessage
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
            }

            Button {
                text: "Cancel"
                onClicked: PopupUtils.close(theDialogOfError)
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

    Component {
        id: restartDialog

        Dialog {
            id: ofonoDialog
            title: "Restart ofono?"

            Label {
                text: "Are you sure you want to restart ofono? This is needed to resume cellular connectivity after interacting with eSIM profiles."
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
            }

            Button {
                text: "Yes"
                color: LomiriColors.green
                onClicked: {
                    PopupUtils.close(ofonoDialog)
                    USim.restartOfono()
                }
            }

            Button {
                text: "No"
                onClicked: PopupUtils.close(ofonoDialog)
            }
        }
    }
}
