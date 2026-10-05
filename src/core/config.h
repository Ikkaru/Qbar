#pragma once
#include <QString>
#include <QStringList>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QFile>
#include <QMap>

struct BarConfig {
    QString position = "top";
    QString integrations = "appbar";
    QString shape = "pill";
    bool autoHide = true;
    int height = 40;
    int marginTop = 2;
    int marginHorizontal = 12;
    bool hideOnFullWindow = false;
    bool hideOnFullscreen = true;
    QString backdrop = "clear";
    double backdropStrength = 0.5;
    QString backdropColor = "#1e1e28";
    double backdropOpacity = 0.55;
    int borderWidth = 0;
    QString borderColor = "#404050";
    int cornerRadius = 12;
    double surfaceOpacity = 0.55;
    // Soft edge shadow, drawn inside the bar's own bounds. See Bar.qml: growing
    // the window to let a shadow escape would intercept clicks meant for the
    // windows underneath.
    bool shadowEnabled = true;
    int shadowHeight = 14;
    double shadowOpacity = 0.22;
    // The 1px rule along the bar's bottom edge. Kept separate from the gradient
    // because it does the actual separating, while the gradient only softens it.
    bool shadowHairline = true;
    double shadowHairlineOpacity = 0.28;
    // Add or remove an HKCU Run entry at startup. The user can also flip it from
    // Task Manager's Startup tab at any time; sync() only acts when the two
    // disagree.
    bool autostart = false;
};

struct ThemeConfig {
    QString seed = "#89b4fa";
    QString font = "Segoe UI Variable";
    QString iconFont = "MaterialSymbolsRounded";
    // Text colour follows the wallpaper behind the bar when autoContrast is on.
    bool autoContrast = true;
    QString textOnLight = "#101010";
    QString textOnDark = "#FFFFFF";
    // 0-255 brightness the background must reach before light text turns dark.
    // Pastel wallpapers stay on white even when black would technically score a
    // higher contrast ratio.
    int lightThreshold = 165;
};

struct LayoutConfig {
    QStringList left;
    QStringList center;
    QStringList right;
};

struct Config {
    BarConfig bar;
    ThemeConfig theme;
    LayoutConfig layout;
    QMap<QString, QJsonObject> modules;
    // action -> chord, e.g. "reload" -> "Ctrl+Alt+R". Read by Hotkeys.
    QMap<QString, QString> hotkeys;

    bool load(const QString& path);
    bool save(const QString& path) const;
    QString themeSeed() const { return theme.seed; }

    bool moduleEnabled(const QString& id) const;
    QJsonObject moduleConfig(const QString& id) const;
};
