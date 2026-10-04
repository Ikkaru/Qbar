#pragma once
#include <QObject>
#include <QStringList>
#include <QAbstractListModel>

class WorkspacesService : public QObject {
    Q_OBJECT
    Q_PROPERTY(int activeIndex READ activeIndex NOTIFY activeIndexChanged)
    Q_PROPERTY(int count READ count NOTIFY workspacesChanged)

public:
    explicit WorkspacesService(QObject* parent = nullptr);

    int activeIndex() const { return m_activeIndex; }
    int count() const { return m_names.size(); }

    Q_INVOKABLE QString nameAt(int index) const;
    Q_INVOKABLE void switchTo(int index);

signals:
    void activeIndexChanged();
    void workspacesChanged();

private:
    QStringList m_names;
    int m_activeIndex = 0;
};
