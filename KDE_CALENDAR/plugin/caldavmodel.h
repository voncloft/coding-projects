#pragma once

#include <QAbstractListModel>
#include <QDate>
#include <QDateTime>
#include <QNetworkAccessManager>
#include <QStringList>
#include <QUrl>
#include <QVariant>

class CalDavModel : public QAbstractListModel {
    Q_OBJECT

    Q_PROPERTY(QString baseUrl READ baseUrl WRITE setBaseUrl NOTIFY baseUrlChanged)
    Q_PROPERTY(QString username READ username WRITE setUsername NOTIFY usernameChanged)
    Q_PROPERTY(QString password READ password WRITE setPassword NOTIFY passwordChanged)

    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(QDateTime lastUpdated READ lastUpdated NOTIFY lastUpdatedChanged)

public:
    enum Roles {
        SummaryRole = Qt::UserRole + 1,
        StartRole,
        EndRole,
        DescriptionRole,
        EventColorRole,
        StartBucketRole,
        StartEpochRole
    };

    explicit CalDavModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString baseUrl() const { return m_baseUrl; }
    QString username() const { return m_username; }
    QString password() const { return m_password; }

    bool loading() const { return m_loading; }
    QString lastError() const { return m_lastError; }
    QDateTime lastUpdated() const { return m_lastUpdated; }

public slots:
    void setBaseUrl(const QString &u);
    void setUsername(const QString &u);
    void setPassword(const QString &p);

    Q_INVOKABLE void discoverCalendars();
    Q_INVOKABLE void refreshForDayAll(const QDateTime &dayLocal);

    // reliable way for QML to read rows without model.get()
    Q_INVOKABLE QVariantList exportEvents() const;

signals:
    void baseUrlChanged();
    void usernameChanged();
    void passwordChanged();

    void loadingChanged();
    void lastErrorChanged();
    void lastUpdatedChanged();

private:
    struct Event {
        QString uid;
        QString summary;
        QDateTime start;
        QDateTime end;
        QString description;
        QString eventColor;
        QString rrule;   // raw RRULE line (unparsed beyond simple checks)
        bool allDay = false;
    };

    void setLoading(bool v);
    void setLastError(const QString &e);
    void setLastUpdated(const QDateTime &dt);

    QString effectiveUsername() const;
    QString effectivePassword() const;
    QUrl effectiveDiscoveryRoot() const;

    void sendPropfindDiscover(const QUrl &rootUrl);

    void sendReportFetchOne(const QUrl &calendarUrl,
                            const QDateTime &fromUtc,
                            const QDateTime &toUtc,
                            const QString &fallbackColor,
                            quint64 generation);

    // NOTE: we intentionally fetch *all* VEVENTs and filter/expand on the client.
    QByteArray buildReportBody(const QDateTime &fromUtc, const QDateTime &toUtc) const;

    QStringList parseCalendarsFromPropfind(const QUrl &rootUrl, const QByteArray &xmlBytes) const;

    QList<Event> parseMultiStatusXmlForEvents(const QByteArray &xmlBytes,
                                              const QString &fallbackColor) const;
    QList<Event> parseIcs(const QString &icsText, const QString &fallbackColor) const;

    static QString unfoldIcs(const QString &ics);
    static QDateTime parseIcsDateTime(const QString &v, bool *isAllDayOut);

    void beginDayFetch(const QDateTime &dayLocal);
    void maybeStartNextRequests();
    void finishDayFetchIfDone();

    static QString stableColorForUrl(const QString &url);

    static qint64 startEpochSeconds(const Event &e);
    static QString startBucketString(const Event &e);

private:
    QString m_baseUrl;
    QString m_username;
    QString m_password;

    bool m_loading = false;
    QString m_lastError;
    QDateTime m_lastUpdated;

    QNetworkAccessManager m_nam;

    QStringList m_calendarUrls;

    QList<Event> m_events;

    QDate m_targetDate;        // the day we are currently rendering (local date)
    QDateTime m_fetchFromUtc;  // retained for API shape (unused by REPORT body)
    QDateTime m_fetchToUtc;

    int m_inFlight = 0;
    int m_nextIndex = 0;
    QList<Event> m_accumulated;

    int m_maxConcurrent = 4;

    quint64 m_generation = 0;
};
