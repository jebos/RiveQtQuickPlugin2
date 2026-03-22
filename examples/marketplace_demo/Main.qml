pragma ComponentBehavior: Bound

import RiveQtQuick
import QtQuick.Window
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

Window {
    id: window
    width: 1500
    height: 920
    minimumWidth: 360
    minimumHeight: 640
    visible: true
    color: "#11161d"
    title: "QtQuickRivePlugin2 demo"
    property bool compactLayout: width < 1100
    property int compactPageIndex: 0
    property url activePreviewSource: ""
    property url pendingPreviewSource: ""
    property bool previewSwitchPendingClear: false
    property var currentMeta: catalog.currentMetadata
    property bool metaCanDownload: Boolean(currentMeta.canDownload)
    property bool metaIsDownloading: Boolean(currentMeta.isDownloading)
    property bool metaIsForHire: Boolean(currentMeta.isForHire)
    property bool livePreviewVisible: activePreviewSource.toString().length > 0 &&
                                      activePreviewSource.toString() === catalog.currentRiveSource.toString()
    property bool centralActionEnabled: catalog.currentHasPreview || metaCanDownload
    property string centralActionText: metaIsDownloading
                                       ? "Downloading..."
                                       : catalog.currentHasPreview
                                         ? "Load live preview"
                                           : metaCanDownload
                                             ? "Download local copy"
                                             : "Poster only"

    function availabilityChipText(availability, hasPreview) {
        if (!hasPreview)
            return "Poster only"
        if (availability === "local")
            return "Bundled"
        if (availability === "downloaded")
            return "Downloaded"
        if (availability === "remote")
            return "Featured"
        return availability
    }

    function availabilityChipColor(availability, hasPreview) {
        if (!hasPreview)
            return "#d7b87a"
        if (availability === "local")
            return "#86d7ff"
        if (availability === "downloaded")
            return "#8ee7b0"
        if (availability === "remote")
            return "#bba7ff"
        return "#8ee7b0"
    }

    function schedulePreviewSourceRefresh(forceReload) {
        pendingPreviewSource = catalog.currentHasPreview ? catalog.currentRiveSource : ""

        if (!forceReload && activePreviewSource.toString() === pendingPreviewSource.toString()) {
            return
        }

        if (activePreviewSource.toString().length > 0) {
            previewSwitchPendingClear = true
            activePreviewSource = ""
            return
        }

        previewSwitchPendingClear = false
        sourceSwitchTimer.restart()
    }

    Timer {
        id: sourceSwitchTimer
        interval: 0
        repeat: false
        onTriggered: activePreviewSource = pendingPreviewSource
    }

    component SectionTitle: Label {
        color: "#e8edf2"
        font.pixelSize: 15
        font.bold: true
    }

    component MetaRow: RowLayout {
        required property string label
        required property string value
        Layout.fillWidth: true
        spacing: 8

        Label {
            text: parent.label
            color: "#8fa1b3"
            font.pixelSize: 12
            Layout.preferredWidth: 90
        }

        Label {
            text: parent.value
            color: "#f3f6f9"
            font.pixelSize: 12
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }
    }

    component CatalogPane: Frame {
        id: catalogPane
        required property bool compactMode
        padding: 0
        background: Rectangle {
            radius: 18
            color: "#16202a"
            border.color: "#24313f"
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 18
            spacing: 14

            SectionTitle { text: catalogPane.compactMode ? "Browse marketplace" : "Marketplace" }

            TextField {
                Layout.fillWidth: true
                placeholderText: "Try a title, author or tag"
                text: catalog.searchText
                onTextChanged: catalog.searchText = text
            }

            Label {
                Layout.fillWidth: true
                color: "#8fa1b3"
                wrapMode: Text.Wrap
                font.pixelSize: 12
                text: catalogPane.compactMode
                      ? "Search the featured feed, tap a card, then switch between preview and inspector pages. Starter assets stay available even if the phone is offline."
                      : "This list mixes a few bundled starter assets with the public featured feed, so the demo still works before the network catches up."
            }

            Frame {
                visible: catalogPane.compactMode
                Layout.fillWidth: true
                padding: 12
                background: Rectangle {
                    radius: 14
                    color: "#1a2631"
                    border.color: "#2d3b48"
                }

                RowLayout {
                    anchors.fill: parent
                    spacing: 10

                    Rectangle {
                        Layout.preferredWidth: 8
                        Layout.preferredHeight: 8
                        radius: 4
                        color: String(catalog.loadError || "").length > 0 ? "#ff9f8a" : "#7bd7a8"
                    }

                    Label {
                        Layout.fillWidth: true
                        color: "#dce7f2"
                        font.pixelSize: 12
                        wrapMode: Text.Wrap
                        text: String(catalog.loadError || "").length > 0
                              ? "Showing bundled starter files right now. Use the button below to retry the online feed."
                              : "Bundled starter files are pinned first. Use Load More below to pull in featured marketplace files."
                    }
                }
            }

            Label {
                Layout.fillWidth: true
                visible: String(catalog.loadError || "").length > 0
                color: "#ffb4a8"
                wrapMode: Text.Wrap
                font.pixelSize: 12
                text: "Live marketplace feed unavailable right now: " + String(catalog.loadError || "")
            }

            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                ListView {
                    id: catalogList
                    anchors.fill: parent
                    model: catalog
                    spacing: 10
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds
                    reuseItems: true
                    cacheBuffer: 1200
                    ScrollIndicator.vertical: ScrollIndicator {}
                    footer: Column {
                        width: ListView.view.width
                        spacing: 10

                        Item {
                            width: parent.width
                            height: 4
                        }

                        Label {
                            width: parent.width
                            visible: !catalogPane.compactMode &&
                                     (catalog.isLoadingMore || catalog.hasMoreRemoteEntries)
                            horizontalAlignment: Text.AlignHCenter
                            color: "#8fa1b3"
                            font.pixelSize: 12
                            text: catalog.isLoadingMore ? "Pulling in a few more..." : "Keep scrolling for more"
                        }

                        Button {
                            width: parent.width
                            visible: catalogPane.compactMode &&
                                     (catalog.hasMoreRemoteEntries ||
                                      String(catalog.loadError || "").length > 0)
                            enabled: !catalog.isLoadingMore
                            text: catalog.isLoadingMore
                                  ? "Loading..."
                                  : String(catalog.loadError || "").length > 0
                                    ? "Retry online feed"
                                    : "Load more featured files"
                            onClicked: catalog.loadMore()
                        }

                        Item {
                            width: parent.width
                            height: 8
                        }
                    }
                    onContentYChanged: {
                        if (catalogPane.compactMode)
                            return
                        if (catalog.isLoadingMore || !catalog.hasMoreRemoteEntries)
                            return
                        if (contentY + height >= contentHeight - 320)
                            catalog.loadMore()
                    }
                    onCountChanged: {
                        if (catalogPane.compactMode)
                            return
                        if (catalog.isLoadingMore || !catalog.hasMoreRemoteEntries)
                            return
                        if (contentHeight <= height + 200)
                            catalog.loadMore()
                    }

                    delegate: Rectangle {
                        required property int index
                        required property string title
                        required property string author
                        required property string license
                        required property var tags
                        required property string description
                        required property string previewImageUrl
                        required property int reactionCount
                        required property bool current
                        required property bool isForHire
                        required property string availability
                        required property bool hasPreview

                        width: ListView.view.width
                        height: catalogPane.compactMode ? 132 : 138
                        radius: 16
                        color: current ? "#213241" : "#19242e"
                        border.color: current ? "#63c7ff" : "#2a3947"

                        TapHandler {
                            onTapped: {
                                catalog.currentIndex = index
                                if (catalogPane.compactMode)
                                    window.compactPageIndex = 1
                            }
                        }

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 14
                            spacing: 12

                            Rectangle {
                                Layout.preferredWidth: catalogPane.compactMode ? 86 : 94
                                Layout.fillHeight: true
                                radius: 12
                                color: "#101820"
                                border.color: "#314150"
                                clip: true

                                Image {
                                    anchors.fill: parent
                                    source: previewImageUrl
                                    fillMode: Image.PreserveAspectCrop
                                    asynchronous: true
                                    cache: true
                                    smooth: true
                                    visible: status === Image.Ready
                                }

                                Rectangle {
                                    anchors.fill: parent
                                    visible: parent.children[0].status !== Image.Ready
                                    color: "#17212a"

                                    Label {
                                        anchors.centerIn: parent
                                        text: title.length > 0 ? title.charAt(0).toUpperCase() : "R"
                                        color: "#a8c2d8"
                                        font.pixelSize: 30
                                        font.bold: true
                                    }
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                spacing: 8

                                RowLayout {
                                    Layout.fillWidth: true

                                    Label {
                                        text: title
                                        color: "#f5f8fb"
                                        font.pixelSize: 15
                                        font.bold: true
                                        Layout.fillWidth: true
                                        elide: Text.ElideRight
                                    }

                                    Rectangle {
                                        visible: isForHire
                                        radius: 999
                                        color: "#21533c"
                                        implicitWidth: hireLabel.implicitWidth + 14
                                        implicitHeight: hireLabel.implicitHeight + 6

                                        Label {
                                            id: hireLabel
                                            anchors.centerIn: parent
                                            text: "For hire"
                                            color: "#baf0ce"
                                            font.pixelSize: 11
                                        }
                                    }
                                }

                                Label {
                                    text: "by " + author
                                    color: "#91a3b5"
                                    font.pixelSize: 12
                                }

                                Label {
                                    Layout.fillWidth: true
                                    text: description
                                    color: "#d4dee7"
                                    font.pixelSize: 12
                                    wrapMode: Text.Wrap
                                    maximumLineCount: catalogPane.compactMode ? 2 : 3
                                    elide: Text.ElideRight
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 8

                                    Repeater {
                                        model: tags
                                        delegate: Rectangle {
                                            required property int index
                                            required property var modelData
                                            visible: !catalogPane.compactMode || index < 2
                                            radius: 999
                                            color: "#23303d"
                                            implicitWidth: tagLabel.implicitWidth + 12
                                            implicitHeight: tagLabel.implicitHeight + 4

                                            Label {
                                                id: tagLabel
                                                anchors.centerIn: parent
                                                text: modelData
                                                color: "#b9c7d4"
                                                font.pixelSize: 10
                                            }
                                        }
                                    }

                                    Item { Layout.fillWidth: true }

                                    Label {
                                        text: reactionCount > 0 ? reactionCount + " likes" : "fresh"
                                        color: "#8fa1b3"
                                        font.pixelSize: 11
                                    }

                                    Label {
                                        text: window.availabilityChipText(availability, hasPreview)
                                        color: window.availabilityChipColor(availability, hasPreview)
                                        font.pixelSize: 11
                                    }

                                    Label {
                                        visible: !catalogPane.compactMode
                                        text: license
                                        color: "#8fa1b3"
                                        font.pixelSize: 11
                                    }
                                }
                            }
                        }
                    }
                }

                Column {
                    anchors.centerIn: parent
                    width: Math.min(parent.width - 40, 340)
                    spacing: 12
                    visible: !catalog.isLoadingMore && catalogList.count === 0

                    Label {
                        width: parent.width
                        text: String(catalog.loadError || "").length > 0
                              ? "Could not load featured assets"
                              : "No assets yet"
                        color: "#eef4fa"
                        font.pixelSize: 22
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.Wrap
                    }

                    Label {
                        width: parent.width
                        text: String(catalog.loadError || "").length > 0
                              ? String(catalog.loadError)
                              : "The featured feed came back empty. Pull to retry or tap the button below."
                        color: "#93a6ba"
                        font.pixelSize: 12
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.Wrap
                    }

                    Button {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "Retry"
                        onClicked: catalog.loadMore()
                    }
                }
            }
        }
    }

    component PreviewPane: Frame {
        id: previewPane
        required property bool compactMode
        property alias riveView: riveView
        padding: 0
        background: Rectangle {
            radius: 18
            color: "#171d24"
            border.color: "#24313f"
        }

        Item {
            anchors.fill: parent

            RiveItem {
                id: riveView
                objectName: "riveView"
                anchors.fill: parent
                anchors.margins: previewPane.compactMode ? 12 : 18
                source: activePreviewSource
                fit: RiveItem.Contain
                Component.onCompleted: schedulePreviewSourceRefresh(false)
            }

            Connections {
                target: catalog
                function onCurrentIndexChanged() {
                    schedulePreviewSourceRefresh(false)
                }
                function onCurrentRiveSourceChanged() {
                    schedulePreviewSourceRefresh(false)
                }
                function onCurrentHasPreviewChanged() {
                    schedulePreviewSourceRefresh(false)
                }
            }

            Connections {
                target: riveView
                function onStatusChanged() {
                    if (!previewSwitchPendingClear)
                        return
                    if (riveView.status !== RiveItem.Null)
                        return
                    previewSwitchPendingClear = false
                    sourceSwitchTimer.restart()
                }
            }

            Rectangle {
                anchors.fill: parent
                anchors.margins: previewPane.compactMode ? 12 : 18
                radius: 14
                visible: !livePreviewVisible
                color: "#11161d"
                border.color: "#2b3948"
                clip: true

                Image {
                    id: heroPoster
                    anchors.fill: parent
                    source: String(currentMeta.previewImageUrl || "")
                    fillMode: Image.PreserveAspectCrop
                    asynchronous: true
                    cache: true
                    smooth: true
                    visible: status === Image.Ready
                }

                Rectangle {
                    anchors.fill: parent
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: "#110d1116" }
                        GradientStop { position: 0.52; color: "#6611161d" }
                        GradientStop { position: 1.0; color: "#e611161d" }
                    }
                }

                Column {
                    anchors.centerIn: parent
                    width: Math.min(parent.width - (previewPane.compactMode ? 40 : 80), 360)
                    spacing: 16
                    visible: centralActionEnabled || metaIsDownloading

                    Button {
                        id: centerActionButton
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: parent.width
                        height: 68
                        enabled: centralActionEnabled
                        text: centralActionText

                        background: Rectangle {
                            radius: 20
                            color: centerActionButton.enabled ? "#ff8d71" : "#45515d"
                            border.color: centerActionButton.enabled ? "#ffd0c1" : "#697684"
                            border.width: 2
                        }

                        contentItem: Label {
                            text: centerActionButton.text
                            color: "#fff8f4"
                            font.pixelSize: 22
                            font.bold: true
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        onClicked: {
                            if (catalog.currentHasPreview)
                                schedulePreviewSourceRefresh(true)
                            else
                                catalog.downloadCurrent()
                        }
                    }

                    Label {
                        width: parent.width
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.Wrap
                        color: "#d5dee7"
                        font.pixelSize: 13
                        text: catalog.currentHasPreview
                              ? "Open the real file and start poking at the artboards, state machines and inputs."
                              : metaCanDownload
                                ? "Grab the actual .riv file locally. The poster will switch to a live preview right after."
                                : metaIsDownloading
                                  ? "The runtime file is on its way."
                                  : ""
                    }
                }

                Column {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.margins: previewPane.compactMode ? 18 : 26
                    spacing: 12

                    Label {
                        text: heroPoster.status === Image.Ready ? "Poster view" : "Nothing to preview yet"
                        color: "#eef4fa"
                        font.pixelSize: previewPane.compactMode ? 20 : 24
                        font.bold: true
                    }

                    Label {
                        width: parent.width
                        text: catalog.currentHasPreview
                              ? "This one has a local runtime file, so you can actually poke at it."
                              : metaIsDownloading
                                ? "Downloading the runtime file now. As soon as it lands in the cache, the live preview swaps in."
                                : metaCanDownload
                                  ? "Right now this is just the marketplace poster. The big button in the middle grabs the live file."
                                  : "This one is poster and metadata only for now."
                        color: "#d0dbe5"
                        font.pixelSize: 13
                        wrapMode: Text.Wrap
                    }

                    Label {
                        visible: String(currentMeta.downloadError || "").length > 0
                        width: parent.width
                        text: String(currentMeta.downloadError || "")
                        color: "#ff9e9e"
                        font.pixelSize: 12
                        wrapMode: Text.Wrap
                    }

                    Label {
                        visible: metaIsDownloading &&
                                 Number(currentMeta.downloadTotalBytes || 0) > 0
                        text: "Download progress: " +
                              Math.round(Number(currentMeta.downloadBytes || 0) * 100 /
                                         Number(currentMeta.downloadTotalBytes || 0)) + "%"
                        color: "#8ee7b0"
                        font.pixelSize: 12
                    }
                }
            }
        }
    }

    component InspectorPane: Frame {
        id: inspectorPane
        required property var riveView
        required property bool compactMode
        function setInspectorPage(index) {
            if (compactMode)
                compactInspectorChooser.currentIndex = index
            else
                tabs.currentIndex = index
        }

        padding: 0
        background: Rectangle {
            radius: 18
            color: "#16202a"
            border.color: "#24313f"
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 18
            spacing: 14

            RowLayout {
                Layout.fillWidth: true

                SectionTitle { text: inspectorPane.compactMode ? "Inspector & controls" : "Inspector" }

                Item { Layout.fillWidth: true }

                Button {
                    visible: inspectorPane.compactMode
                    text: "Preview"
                    onClicked: window.compactPageIndex = 1
                }
            }

            GridLayout {
                Layout.fillWidth: true
                columns: inspectorPane.compactMode ? 1 : 3
                columnSpacing: 10
                rowSpacing: 10

                Button {
                    Layout.fillWidth: true
                    text: metaIsDownloading
                          ? "Downloading..."
                          : catalog.currentHasPreview
                            ? "Reload preview"
                            : metaCanDownload
                              ? "Get local copy"
                              : "No preview"
                    enabled: catalog.currentHasPreview || metaCanDownload
                    onClicked: {
                        if (catalog.currentHasPreview)
                            schedulePreviewSourceRefresh(true)
                        else
                            catalog.downloadCurrent()
                    }
                }

                Button {
                    Layout.fillWidth: true
                    text: "Open marketplace"
                    enabled: String(currentMeta.sourceUrl || "").length > 0
                    onClicked: Qt.openUrlExternally(String(currentMeta.sourceUrl || ""))
                }

                Button {
                    Layout.fillWidth: true
                    text: "Asset info"
                    enabled: true
                    onClicked: inspectorPane.setInspectorPage(4)
                }
            }

            GridLayout {
                Layout.fillWidth: true
                columns: inspectorPane.compactMode ? 1 : 3
                columnSpacing: 10
                rowSpacing: 10

                Button {
                    Layout.fillWidth: true
                    text: inspectorPane.riveView.playing ? "Pause" : "Play"
                    onClicked: inspectorPane.riveView.playing ? inspectorPane.riveView.pause() : inspectorPane.riveView.play()
                }

                Button {
                    Layout.fillWidth: true
                    text: "Reload"
                    onClicked: inspectorPane.riveView.reload()
                }

                Label {
                    Layout.fillWidth: true
                    horizontalAlignment: inspectorPane.compactMode ? Text.AlignLeft : Text.AlignRight
                    color: inspectorPane.riveView.status === RiveItem.Error ? "#ff9e9e" : "#8fa1b3"
                    text: inspectorPane.riveView.status === RiveItem.Error ? inspectorPane.riveView.errorString : "Ready"
                    wrapMode: Text.Wrap
                }
            }

            TabBar {
                id: tabs
                visible: !inspectorPane.compactMode
                Layout.fillWidth: true

                TabButton { text: "Artboards" }
                TabButton { text: "State Machines" }
                TabButton { text: "Inputs" }
                TabButton { text: "Events" }
                TabButton { text: "Asset Info" }
            }

            ComboBox {
                id: compactInspectorChooser
                visible: inspectorPane.compactMode
                Layout.fillWidth: true
                model: ["Artboards", "State Machines", "Inputs", "Events", "Asset Info"]
            }

            StackLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                currentIndex: inspectorPane.compactMode ? compactInspectorChooser.currentIndex : tabs.currentIndex

                ListView {
                    clip: true
                    model: inspectorPane.riveView.artboardsModel
                    spacing: 8
                    delegate: Button {
                        required property string name
                        required property bool current
                        width: ListView.view.width
                        text: current ? name + "  (current)" : name
                        highlighted: current
                        onClicked: inspectorPane.riveView.selectArtboard(name)
                    }
                }

                ListView {
                    clip: true
                    model: inspectorPane.riveView.stateMachinesModel
                    spacing: 8
                    delegate: Button {
                        required property string name
                        required property bool current
                        width: ListView.view.width
                        text: current ? name + "  (current)" : name
                        highlighted: current
                        onClicked: inspectorPane.riveView.selectStateMachine(name)
                    }
                }

                Item {
                    clip: true

                    ListView {
                        id: inputsList
                        anchors.fill: parent
                        model: inspectorPane.riveView.inputsModel
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
                                            inspectorPane.riveView.fireViewModelTrigger(inputCard.path)
                                        else
                                            inspectorPane.riveView.fireTrigger(inputCard.name)
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
                                            inspectorPane.riveView.setViewModelValue(inputCard.path, checked)
                                        else
                                            inspectorPane.riveView.setBoolean(inputCard.name, checked)
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
                                                inspectorPane.riveView.setViewModelValue(inputCard.path, value)
                                            else
                                                inspectorPane.riveView.setNumber(inputCard.name, value)
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
                                            inspectorPane.riveView.setViewModelValue(inputCard.path, text)
                                        else
                                            inspectorPane.riveView.setString(inputCard.name, text)
                                    }
                                }
                            }
                        }
                    }

                    Column {
                        anchors.centerIn: parent
                        spacing: 8
                        visible: inputsList.count === 0

                        Label {
                            text: "Nothing to tweak here"
                            color: "#eef4fa"
                            font.pixelSize: 18
                            font.bold: true
                        }

                        Label {
                            width: 260
                            text: "The selected file is not exposing any state machine or view model controls right now."
                            color: "#93a6ba"
                            font.pixelSize: 12
                            wrapMode: Text.Wrap
                            horizontalAlignment: Text.AlignHCenter
                        }
                    }
                }

                ColumnLayout {
                    spacing: 10

                    RowLayout {
                        Layout.fillWidth: true
                        SectionTitle { text: "Recent events" }
                        Item { Layout.fillWidth: true }
                        Button {
                            text: "Clear"
                            onClicked: inspectorPane.riveView.clearEventLog()
                        }
                    }

                    ListView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        model: inspectorPane.riveView.eventsModel
                        spacing: 8

                        delegate: Frame {
                            required property string name
                            required property var payload
                            required property double timestamp
                            required property string sourceStateMachine
                            width: ListView.view.width
                            padding: 10
                            background: Rectangle {
                                radius: 14
                                color: "#1b2733"
                                border.color: "#2d3c4c"
                            }

                            Column {
                                anchors.fill: parent
                                spacing: 4

                                Label {
                                    text: name
                                    color: "#eef4fa"
                                    font.pixelSize: 13
                                    font.bold: true
                                }

                                Label {
                                    text: sourceStateMachine.length > 0 ? sourceStateMachine : "Unknown source"
                                    color: "#8fa1b3"
                                    font.pixelSize: 11
                                }

                                Label {
                                    width: parent.width
                                    text: JSON.stringify(payload)
                                    color: "#c8d6e3"
                                    font.pixelSize: 11
                                    wrapMode: Text.Wrap
                                }
                            }
                        }
                    }
                }

                ScrollView {
                    clip: true

                    ColumnLayout {
                        width: parent.width
                        spacing: 10

                        SectionTitle {
                            text: String(currentMeta.title || "").length > 0 ? String(currentMeta.title) : "Nothing selected yet"
                        }

                        MetaRow { label: "Author"; value: String(currentMeta.author || "").length > 0 ? String(currentMeta.author) : "Unknown" }
                        MetaRow { label: "License"; value: String(currentMeta.license || "").length > 0 ? String(currentMeta.license) : "Unknown" }
                        MetaRow { label: "Availability"; value: String(currentMeta.availability || "").length > 0 ? String(currentMeta.availability) : "Unknown" }
                        MetaRow { label: "Preview"; value: catalog.currentHasPreview ? "Live" : "Poster only" }
                        MetaRow { label: "Cached file"; value: String(currentMeta.localFilePath || "").length > 0 ? String(currentMeta.localFilePath) : "Not downloaded" }
                        MetaRow { label: "For hire"; value: metaIsForHire ? "Yes" : "No" }
                        MetaRow { label: "Source"; value: String(currentMeta.sourceUrl || "").length > 0 ? String(currentMeta.sourceUrl) : "Unavailable" }

                        Label {
                            text: "Tags"
                            color: "#8fa1b3"
                            font.pixelSize: 12
                        }

                        Flow {
                            Layout.fillWidth: true
                            spacing: 8

                            Repeater {
                                model: currentMeta.tags || []
                                delegate: Rectangle {
                                    required property var modelData
                                    radius: 999
                                    color: "#22303d"
                                    implicitWidth: tagText.implicitWidth + 12
                                    implicitHeight: tagText.implicitHeight + 4

                                    Label {
                                        id: tagText
                                        anchors.centerIn: parent
                                        text: modelData
                                        color: "#dce8f2"
                                        font.pixelSize: 10
                                    }
                                }
                            }
                        }

                        Label {
                            text: "About"
                            color: "#8fa1b3"
                            font.pixelSize: 12
                        }

                        Label {
                            Layout.fillWidth: true
                            text: String(currentMeta.description || "")
                            color: "#eef4fa"
                            wrapMode: Text.Wrap
                        }
                    }
                }
            }
        }
    }

    Component {
        id: wideWorkspaceComponent

        RowLayout {
            anchors.fill: parent
            anchors.margins: 16
            spacing: 16

            CatalogPane {
                compactMode: false
                Layout.preferredWidth: 360
                Layout.minimumWidth: 360
                Layout.maximumWidth: 360
                Layout.fillHeight: true
            }

            PreviewPane {
                id: widePreviewPane
                compactMode: false
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumWidth: 520
            }

            InspectorPane {
                compactMode: false
                riveView: widePreviewPane.riveView
                Layout.preferredWidth: 400
                Layout.minimumWidth: 400
                Layout.maximumWidth: 400
                Layout.fillHeight: true
            }
        }
    }

    Component {
        id: compactWorkspaceComponent

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 12

            Frame {
                Layout.fillWidth: true
                padding: 0
                background: Rectangle {
                    radius: 18
                    color: "#16202a"
                    border.color: "#24313f"
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 12

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4

                        Label {
                            Layout.fillWidth: true
                            text: window.compactPageIndex === 0
                                  ? "Marketplace"
                                  : String(currentMeta.title || "").length > 0
                                    ? String(currentMeta.title)
                                    : "Select an asset"
                            color: "#eef4fa"
                            font.pixelSize: 22
                            font.bold: true
                            elide: Text.ElideRight
                        }

                        Label {
                            Layout.fillWidth: true
                            text: window.compactPageIndex === 0
                                  ? "A phone-friendly flow: browse, preview, then inspect."
                                  : String(currentMeta.author || "").length > 0
                                    ? "by " + String(currentMeta.author)
                                    : "Use the tabs below to jump between pages."
                            color: "#8fa1b3"
                            font.pixelSize: 12
                            wrapMode: Text.Wrap
                        }
                    }

                    Button {
                        visible: window.compactPageIndex !== 0
                        text: "Browse"
                        onClicked: window.compactPageIndex = 0
                    }

                    Button {
                        visible: window.compactPageIndex === 1
                        text: "Inspect"
                        onClicked: window.compactPageIndex = 2
                    }

                    Button {
                        visible: window.compactPageIndex === 2
                        text: "Preview"
                        onClicked: window.compactPageIndex = 1
                    }
                }
            }

            TabBar {
                Layout.fillWidth: true
                currentIndex: window.compactPageIndex

                TabButton {
                    text: "Browse"
                    onClicked: window.compactPageIndex = 0
                }

                TabButton {
                    text: "Preview"
                    onClicked: window.compactPageIndex = 1
                }

                TabButton {
                    text: "Inspect"
                    onClicked: window.compactPageIndex = 2
                }
            }

            StackLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                currentIndex: window.compactPageIndex

                CatalogPane {
                    compactMode: true
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                }

                PreviewPane {
                    id: compactPreviewPane
                    compactMode: true
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                }

                InspectorPane {
                    compactMode: true
                    riveView: compactPreviewPane.riveView
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                }
            }
        }
    }

    Loader {
        anchors.fill: parent
        sourceComponent: compactLayout ? compactWorkspaceComponent : wideWorkspaceComponent
    }
}
