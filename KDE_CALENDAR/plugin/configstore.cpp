#include "configstore.h"

#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QTextStream>

static QString homeFilePath() {
    return QDir::homePath() + "/kde-cal";
}

ConfigStore::ConfigStore(QObject *parent) : QObject(parent) {}

QString ConfigStore::path() const {
    return homeFilePath();
}

void ConfigStore::setLastError(const QString &e) {
    if (m_lastError == e) return;
    m_lastError = e;
    emit lastErrorChanged();
}

QVariantMap ConfigStore::load() {
    setLastError(QString());

    QVariantMap out;
    out["url"] = "";
    out["user"] = "";
    out["pass"] = "";

    QFile f(homeFilePath());
    if (!f.exists()) return out;

    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        setLastError("Cannot open " + homeFilePath() + ": " + f.errorString());
        return out;
    }

    QTextStream in(&f);
    while (!in.atEnd()) {
        const QString line = in.readLine().trimmed();
        if (line.isEmpty()) continue;
        if (line.startsWith('#')) continue;

        const int eq = line.indexOf('=');
        if (eq <= 0) continue;

        const QString key = line.left(eq).trimmed();
        const QString val = line.mid(eq + 1); // keep as-is

        if (key == "url") out["url"] = val.trimmed();
        else if (key == "user") out["user"] = val.trimmed();
        else if (key == "pass") out["pass"] = val; // don't trim passwords
    }

    return out;
}

bool ConfigStore::save(const QString &url, const QString &user, const QString &pass) {
    setLastError(QString());

    QSaveFile f(homeFilePath());
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        setLastError("Cannot write " + homeFilePath() + ": " + f.errorString());
        return false;
    }

    QTextStream out(&f);
    out << "# kde-cal (Radicale Agenda)\n";
    out << "url=" << url.trimmed() << "\n";
    out << "user=" << user.trimmed() << "\n";
    out << "pass=" << pass << "\n";

    if (!f.commit()) {
        setLastError("Commit failed for " + homeFilePath() + ": " + f.errorString());
        return false;
    }

    // Best-effort permission lock (owner read/write only)
    QFile::setPermissions(homeFilePath(),
                          QFileDevice::ReadOwner | QFileDevice::WriteOwner);

    return true;
}

bool ConfigStore::clear() {
    setLastError(QString());
    QFile f(homeFilePath());
    if (!f.exists()) return true;
    if (!f.remove()) {
        setLastError("Cannot remove " + homeFilePath() + ": " + f.errorString());
        return false;
    }
    return true;
}
