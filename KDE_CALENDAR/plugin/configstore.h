#pragma once

#include <QObject>
#include <QVariantMap>

class ConfigStore : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString path READ path CONSTANT)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

public:
    explicit ConfigStore(QObject *parent = nullptr);

    QString path() const;
    QString lastError() const { return m_lastError; }

    Q_INVOKABLE QVariantMap load(); // {url,user,pass}
    Q_INVOKABLE bool save(const QString &url, const QString &user, const QString &pass);
    Q_INVOKABLE bool clear();

signals:
    void lastErrorChanged();

private:
    void setLastError(const QString &e);
    QString m_lastError;
};
