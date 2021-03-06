/**
* Filename: Wallet.qml
*
* XCITE is a secure platform utilizing the XTRABYTES Proof of Signature
* blockchain protocol to host decentralized applications
*
* Copyright (c) 2017-2018 Zoltan Szabo & XTRABYTES developers
*
* This file is part of an XTRABYTES Ltd. project.
*
*/

import QtQuick 2.7
import QtQuick.Controls 2.3
import QtGraphicalEffects 1.0
import QtQuick.Window 2.2
import QtQuick.Layouts 1.3
import QtMultimedia 5.8
import QtCharts 2.0

import "qrc:/Controls" as Controls
import "qrc:/Controls/+mobile" as Mobile
import "qrc:/Controls/+desktop" as Desktop

Rectangle {
    id: backgroundWallet
    z: 1
    width: appWidth/6 * 5
    height: appHeight
    anchors.right: xcite.right
    anchors.top: xcite.top
    color: bgcolor
    clip: true

    property var balanceArray: ((totalBalance).toLocaleString(Qt.locale("en_US"), "f", 2)).split('.')
    property int newAlert: newAlerts
    property bool myTheme: darktheme

    onMyThemeChanged: {
        coinsButton.border.color = themecolor
        coinsLabel.color = themecolor
    }

    onNewAlertChanged: {
        sumBalance()
    }

    Label {
        id: walletLabel
        text: "WALLET"
        anchors.right: parent.right
        anchors.rightMargin: appWidth/12
        anchors.top: parent.top
        anchors.topMargin: appWidth/24
        font.pixelSize: appHeight/18
        font.family: xciteMobile.name
        color: darktheme == true? "#F2F2F2" : "#2A2C31"
        font.letterSpacing: 2
    }

    Item {
        id: totalWalletValue
        width: valueTicker.implicitWidth + value1.implicitWidth + value2.implicitWidth
        height: value1.implicitHeight
        anchors.top: walletLabel.bottom
        anchors.topMargin: appWidth/48
        anchors.right: walletLabel.right

        Label {
            id: valueTicker
            anchors.left: totalWalletValue.left
            anchors.bottom: value1.bottom
            text: userSettings.showBalance === true? fiatTicker + " " : ""
            font.pixelSize: appHeight/15
            font.family: xciteMobile.name
            color: themecolor
            visible: userSettings.showBalance === true
        }

        Label {
            id: value1
            anchors.left: valueTicker.right
            anchors.verticalCenter: totalWalletValue.verticalCenter
            text: userSettings.showBalance === true? balanceArray[0] : "****"
            font.pixelSize: appHeight/15
            font.family: xciteMobile.name
            color: themecolor
        }

        Label {
            id: value2
            anchors.left: value1.right
            anchors.bottom: value1.bottom
            text: userSettings.showBalance === true? ("." + balanceArray[1]) : ""
            font.pixelSize: appHeight/15
            font.family: xciteMobile.name
            color: themecolor
        }
    }

    Label {
        id: totalLabel
        text: "TOTAL"
        anchors.right: parent.right
        anchors.rightMargin: appWidth/12
        anchors.top: totalWalletValue.bottom
        anchors.topMargin: appWidth/192
        font.pixelSize: appHeight/54
        font.family: xciteMobile.name
        color: themecolor
        font.letterSpacing: 2
    }

    Desktop.Portfolio {
        id: myPortfolio
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.leftMargin: appWidth/24
    }

    Rectangle {
        id: coinsButton
        height: appHeight/27
        width: appWidth/6
        radius: height/2
        anchors.right: parent.right
        anchors.rightMargin: appWidth/12
        anchors.top: totalLabel.bottom
        anchors.topMargin: appHeight/36
        color: "transparent"
        border.width: 1
        border.color: themecolor

        Rectangle {
            id: selectCoins
            anchors.fill: parent
            radius: height/2
            color: maincolor
            opacity: 0.3
            visible: false
        }

        Label {
            id: coinsLabel
            text: "MANAGE COINS"
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.verticalCenter: parent.verticalCenter
            font.pixelSize: parent.height/2
            font.family: xciteMobile.name
            color: themecolor
        }

        MouseArea {
            anchors.fill: parent
            hoverEnabled: true

            onEntered: {
                selectCoins.visible = true
            }

            onExited: {
                selectCoins.visible = false
            }

            onPressed: {
                click01.play()
                detectInteraction()
            }

            onClicked: {
                if (addCoins.sidebarState == "closed") {
                    addCoins.sidebarState = "open"
                }
                else {
                    addCoins.sidebarState = "closed"
                }
            }
        }
    }

    Rectangle {
        id: walletArea
        width: parent.width
        anchors.top: coinsButton.bottom
        anchors.topMargin: appHeight/36
        anchors.bottom: parent.bottom
        anchors.bottomMargin: appWidth/24
        color: "transparent"
        clip: true

        Desktop.CoinList {
            id: myCoinlist
            anchors.top: parent.top
        }

        DropShadow {
            anchors.fill: addCoins
            source: addCoins
            horizontalOffset: -4
            verticalOffset: -4
            radius: 12
            samples: 25
            spread: 0
            color: "black"
            opacity: 0.3
            transparentBorder: true
        }

        Mobile.AddCoinModal {
            id: addCoins
            width: parent.width
            height: parent.height
            color: "transparent"
            anchors.top: walletArea.top
            anchors.topMargin: 0
            anchors.right: walletArea.right
            colomnWidth: coinsButton.width + coinsButton.anchors.rightMargin
        }
    }

    Desktop.CoinWallets {
        id: mycoinWallets
        anchors.top: parent.top
    }

    Desktop.AddWallet {
        id: myAddWallet
        anchors.top: parent.top
    }

    Desktop.ImportWallet {
        id: myImportWallet
        anchors.top: parent.top
    }

    Desktop.CreateWallet {
        id: myCreateWallet
        anchors.top: parent.top
    }

    Desktop.AddViewOnlyWallet {
        id: myViewOnlyWallet
        anchors.top: parent.top
    }

    Desktop.WalletModal {
        id: myWalletModal
        anchors.top: parent.top
    }

    Desktop.TransactionDetailModal {
        id: myTransactionDetails
        anchors.top:parent.top
    }

    Desktop.TransferModal {
        id: myTransferModal
        anchors.top: parent.top
    }

    Image {
        id: bigLogo
        source: getLogoBig(getName(coinIndex))
        height: appWidth/6
        fillMode: Image.PreserveAspectFit
        anchors.top: parent.top
        anchors.horizontalCenter: parent.left
        visible: coinTracker == 1 || transferTracker == 1
    }

    Rectangle {
        height: appWidth/24
        width: parent.width
        anchors.bottom: parent.bottom
        color: darktheme == true? "#14161B" : "#FDFDFD"
    }
}
