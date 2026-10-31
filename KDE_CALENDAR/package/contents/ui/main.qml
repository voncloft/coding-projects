import QtQuick 2.15
import QtQuick.Layouts 1.15

import org.kde.plasma.components 3.0 as PC3
import org.kde.plasma.plasmoid 2.0

import org.nick.radicaleagenda 1.0

PlasmoidItem {
    id: root
    width: 760
    height: 380

    function isValidDate(d) { return d instanceof Date && !isNaN(d.getTime()) }

    property date selectedDay: new Date()
    property int viewYear: (new Date()).getFullYear()
    property int viewMonth: (new Date()).getMonth()

    property string cfgUrl: ""
    property string cfgUser: ""
    property string cfgPass: ""
    property bool configApplied: false

    property date pendingDay: new Date()
    property int retryCount: 0

    ListModel { id: sortedEvents }

    function minutesFromBucket(bucket) {
        if (!bucket || typeof bucket !== "string") return 999999
        if (bucket === "All day") return -1
        const parts = bucket.split(":")
        if (parts.length !== 2) return 999999
        const hh = parseInt(parts[0], 10)
        const mm = parseInt(parts[1], 10)
        if (isNaN(hh) || isNaN(mm)) return 999999
        return (hh * 60) + mm
    }

    function rebuildSorted() {
        sortedEvents.clear()

        const list = model.exportEvents()
        const arr = []

        for (let i = 0; i < list.length; i++) {
            const it = list[i]
            const bucket = it.startBucket || ""
            const s = it.start

            let keyMin = minutesFromBucket(bucket)
            if (keyMin === 999999 && isValidDate(s)) keyMin = s.getHours() * 60 + s.getMinutes()

            arr.push({
                keyMin: keyMin,
                startBucket: (bucket && bucket.length) ? bucket : (keyMin === -1 ? "All day" : ""),
                summary: it.summary || "",
                start: it.start,
                end: it.end,
                description: it.description || "",
                eventColor: (it.eventColor && it.eventColor.length) ? it.eventColor : "transparent"
            })
        }

        arr.sort(function(a, b) {
            if (a.keyMin !== b.keyMin) return a.keyMin - b.keyMin
            if (a.summary < b.summary) return -1
            if (a.summary > b.summary) return 1
            return 0
        })

        for (let j = 0; j < arr.length; j++) sortedEvents.append(arr[j])
    }

    function monthName(m) {
        const names = ["January","February","March","April","May","June","July","August","September","October","November","December"]
        return names[m] || ""
    }
    function gotoPrevMonth() { if (viewMonth === 0) { viewMonth = 11; viewYear -= 1 } else viewMonth -= 1 }
    function gotoNextMonth() { if (viewMonth === 11) { viewMonth = 0; viewYear += 1 } else viewMonth += 1 }

    function setToday() {
        const t = new Date()
        selectedDay = t
        viewYear = t.getFullYear()
        viewMonth = t.getMonth()
        requestDayRefresh(selectedDay)
    }

    CalDavModel { id: model }
    ConfigStore { id: store }

    function applyToModel() {
        model.baseUrl = (cfgUrl || "").trim()
        model.username = (cfgUser || "").trim()
        model.password = (cfgPass || "")
        configApplied = true
    }

    function loadFromFile() {
        const m = store.load()
        cfgUrl = (m.url || "")
        cfgUser = (m.user || "")
        cfgPass = (m.pass || "")
    }

    function hasCompleteFileConfig() {
        return (cfgUrl.trim().length > 0) &&
               (cfgUser.trim().length > 0) &&
               (cfgPass.length > 0)
    }

    function requestDayRefresh(day) {
        pendingDay = day
        retryCount = 0

        model.refreshForDayAll(pendingDay)

        if ((model.lastError || "").indexOf("No calendars discovered yet") !== -1) {
            model.discoverCalendars()
            retryTimer.restart()
        }
    }

    Timer {
        id: retryTimer
        interval: 350
        repeat: true
        running: false
        onTriggered: {
            retryCount += 1
            if (retryCount > 20) { stop(); return }

            model.refreshForDayAll(pendingDay)

            if ((model.lastError || "").indexOf("No calendars discovered yet") === -1) stop()
        }
    }

    Connections {
        target: model
        function onLoadingChanged() { if (!model.loading) rebuildSorted() }
        function onLastUpdatedChanged() { rebuildSorted() }
    }

    Component.onCompleted: {
        loadFromFile()
        if (hasCompleteFileConfig()) {
            applyToModel()
            model.discoverCalendars()
            requestDayRefresh(selectedDay)
        } else {
            configApplied = false
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 8

        PC3.TabBar {
            id: tabs
            Layout.fillWidth: true
            PC3.TabButton { text: "Agenda" }
            PC3.TabButton { text: "Settings" }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: tabs.currentIndex

            // ===== Agenda =====
            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 8

                    RowLayout {
                        Layout.fillWidth: true

                        PC3.Label {
                            Layout.fillWidth: true
                            text: "Radicale Agenda"
                            font.bold: true
                            elide: Text.ElideRight
                        }

                        PC3.Button {
                            Layout.preferredWidth: 120
                            text: "Today"
                            enabled: !model.loading && root.configApplied
                            onClicked: root.setToday()
                        }
                    }

                    PC3.Label {
                        Layout.fillWidth: true
                        visible: !root.configApplied
                        wrapMode: Text.WordWrap
                        opacity: 0.85
                        text: "Not configured yet. Go to Settings, enter URL/USER/PASS, click Save, then Apply + Refresh."
                    }

                    PC3.Label {
                        Layout.fillWidth: true
                        visible: model.lastError.length > 0
                        text: model.lastError
                        wrapMode: Text.WordWrap
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        spacing: 12

                        // LEFT: sorted list
                        PC3.Frame {
                            Layout.fillWidth: true
                            Layout.fillHeight: true

                            ColumnLayout {
                                anchors.fill: parent
                                spacing: 8

                                PC3.Label {
                                    Layout.fillWidth: true
                                    font.bold: true
                                    text: "Events for " + root.selectedDay.toLocaleDateString()
                                }

                                PC3.ScrollView {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true

                                    ListView {
                                        model: sortedEvents
                                        clip: true
                                        spacing: 6

                                        section.property: "startBucket"
                                        section.criteria: ViewSection.FullString
                                        section.delegate: PC3.Label {
                                            width: ListView.view.width
                                            padding: 6
                                            opacity: 0.85
                                            font.bold: true
                                            text: section
                                        }

                                        delegate: PC3.ItemDelegate {
                                            width: ListView.view.width

                                            contentItem: RowLayout {
                                                spacing: 10

                                                Rectangle {
                                                    width: 8
                                                    Layout.fillHeight: true
                                                    radius: 2
                                                    color: (typeof eventColor === "string" && eventColor.length > 0) ? eventColor : "transparent"
                                                }

                                                ColumnLayout {
                                                    Layout.fillWidth: true
                                                    spacing: 2

                                                    PC3.Label {
                                                        Layout.fillWidth: true
                                                        text: summary
                                                        font.bold: true
                                                        wrapMode: Text.WordWrap
                                                    }

                                                    PC3.Label {
                                                        Layout.fillWidth: true
                                                        opacity: 0.8
                                                        wrapMode: Text.WordWrap
                                                        text: {
                                                            const s = root.isValidDate(start) ? start.toLocaleTimeString() : ""
                                                            const e = root.isValidDate(end) ? end.toLocaleTimeString() : ""
                                                            return e ? (s + " → " + e) : (s || startBucket)
                                                        }
                                                    }

                                                    PC3.Label {
                                                        Layout.fillWidth: true
                                                        visible: (typeof description === "string" && description.length > 0)
                                                        opacity: 0.75
                                                        wrapMode: Text.WordWrap
                                                        text: description
                                                    }
                                                }
                                            }
                                        }

                                        footer: PC3.Label {
                                            width: ListView.view.width
                                            padding: 8
                                            opacity: 0.7
                                            text: (sortedEvents.count === 0 && !model.loading && model.lastError.length === 0)
                                                  ? "No events for this day."
                                                  : ""
                                        }
                                    }
                                }
                            }
                        }

                        // RIGHT: calendar month grid (restored)
                        PC3.Frame {
                            Layout.preferredWidth: 340
                            Layout.fillHeight: true

                            ColumnLayout {
                                anchors.fill: parent
                                spacing: 8

                                RowLayout {
                                    Layout.fillWidth: true
                                    PC3.Button { text: "◀"; onClicked: root.gotoPrevMonth() }

                                    PC3.Label {
                                        Layout.fillWidth: true
                                        horizontalAlignment: Text.AlignHCenter
                                        font.bold: true
                                        text: root.monthName(root.viewMonth) + " " + root.viewYear
                                    }

                                    PC3.Button { text: "▶"; onClicked: root.gotoNextMonth() }
                                }

                                GridLayout {
                                    Layout.fillWidth: true
                                    columns: 7
                                    rowSpacing: 4
                                    columnSpacing: 4

                                    Repeater {
                                        model: ["Sun","Mon","Tue","Wed","Thu","Fri","Sat"]
                                        delegate: PC3.Label {
                                            Layout.fillWidth: true
                                            horizontalAlignment: Text.AlignHCenter
                                            opacity: 0.8
                                            text: modelData
                                            font.bold: true
                                        }
                                    }
                                }

                                GridLayout {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    columns: 7
                                    rowSpacing: 4
                                    columnSpacing: 4

                                    function firstCellDate() {
                                        const firstOfMonth = new Date(root.viewYear, root.viewMonth, 1)
                                        const dow = firstOfMonth.getDay()
                                        return new Date(root.viewYear, root.viewMonth, 1 - dow)
                                    }

                                    Repeater {
                                        model: 42
                                        delegate: PC3.Button {
                                            readonly property date base: parent.firstCellDate()
                                            readonly property date day: new Date(base.getFullYear(),
                                                                                 base.getMonth(),
                                                                                 base.getDate() + index)

                                            Layout.fillWidth: true
                                            Layout.fillHeight: true
                                            text: day.getDate().toString()
                                            opacity: (day.getMonth() === root.viewMonth) ? 1.0 : 0.45
                                            enabled: root.configApplied && !model.loading

                                            onClicked: {
                                                root.selectedDay = day
                                                root.viewYear = day.getFullYear()
                                                root.viewMonth = day.getMonth()
                                                root.requestDayRefresh(day)
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // ===== Settings =====
            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 10

                    PC3.Label {
                        Layout.fillWidth: true
                        font.bold: true
                        text: "File: " + store.path
                        elide: Text.ElideRight
                    }

                    PC3.TextField {
                        Layout.fillWidth: true
                        placeholderText: "url=... (blank => RADICALE_URL)"
                        text: root.cfgUrl
                        onTextChanged: root.cfgUrl = text
                    }

                    PC3.TextField {
                        Layout.fillWidth: true
                        placeholderText: "user=... (blank => RADICALE_USER)"
                        text: root.cfgUser
                        onTextChanged: root.cfgUser = text
                    }

                    PC3.TextField {
                        Layout.fillWidth: true
                        echoMode: TextInput.Password
                        placeholderText: "pass=... (blank => RADICALE_PASS)"
                        text: root.cfgPass
                        onTextChanged: root.cfgPass = text
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        PC3.Button { text: "Load"; onClicked: root.loadFromFile() }
                        PC3.Button { text: "Save"; onClicked: store.save(root.cfgUrl, root.cfgUser, root.cfgPass) }

                        PC3.Button {
                            text: "Apply + Refresh"
                            onClicked: {
                                root.applyToModel()
                                model.discoverCalendars()
                                root.requestDayRefresh(root.selectedDay)
                                tabs.currentIndex = 0
                            }
                        }

                        PC3.Button {
                            text: "Clear File"
                            onClicked: {
                                store.clear()
                                root.cfgUrl = ""
                                root.cfgUser = ""
                                root.cfgPass = ""
                                root.configApplied = false
                                sortedEvents.clear()
                            }
                        }
                    }

                    PC3.Label {
                        Layout.fillWidth: true
                        visible: store.lastError.length > 0
                        text: store.lastError
                        wrapMode: Text.WordWrap
                    }

                    Item { Layout.fillHeight: true }
                }
            }
        }
    }
}
