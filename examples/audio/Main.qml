import RiveQtQuick
import QtQuick.Layouts
import QtQuick
import QtQuick.Window
import QtQuick.Controls

Window {
    width: 800
    height: 600
    visible: true
    color: "#101418"
    title: "Rive Qt Quick Interactive"

    RiveItem {
        id: riveView
        objectName: "riveView"
        
        anchors.fill: parent
        anchors.rightMargin: inspectorPane.width
        
        source: exampleRiveUrl
    }

    Item {
        id: inspectorPane
        anchors.top: parent.top
        anchors.right: parent.right
        width: 300
        height: parent.height
        visible: true

        ListView {
            id: inputsList
            anchors.fill: parent
            model: riveView.inputsModel
            spacing: 10
            delegate: Frame {
                id: inputCard
                required property string name
                required property string path
                required property string displayName
                required property string kind
                required property string source
                required property var value
                required property var minimum
                required property var maximum
                property bool usesViewModelApi: source === "ViewModel"

                width: ListView.view.width
                padding: 10
                background: Rectangle {
                    radius: 14
                    color: "#1b2733"
                    border.color: "#2d3c4c"
                }

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 8

                    Label {
                        text: displayName
                        color: "#eef4fa"
                        font.pixelSize: 13
                        font.bold: true
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Label {
                            text: kind
                            color: "#8fa1b3"
                            font.pixelSize: 11
                        }

                        Rectangle {
                            radius: 999
                            color: source === "ViewModel" ? "#27445a" : "#314429"
                            implicitWidth: sourceLabel.implicitWidth + 12
                            implicitHeight: sourceLabel.implicitHeight + 4

                            Label {
                                id: sourceLabel
                                anchors.centerIn: parent
                                text: source
                                color: "#e7f2fb"
                                font.pixelSize: 10
                            }
                        }

                        Item { Layout.fillWidth: true }

                        Label {
                            visible: path !== displayName
                            text: path
                            color: "#6f8395"
                            font.pixelSize: 10
                        }
                    }

                    Button {
                        visible: inputCard.kind === "Trigger"
                        text: inputCard.usesViewModelApi ? "Fire View Model Trigger" : "Fire Trigger"
                        onClicked: {
                            if (inputCard.usesViewModelApi)
                                riveView.fireViewModelTrigger(inputCard.path)
                            else
                                riveView.fireTrigger(inputCard.name)
                        }
                    }

                    Switch {
                        id: boolSwitch
                        visible: inputCard.kind === "Boolean"
                        Binding on checked {
                            value: Boolean(inputCard.value)
                            when: !boolSwitch.down
                        }
                        onToggled: {
                            if (inputCard.usesViewModelApi)
                                riveView.setViewModelValue(inputCard.path, checked)
                            else
                                riveView.setBoolean(inputCard.name, checked)
                        }
                    }

                    ColumnLayout {
                        visible: inputCard.kind === "Number"
                        spacing: 6

                        Slider {
                            id: numberSlider
                            Layout.fillWidth: true
                            from: typeof inputCard.minimum === "number" ? inputCard.minimum : Math.min(-100, Number(inputCard.value))
                            to: typeof inputCard.maximum === "number" ? inputCard.maximum : Math.max(100, Number(inputCard.value))
                            Binding on value {
                                value: Number(inputCard.value)
                                when: !numberSlider.pressed
                            }
                            onMoved: {
                                if (inputCard.usesViewModelApi)
                                    riveView.setViewModelValue(inputCard.path, value)
                                else
                                    riveView.setNumber(inputCard.name, value)
                            }
                        }

                        Label {
                            text: Number(numberSlider.pressed ? numberSlider.value : inputCard.value).toFixed(2)
                            color: "#c8d6e3"
                            font.pixelSize: 12
                        }
                    }

                    TextField {
                        id: stringField
                        visible: inputCard.kind === "String"
                        Layout.fillWidth: true
                        Binding on text {
                            value: String(inputCard.value)
                            when: !stringField.activeFocus
                        }
                        onEditingFinished: {
                            if (inputCard.usesViewModelApi)
                                riveView.setViewModelValue(inputCard.path, text)
                            else
                                riveView.setString(inputCard.name, text)
                        }
                    }
                }
            }
        }
    }
}
