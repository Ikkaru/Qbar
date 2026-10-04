#pragma once
#include <QObject>
#include <QQmlContext>
#include <QJsonArray>
#include <QJsonObject>
#include "core/config.h"

class ModuleRegistry : public QObject {
    Q_OBJECT
    Q_PROPERTY(QJsonArray leftModules READ leftModules NOTIFY layoutChanged)
    Q_PROPERTY(QJsonArray centerModules READ centerModules NOTIFY layoutChanged)
    Q_PROPERTY(QJsonArray rightModules READ rightModules NOTIFY layoutChanged)

public:
    explicit ModuleRegistry(QObject* parent = nullptr) : QObject(parent) {}

    void setConfig(Config* config) { m_config = config; rebuild(); }
    void rebuild();

    QJsonArray leftModules() const { return m_left; }
    QJsonArray centerModules() const { return m_center; }
    QJsonArray rightModules() const { return m_right; }

    Q_INVOKABLE QString qmlUrlFor(const QString& id) const;

signals:
    void layoutChanged();

private:
    Config* m_config = nullptr;
    QJsonArray m_left;
    QJsonArray m_center;
    QJsonArray m_right;
};
