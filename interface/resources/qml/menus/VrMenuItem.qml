//
//  VrMenuItem.qml
//
//  Created by Bradley Austin Davis on 29 Apr 2015
//  Copyright 2015 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

import QtQuick 2.5
import QtQuick.Controls 1.4
import QtQuick.Controls.Styles 1.4

import "../controls-uit"
import "../styles-uit"

Item {
    id: root
    HifiConstants { id: hifi  }
    property alias text: label.text
    property var source

    implicitHeight: source.visible ? 2 * label.implicitHeight : 0
    implicitWidth: 2 * hifi.dimensions.menuPadding.x + check.width + label.width + tail.width
    visible: source.visible
    width: parent.width

    CheckBox {
        id: check
        // FIXME: Shouild use radio buttons if source.exclusiveGroup.
        anchors {
            left: parent.left
            leftMargin: hifi.dimensions.menuPadding.x
            top: label.top
            topMargin: 0
        }
        width: 20
        visible: source.visible && source.type === 1 && source.checkable
        checked: setChecked()
        function setChecked() {
            if (!source || source.type !== 1 || !source.checkable) {
                return false;
            }
            // FIXME this works for native QML menus but I don't think it will
            // for proxied QML menus
            return source.checked;
        }
    }

    RalewaySemiBold {
        id: label
        size: hifi.fontSizes.rootMenu
        font.capitalization: Font.AllUppercase
        anchors.left: check.right
        anchors.verticalCenter: parent.verticalCenter
        verticalAlignment: Text.AlignVCenter
        color: source.enabled ? hifi.colors.baseGrayShadow : hifi.colors.baseGrayShadow50
        enabled: source.visible && (source.type !== 0 ? source.enabled : false)
        visible: source.visible
    }

    Item {
        // Space for shortcut key or disclosure icon.
        id: tail
        width: 48
        anchors {
            verticalCenter: parent.verticalCenter
            right: parent.right
            rightMargin: hifi.dimensions.menuPadding.x
        }

        HiFiGlyphs {
            text: hifi.glyphs.disclosureExpand
            color: source.enabled ? hifi.colors.baseGrayShadow : hifi.colors.baseGrayShadow25
            size: 2 * hifi.fontSizes.rootMenuDisclosure
            anchors.verticalCenter: parent.verticalCenter
            anchors.right: parent.right
            horizontalAlignment: Text.AlignRight
            visible: source.visible && (source.type === 2)
        }
    }
}
