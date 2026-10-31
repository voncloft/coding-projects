import QtQuick 2.15
import QtQuick.Layouts 1.15
import org.kde.kirigami 2.20 as Kirigami
import org.kde.plasma.configuration 2.0

Kirigami.FormLayout {
    property alias cfg_calendarUrl: calendarUrl.text
    property alias cfg_username: username.text
    property alias cfg_password: password.text
    property alias cfg_daysAhead: daysAhead.value

    Kirigami.FormLayout.anchors.leftMargin: 0
    Kirigami.FormLayout.anchors.rightMargin: 0

    Kirigami.Heading {
        text: "Radicale / CalDAV"
        level: 3
        Kirigami.FormData.isSection: true
    }

    TextField {
        id: calendarUrl
        Kirigami.FormData.label: "Calendar URL:"
        placeholderText: "https://server.example/caldav/user/calendar/"
    }

    TextField {
        id: username
        Kirigami.FormData.label: "Username:"
        placeholderText: "user"
    }

    TextField {
        id: password
        Kirigami.FormData.label: "Password:"
        echoMode: TextInput.Password
        placeholderText: "stored in plasmoid config (MVP)"
    }

    SpinBox {
        id: daysAhead
        Kirigami.FormData.label: "Days ahead:"
        from: 1
        to: 365
        value: 7
    }

    Kirigami.InlineMessage {
        type: Kirigami.MessageType.Warning
        text: "MVP uses Basic Auth and stores credentials in the plasmoid config. For production, use KWallet/Secret Service."
        visible: true
    }
}
