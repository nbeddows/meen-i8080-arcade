import QtQuick
import QtQuick.Controls
import meen_i8080_arcade 1.1

ApplicationWindow {
  id: window
  width: QT6IO.windowWidth
  height: QT6IO.windowHeight
  visible: true
  color: "#1E2224"
  title: "MEEN i8080 arcade"

  onClosing: QT6IO.KeyPressed(Qt.Key_Q)

  Item {
    id: keyHandler
    anchors.fill: parent
    focus: true

    QT6IODisplay {
        id: display
        anchors.fill: parent
    }

    Keys.onPressed: function(event) {
      if (!event.isAutoRepeat) {
        QT6IO.KeyPressed(event.key)
      }

      event.accepted = true
    }

    Keys.onReleased: function(event) {
      QT6IO.KeyReleased(event.key)
      event.accepted = true
    }
  }
}