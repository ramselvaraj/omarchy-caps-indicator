import QtQuick
import Quickshell
import Quickshell.Io

// Owns the capsim process: builds it on first use, then keeps it running.
// capsim is a Wayland input-method client; Hyprland positions its popup at the
// focused text caret, so nothing here needs to know about window geometry.
Item {
  id: root

  property var shell: null
  property var manifest: null
  property var pluginRegistry: null

  readonly property string pluginDir: manifest && manifest.__sourceDir
    ? String(manifest.__sourceDir)
    : Quickshell.env("HOME") + "/.config/omarchy/plugins/ramselvaraj.caps-indicator"

  property bool binaryFound: false
  property string buildError: ""

  Component.onCompleted: checkProc.running = true

  Process {
    id: checkProc
    command: ["sh", "-c", "test -x \"$1\" && echo yes || echo no", "caps", root.pluginDir + "/bin/capsim"]
    stdout: StdioCollector {
      onStreamFinished: {
        if (text.trim() === "yes") root.binaryFound = true
        else buildProc.running = true
      }
    }
  }

  Process {
    id: buildProc
    command: [root.pluginDir + "/build.sh"]
    stderr: StdioCollector { id: buildErr; waitForEnd: true }
    onExited: function(code) {
      if (code === 0) { root.buildError = ""; root.binaryFound = true }
      else {
        root.buildError = String(buildErr.text || "").trim().split("\n").slice(-3).join("\n") || ("exit " + code)
        console.warn("caps-indicator build failed: " + root.buildError)
      }
    }
  }

  Process {
    id: capsim
    command: [root.pluginDir + "/run.sh"]
    running: root.binaryFound
    stderr: SplitParser { onRead: function(line) { console.warn("capsim: " + line) } }
    onExited: if (root.binaryFound) restartTimer.restart()
  }

  Timer {
    id: restartTimer
    interval: 3000
    onTriggered: if (root.binaryFound && !capsim.running) capsim.running = true
  }
}
