pragma Singleton
import QtQuick

QtObject {
    property color primary: "#89b4fa"
    property color onPrimary: "#1a1a2e"
    property color primaryContainer: "#2a2a4a"
    property color onPrimaryContainer: "#e0e0ff"
    property color surface: "#121218"
    property color onSurface: "#e0e0e0"
    property color surfaceContainer: "#1e1e28"
    property color onSurfaceContainer: "#c0c0c0"
    property color outline: "#404050"
    property color surfaceVariant: "#252535"
    // Backs Theme.tertiary from C++; shown here so the token is discoverable.
    property color tertiary: "#f5a623"
    property color onTertiary: "#4a2f00"
    // Low battery. Fixed red, not seed-derived.
    property color error: "#e5484d"
    property color onError: "#ffffff"
    // Battery fill states. Fixed hues: they mean the same on every machine.
    property color charging: "#2fbc6b"
    property color full: "#5b8dee"
}
