#pragma once
#include <QObject>
#include <QString>

class WorkspacesService;

class WindowStatusService : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString appName READ appName NOTIFY changed)
    Q_PROPERTY(QString windowTitle READ windowTitle NOTIFY changed)
    Q_PROPERTY(bool isDesktop READ isDesktop NOTIFY changed)

public:
    explicit WindowStatusService(QObject* parent = nullptr);

    void setWorkspaces(WorkspacesService* ws) { m_workspaces = ws; }

    QString appName() const { return m_appName; }
    QString windowTitle() const { return m_windowTitle; }
    bool isDesktop() const { return m_isDesktop; }

public slots:
    void refresh();

signals:
    void changed();

private:
    QString m_appName;
    QString m_windowTitle;
    bool m_isDesktop = false;
    WorkspacesService* m_workspaces = nullptr;
};