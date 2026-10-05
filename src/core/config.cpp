#include "config.h"
#include <QDebug>

bool Config::load(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        qWarning() << "Cannot open config:" << path;
        return false;
    }
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();
    if (!doc.isObject()) return false;

    // load() is called again on every hot reload, so the accumulators must be
    // reset first or module ids pile up into duplicate bar entries.
    layout.left.clear();
    layout.center.clear();
    layout.right.clear();
    modules.clear();

    QJsonObject root = doc.object();

    if (root.contains("bar")) {
        QJsonObject b = root["bar"].toObject();
        bar.position = b.value("position").toString("top");
        bar.integrations = b.value("integrations").toString("appbar");
        bar.shape = b.value("shape").toString("pill");
        bar.autoHide = b.value("autoHide").toBool(true);
        bar.height = b.value("height").toInt(40);
        bar.marginTop = b.value("marginTop").toInt(2);
        bar.marginHorizontal = b.value("marginHorizontal").toInt(12);
        bar.hideOnFullWindow = b.value("hideOnFullWindow").toBool(false);
        bar.hideOnFullscreen = b.value("hideOnFullscreen").toBool(true);
        bar.backdrop = b.value("backdrop").toString("clear");
        bar.backdropStrength = b.value("backdropStrength").toDouble(0.5);
        bar.backdropColor = b.value("backdropColor").toString("#1e1e28");
        bar.backdropOpacity = b.value("backdropOpacity").toDouble(0.55);
        bar.borderWidth = b.value("borderWidth").toInt(0);
        bar.borderColor = b.value("borderColor").toString("#404050");
        bar.cornerRadius = b.value("cornerRadius").toInt(12);
        bar.surfaceOpacity = b.value("surfaceOpacity").toDouble(0.55);
        bar.shadowEnabled = b.value("shadowEnabled").toBool(true);
        bar.shadowHeight = b.value("shadowHeight").toInt(14);
        bar.shadowOpacity = b.value("shadowOpacity").toDouble(0.22);
        bar.shadowHairline = b.value("shadowHairline").toBool(true);
        bar.shadowHairlineOpacity = b.value("shadowHairlineOpacity").toDouble(0.28);
        bar.autostart = b.value("autostart").toBool(false);
    }

    if (root.contains("theme")) {
        QJsonObject t = root["theme"].toObject();
        theme.seed = t.value("seed").toString("#89b4fa");
        theme.font = t.value("font").toString("Segoe UI Variable");
        theme.iconFont = t.value("iconFont").toString("MaterialSymbolsRounded");
        theme.autoContrast = t.value("autoContrast").toBool(true);
        theme.textOnLight = t.value("textOnLight").toString("#101010");
        theme.textOnDark = t.value("textOnDark").toString("#FFFFFF");
        theme.lightThreshold = t.value("lightThreshold").toInt(165);
    }

    if (root.contains("layout")) {
        QJsonObject l = root["layout"].toObject();
        for (const auto& v : l.value("left").toArray())
            layout.left << v.toString();
        for (const auto& v : l.value("center").toArray())
            layout.center << v.toString();
        for (const auto& v : l.value("right").toArray())
            layout.right << v.toString();
    }

    if (root.contains("modules")) {
        QJsonObject m = root["modules"].toObject();
        for (auto it = m.begin(); it != m.end(); ++it) {
            modules[it.key()] = it.value().toObject();
        }
    }

    // Defaults for the two actions the bar understands. A config that omits
    // hotkeys entirely still gets both.
    hotkeys["toggleBar"] = QStringLiteral("Ctrl+Alt+B");
    hotkeys["reload"] = QStringLiteral("Ctrl+Alt+R");
    if (root.contains("hotkeys")) {
        QJsonObject h = root["hotkeys"].toObject();
        for (auto it = h.begin(); it != h.end(); ++it)
            hotkeys[it.key()] = it.value().toString();
    }

    return true;
}

bool Config::save(const QString& path) const {
    QJsonObject b;
    b["position"] = bar.position;
    b["integrations"] = bar.integrations;
    b["shape"] = bar.shape;
    b["autoHide"] = bar.autoHide;
    b["height"] = bar.height;
    b["marginTop"] = bar.marginTop;
    b["marginHorizontal"] = bar.marginHorizontal;
    b["hideOnFullWindow"] = bar.hideOnFullWindow;
    b["hideOnFullscreen"] = bar.hideOnFullscreen;
    b["backdrop"] = bar.backdrop;
    b["backdropStrength"] = bar.backdropStrength;
    b["backdropColor"] = bar.backdropColor;
    b["backdropOpacity"] = bar.backdropOpacity;
    b["borderWidth"] = bar.borderWidth;
    b["borderColor"] = bar.borderColor;
    b["cornerRadius"] = bar.cornerRadius;
    b["surfaceOpacity"] = bar.surfaceOpacity;
    b["shadowEnabled"] = bar.shadowEnabled;
    b["shadowHeight"] = bar.shadowHeight;
    b["shadowOpacity"] = bar.shadowOpacity;
    b["shadowHairline"] = bar.shadowHairline;
    b["shadowHairlineOpacity"] = bar.shadowHairlineOpacity;
    b["autostart"] = bar.autostart;

    QJsonObject t;
    t["seed"] = theme.seed;
    t["font"] = theme.font;
    t["iconFont"] = theme.iconFont;
    t["autoContrast"] = theme.autoContrast;
    t["textOnLight"] = theme.textOnLight;
    t["textOnDark"] = theme.textOnDark;
    t["lightThreshold"] = theme.lightThreshold;

    QJsonArray leftArr, centerArr, rightArr;
    for (const auto& s : layout.left) leftArr.append(s);
    for (const auto& s : layout.center) centerArr.append(s);
    for (const auto& s : layout.right) rightArr.append(s);

    QJsonObject l;
    l["left"] = leftArr;
    l["center"] = centerArr;
    l["right"] = rightArr;

    QJsonObject m;
    for (auto it = modules.constBegin(); it != modules.constEnd(); ++it)
        m[it.key()] = it.value();

    QJsonObject h;
    for (auto it = hotkeys.constBegin(); it != hotkeys.constEnd(); ++it)
        h[it.key()] = it.value();

    QJsonObject root;
    root["bar"] = b;
    root["theme"] = t;
    root["layout"] = l;
    root["modules"] = m;
    root["hotkeys"] = h;

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) return false;
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    f.close();
    return true;
}

bool Config::moduleEnabled(const QString& id) const {
    auto it = modules.find(id);
    if (it == modules.end()) return true; // default enabled
    return it.value().value("enabled").toBool(true);
}

QJsonObject Config::moduleConfig(const QString& id) const {
    auto it = modules.find(id);
    if (it == modules.end()) return QJsonObject();
    return it.value();
}
