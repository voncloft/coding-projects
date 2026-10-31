#include "caldavmodel.h"

#include <QNetworkRequest>
#include <QNetworkReply>
#include <QXmlStreamReader>
#include <QTimer>
#include <QSet>
#include <QLoggingCategory>
#include <QDateTime>
#include <QFile>
#include <QTextStream>
#include <QMutex>
#include <QMutexLocker>
#include <QCoreApplication>
#include <algorithm>
#include <limits>

Q_LOGGING_CATEGORY(caldavLog, "caldav")

static constexpr int kTimeoutMs = 5000;
static constexpr int kMaxBytes = 3 * 1024 * 1024;
static const char *kLogPath = "/tmp/kde-log.log";

// ---------- File logger (Qt message handler) ----------
static QMutex g_logMutex;

static const char* msgTypeToStr(QtMsgType type) {
    switch (type) {
        case QtDebugMsg:    return "DEBUG";
        case QtInfoMsg:     return "INFO";
        case QtWarningMsg:  return "WARN";
        case QtCriticalMsg: return "CRIT";
        case QtFatalMsg:    return "FATAL";
        default:            return "LOG";
    }
}

static void fileMessageHandler(QtMsgType type, const QMessageLogContext &ctx, const QString &msg) {
    const QString ts = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");
    const QString cat = ctx.category ? QString::fromUtf8(ctx.category) : QStringLiteral("unknown");

    const QString line = QString("%1 [%2] [%3] %4\n")
        .arg(ts)
        .arg(QString::fromUtf8(msgTypeToStr(type)))
        .arg(cat)
        .arg(msg);

    {
        QMutexLocker lock(&g_logMutex);
        QFile f(QString::fromUtf8(kLogPath));
        if (f.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            QTextStream out(&f);
            out << line;
            out.flush();
            f.close();
        }
    }

    // keep terminal output too
    fprintf(stderr, "%s", line.toUtf8().constData());
    fflush(stderr);

    if (type == QtFatalMsg) abort();
}

static void ensureFileLoggerInstalled() {
    static bool installed = false;
    if (installed) return;
    installed = true;

    qInstallMessageHandler(fileMessageHandler);
    qInfo().noquote() << QString("Logging enabled -> %1").arg(QString::fromUtf8(kLogPath));
}
// ------------------------------------------------------

static QString ensureTrailingSlash(const QString &s) {
    return s.endsWith('/') ? s : (s + "/");
}

static QString dtStr(const QDateTime &dt) {
    if (!dt.isValid()) return "<invalid>";
    return dt.toString(Qt::ISODate) + " (local " + dt.toLocalTime().toString(Qt::ISODate) + ")";
}

// RRULE mini parser
static QHash<QString, QString> parseRRuleMap(const QString &rrule) {
    QHash<QString, QString> m;
    const QStringList parts = rrule.split(';', Qt::SkipEmptyParts);
    for (const QString &p : parts) {
        const int eq = p.indexOf('=');
        if (eq <= 0) continue;
        m.insert(p.left(eq).trimmed().toUpper(), p.mid(eq + 1).trimmed());
    }
    return m;
}

static int parseInterval(const QHash<QString, QString> &rr) {
    bool ok = false;
    const int v = rr.value("INTERVAL").toInt(&ok);
    return (ok && v > 0) ? v : 1;
}

static QDateTime parseUntilLocal(const QHash<QString, QString> &rr) {
    const QString u = rr.value("UNTIL");
    if (u.isEmpty()) return {};

    if (u.endsWith('Z')) {
        QDateTime dt = QDateTime::fromString(u, "yyyyMMdd'T'HHmmss'Z'");
        dt.setTimeSpec(Qt::UTC);
        return dt.toLocalTime();
    }
    QDateTime dt = QDateTime::fromString(u, "yyyyMMdd'T'HHmmss");
    if (!dt.isValid()) dt = QDateTime::fromString(u, "yyyyMMdd'T'HHmm");
    dt.setTimeSpec(Qt::LocalTime);
    return dt;
}

static int wkstToQt(const QString &wkst) {
    const QString s = wkst.trimmed().toUpper();
    if (s == "MO") return Qt::Monday;
    if (s == "TU") return Qt::Tuesday;
    if (s == "WE") return Qt::Wednesday;
    if (s == "TH") return Qt::Thursday;
    if (s == "FR") return Qt::Friday;
    if (s == "SA") return Qt::Saturday;
    if (s == "SU") return Qt::Sunday;
    return Qt::Monday;
}

static int bydayTokenToQt(const QString &tok) {
    QString t = tok.trimmed().toUpper();
    while (!t.isEmpty() && (t[0] == '+' || t[0] == '-' || t[0].isDigit())) t.remove(0, 1);
    if (t == "MO") return Qt::Monday;
    if (t == "TU") return Qt::Tuesday;
    if (t == "WE") return Qt::Wednesday;
    if (t == "TH") return Qt::Thursday;
    if (t == "FR") return Qt::Friday;
    if (t == "SA") return Qt::Saturday;
    if (t == "SU") return Qt::Sunday;
    return 0;
}

static QDate weekStart(const QDate &d, int wkstQtDay) {
    int delta = d.dayOfWeek() - wkstQtDay;
    if (delta < 0) delta += 7;
    return d.addDays(-delta);
}

// -------- CalDavModel --------

CalDavModel::CalDavModel(QObject *parent)
    : QAbstractListModel(parent)
{
    ensureFileLoggerInstalled();
    qCInfo(caldavLog) << "CalDavModel constructed (logging to" << kLogPath << ")";
}

int CalDavModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid()) return 0;
    return m_events.size();
}

qint64 CalDavModel::startEpochSeconds(const Event &e) {
    if (!e.start.isValid()) return std::numeric_limits<qint64>::max();
    return e.start.toUTC().toSecsSinceEpoch();
}

QString CalDavModel::startBucketString(const Event &e) {
    if (!e.start.isValid()) return "Unknown";
    if (e.allDay) return "All day";
    return e.start.toLocalTime().toString("HH:mm");
}

QVariant CalDavModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= m_events.size()) return {};
    const auto &e = m_events.at(index.row());

    switch (role) {
        case SummaryRole: return e.summary;
        case StartRole: return e.start;
        case EndRole: return e.end;
        case DescriptionRole: return e.description;
        case EventColorRole: return e.eventColor;
        case StartBucketRole: return startBucketString(e);
        case StartEpochRole: return startEpochSeconds(e);
        default: return {};
    }
}

QHash<int, QByteArray> CalDavModel::roleNames() const {
    return {
        {SummaryRole, "summary"},
        {StartRole, "start"},
        {EndRole, "end"},
        {DescriptionRole, "description"},
        {EventColorRole, "eventColor"},
        {StartBucketRole, "startBucket"},
        {StartEpochRole, "startEpoch"}
    };
}

QVariantList CalDavModel::exportEvents() const {
    QVariantList out;
    out.reserve(m_events.size());

    for (const auto &e : m_events) {
        QVariantMap m;
        m["summary"] = e.summary;
        m["start"] = e.start;
        m["end"] = e.end;
        m["description"] = e.description;
        m["eventColor"] = e.eventColor;
        m["startBucket"] = startBucketString(e);
        m["startEpoch"] = startEpochSeconds(e);
        out.push_back(m);
    }
    return out;
}

void CalDavModel::setBaseUrl(const QString &u) {
    if (m_baseUrl == u) return;
    m_baseUrl = u;
    qCInfo(caldavLog) << "Base URL set to:" << m_baseUrl;
    emit baseUrlChanged();
}

void CalDavModel::setUsername(const QString &u) {
    if (m_username == u) return;
    m_username = u;
    qCInfo(caldavLog) << "Username set (len=" << m_username.size() << ")";
    emit usernameChanged();
}

void CalDavModel::setPassword(const QString &p) {
    if (m_password == p) return;
    m_password = p;
    qCInfo(caldavLog) << "Password set (len=" << m_password.size() << ")";
    emit passwordChanged();
}

void CalDavModel::setLoading(bool v) {
    if (m_loading == v) return;
    m_loading = v;
    qCInfo(caldavLog) << "Loading =" << m_loading;
    emit loadingChanged();
}

void CalDavModel::setLastError(const QString &e) {
    if (m_lastError == e) return;
    m_lastError = e;
    if (!m_lastError.isEmpty()) qCWarning(caldavLog) << "LastError:" << m_lastError;
    emit lastErrorChanged();
}

void CalDavModel::setLastUpdated(const QDateTime &dt) {
    if (m_lastUpdated == dt) return;
    m_lastUpdated = dt;
    qCInfo(caldavLog) << "LastUpdated:" << dt.toString(Qt::ISODate);
    emit lastUpdatedChanged();
}

QString CalDavModel::effectiveUsername() const {
    if (!m_username.isEmpty()) return m_username;
    return qEnvironmentVariable("RADICALE_USER");
}

QString CalDavModel::effectivePassword() const {
    if (!m_password.isEmpty()) return m_password;
    return qEnvironmentVariable("RADICALE_PASS");
}

QUrl CalDavModel::effectiveDiscoveryRoot() const {
    QString u = m_baseUrl;
    if (u.isEmpty()) u = qEnvironmentVariable("RADICALE_URL");
    QUrl url(u);
    if (!url.isValid() || url.isEmpty()) return {};
    url = QUrl(ensureTrailingSlash(url.toString()));
    return url;
}

QByteArray CalDavModel::buildReportBody(const QDateTime &fromUtc, const QDateTime &toUtc) const {
    Q_UNUSED(fromUtc);
    Q_UNUSED(toUtc);

    const QString body = QString(
R"(<?xml version="1.0" encoding="utf-8" ?>
<c:calendar-query xmlns:d="DAV:" xmlns:c="urn:ietf:params:xml:ns:caldav">
  <d:prop>
    <d:getetag/>
    <c:calendar-data/>
  </d:prop>
  <c:filter>
    <c:comp-filter name="VCALENDAR">
      <c:comp-filter name="VEVENT"/>
    </c:comp-filter>
  </c:filter>
</c:calendar-query>
)");
    return body.toUtf8();
}

QString CalDavModel::unfoldIcs(const QString &ics) {
    QStringList in = ics.split('\n');
    QStringList out;
    for (QString l : in) {
        if (!out.isEmpty() && (l.startsWith(' ') || l.startsWith('\t'))) out.last() += l.mid(1);
        else out.push_back(l);
    }
    return out.join('\n');
}

QDateTime CalDavModel::parseIcsDateTime(const QString &v, bool *isAllDayOut) {
    if (isAllDayOut) *isAllDayOut = false;

    if (v.size() == 8) {
        if (isAllDayOut) *isAllDayOut = true;
        QDate d = QDate::fromString(v, "yyyyMMdd");
        return QDateTime(d, QTime(0,0), Qt::LocalTime);
    }

    if (v.endsWith('Z')) {
        QDateTime dt = QDateTime::fromString(v, "yyyyMMdd'T'HHmmss'Z'");
        dt.setTimeSpec(Qt::UTC);
        return dt.toLocalTime();
    }

    QDateTime dt = QDateTime::fromString(v, "yyyyMMdd'T'HHmmss");
    if (!dt.isValid()) dt = QDateTime::fromString(v, "yyyyMMdd'T'HHmm");
    dt.setTimeSpec(Qt::LocalTime);
    return dt;
}

QList<CalDavModel::Event> CalDavModel::parseIcs(const QString &icsText, const QString &fallbackColor) const {
    const QString u = unfoldIcs(icsText);
    const QStringList lines = u.split('\n', Qt::SkipEmptyParts);

    QString calendarColor = fallbackColor;

    QList<Event> out;
    bool inEvent = false;
    Event cur;

    auto flush = [&]() {
        if (!cur.summary.isEmpty() && cur.start.isValid()) {
            if (cur.eventColor.isEmpty()) cur.eventColor = calendarColor;
            if (!cur.end.isValid()) cur.end = cur.allDay ? cur.start.addDays(1) : cur.start.addSecs(3600);
            out.push_back(cur);
        }
        cur = Event{};
    };

    for (QString rawLine : lines) {
        QString line = rawLine.trimmed();

        if (!inEvent) {
            if (line.startsWith("X-APPLE-CALENDAR-COLOR:")) {
                calendarColor = line.mid(QString("X-APPLE-CALENDAR-COLOR:").size()).trimmed();
            } else if (line.startsWith("COLOR:")) {
                calendarColor = line.mid(QString("COLOR:").size()).trimmed();
            }
        }

        if (line == "BEGIN:VEVENT") { inEvent = true; cur = Event{}; continue; }
        if (line == "END:VEVENT")   { if (inEvent) flush(); inEvent = false; continue; }
        if (!inEvent) continue;

        const int colon = line.indexOf(':');
        if (colon < 0) continue;

        const QString name = line.left(colon).section(';', 0, 0);
        const QString value = line.mid(colon + 1);

        if (name == "UID") cur.uid = value;
        else if (name == "SUMMARY") cur.summary = value;
        else if (name == "DTSTART") {
            bool allDay = false;
            cur.start = parseIcsDateTime(value, &allDay);
            cur.allDay = allDay;
        }
        else if (name == "DTEND") {
            bool dummy = false;
            cur.end = parseIcsDateTime(value, &dummy);
        }
        else if (name == "DESCRIPTION") cur.description = value;
        else if (name == "RRULE") cur.rrule = value;
        else if (name == "COLOR") cur.eventColor = value;
    }

    return out;
}

QList<CalDavModel::Event> CalDavModel::parseMultiStatusXmlForEvents(const QByteArray &xmlBytes, const QString &fallbackColor) const {
    QList<Event> all;
    QXmlStreamReader xr(xmlBytes);
    int calendarDataBlocks = 0;

    while (!xr.atEnd()) {
        xr.readNext();
        if (xr.isStartElement() && xr.name().toString().endsWith("calendar-data")) {
            calendarDataBlocks++;
            const QString ics = xr.readElementText(QXmlStreamReader::IncludeChildElements);
            all.append(parseIcs(ics, fallbackColor));
        }
    }

    if (xr.hasError()) qCWarning(caldavLog) << "XML parse error:" << xr.errorString();

    qCInfo(caldavLog) << "Multi-Status parse: calendar-data blocks =" << calendarDataBlocks
                      << "events parsed =" << all.size();
    return all;
}

QStringList CalDavModel::parseCalendarsFromPropfind(const QUrl &rootUrl, const QByteArray &xmlBytes) const {
    QStringList out;

    QXmlStreamReader xr(xmlBytes);

    QString currentHref;
    bool inResponse = false;
    bool isCalendar = false;

    auto finish = [&]() {
        if (inResponse && isCalendar && !currentHref.isEmpty()) {
            QUrl hrefUrl(currentHref);
            QUrl resolved = hrefUrl.isRelative() ? rootUrl.resolved(hrefUrl) : hrefUrl;

            resolved.setScheme(rootUrl.scheme());
            resolved.setHost(rootUrl.host());
            resolved.setPort(rootUrl.port());

            const QString s = ensureTrailingSlash(resolved.toString());
            if (!out.contains(s)) out.push_back(s);
        }
        currentHref.clear();
        isCalendar = false;
        inResponse = false;
    };

    while (!xr.atEnd()) {
        xr.readNext();
        if (xr.isStartElement()) {
            const QString n = xr.name().toString();
            if (n.endsWith("response")) { finish(); inResponse = true; }
            else if (inResponse && n.endsWith("href")) currentHref = xr.readElementText().trimmed();
            else if (inResponse && n.endsWith("calendar")) isCalendar = true;
        } else if (xr.isEndElement() && xr.name().toString().endsWith("response")) {
            finish();
        }
    }
    finish();

    if (xr.hasError()) qCWarning(caldavLog) << "PROPFIND XML error:" << xr.errorString();

    qCInfo(caldavLog) << "Discovered calendar URLs:" << out;
    return out;
}

void CalDavModel::sendPropfindDiscover(const QUrl &rootUrl) {
    setLastError(QString());

    if (!rootUrl.isValid() || rootUrl.isEmpty()) {
        setLastError("Discovery root invalid (set calendar URL in config, or RADICALE_URL).");
        return;
    }

    qCInfo(caldavLog) << "PROPFIND discovery root:" << rootUrl.toString();

    QNetworkRequest req(rootUrl);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/xml; charset=utf-8");
    req.setRawHeader("Depth", "1");

    const QString user = effectiveUsername();
    const QString pass = effectivePassword();
    if (!user.isEmpty()) {
        const QByteArray userpass = (user + ":" + pass).toUtf8();
        req.setRawHeader("Authorization", "Basic " + userpass.toBase64());
    }

    const QByteArray body =
R"(<?xml version="1.0" encoding="utf-8" ?>
<d:propfind xmlns:d="DAV:" xmlns:c="urn:ietf:params:xml:ns:caldav">
  <d:prop>
    <d:resourcetype/>
  </d:prop>
</d:propfind>
)";

    setLoading(true);
    QNetworkReply *reply = m_nam.sendCustomRequest(req, "PROPFIND", body);

    QTimer *timeout = new QTimer(reply);
    timeout->setSingleShot(true);
    timeout->start(kTimeoutMs);
    connect(timeout, &QTimer::timeout, reply, [reply]() { reply->abort(); });

    connect(reply, &QNetworkReply::finished, this, [this, reply, rootUrl]() {
        setLoading(false);

        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        qCInfo(caldavLog) << "PROPFIND finished status=" << status
                          << "error=" << reply->error() << reply->errorString();

        if (reply->error() != QNetworkReply::NoError) {
            setLastError(QString("Discovery error: %1").arg(reply->errorString()));
            reply->deleteLater();
            return;
        }

        const QByteArray bytes = reply->readAll();
        qCInfo(caldavLog) << "PROPFIND bytes=" << bytes.size();
        reply->deleteLater();

        if (bytes.size() > kMaxBytes) {
            setLastError("Discovery response too large");
            return;
        }

        m_calendarUrls = parseCalendarsFromPropfind(rootUrl, bytes);
        qCInfo(caldavLog) << "Calendar count=" << m_calendarUrls.size();

        if (m_calendarUrls.isEmpty()) {
            setLastError("No CalDAV calendars found under the root URL (Depth:1).");
        }
    });
}

void CalDavModel::discoverCalendars() {
    sendPropfindDiscover(effectiveDiscoveryRoot());
}

void CalDavModel::beginDayFetch(const QDateTime &dayLocal) {
    m_generation++;

    m_targetDate = dayLocal.date();

    const QDateTime startLocal(m_targetDate, QTime(0,0,0), Qt::LocalTime);
    const QDateTime endLocal(m_targetDate.addDays(1), QTime(0,0,0), Qt::LocalTime);

    m_fetchFromUtc = startLocal.toUTC();
    m_fetchToUtc = endLocal.toUTC();

    m_inFlight = 0;
    m_nextIndex = 0;
    m_accumulated.clear();

    setLastError(QString());
    setLoading(true);

    qCInfo(caldavLog) << "Begin day fetch for" << m_targetDate.toString(Qt::ISODate)
                      << "dayStartLocal=" << startLocal.toString(Qt::ISODate)
                      << "dayEndLocal=" << endLocal.toString(Qt::ISODate)
                      << "fromUtc=" << m_fetchFromUtc.toString(Qt::ISODate)
                      << "toUtc=" << m_fetchToUtc.toString(Qt::ISODate)
                      << "calendars=" << m_calendarUrls.size()
                      << "generation=" << m_generation;

    maybeStartNextRequests();
}

// Static member (matches header)
QString CalDavModel::stableColorForUrl(const QString &url) {
    uint h = 2166136261u;
    const auto bytes = url.toUtf8();
    for (auto ch : bytes) h = (h ^ (uint)ch) * 16777619u;

    const int r = 80 + (h & 0x7F);
    const int g = 80 + ((h >> 8) & 0x7F);
    const int b = 80 + ((h >> 16) & 0x7F);

    return QString("#%1%2%3")
        .arg(r, 2, 16, QLatin1Char('0'))
        .arg(g, 2, 16, QLatin1Char('0'))
        .arg(b, 2, 16, QLatin1Char('0'));
}

void CalDavModel::finishDayFetchIfDone() {
    if (m_nextIndex < m_calendarUrls.size()) return;
    if (m_inFlight > 0) return;

    const QDateTime dayStart(m_targetDate, QTime(0,0,0), Qt::LocalTime);
    const QDateTime dayEnd(m_targetDate.addDays(1), QTime(0,0,0), Qt::LocalTime);

    qCInfo(caldavLog) << "Filtering for day window:"
                      << "dayStart=" << dayStart.toString(Qt::ISODate)
                      << "dayEnd=" << dayEnd.toString(Qt::ISODate)
                      << "rawAccumulated=" << m_accumulated.size();

    QList<Event> filtered;
    filtered.reserve(m_accumulated.size());
    QSet<QString> seen;

    auto pushIfUnique = [&](const Event &e, int &droppedDedup, int &kept) {
        const QString key = e.uid + "|" + e.start.toUTC().toString(Qt::ISODate) + "|" + e.summary;
        if (seen.contains(key)) { droppedDedup++; return; }
        seen.insert(key);
        filtered.push_back(e);
        kept++;
    };

    auto durationSecs = [](const Event &src) -> qint64 {
        if (src.start.isValid() && src.end.isValid()) {
            const qint64 d = src.start.secsTo(src.end);
            if (d > 0) return d;
        }
        return src.allDay ? 86400 : 3600;
    };

    auto occursOnDay = [&](const Event &src, Event *occOut) -> bool {
        if (src.rrule.trimmed().isEmpty()) return false;
        if (!src.start.isValid()) return false;

        const auto rr = parseRRuleMap(src.rrule);
        const QString freq = rr.value("FREQ").trimmed().toUpper();
        if (freq.isEmpty()) return false;

        const int interval = parseInterval(rr);
        const QDateTime untilLocal = parseUntilLocal(rr);

        auto withinUntil = [&](const QDateTime &occStartLocal) -> bool {
            if (!untilLocal.isValid()) return true;
            return occStartLocal <= untilLocal;
        };

        auto buildOccurrence = [&](const QTime &t) {
            Event occ = src;
            if (src.allDay) {
                occ.start = QDateTime(m_targetDate, QTime(0,0,0), Qt::LocalTime);
                occ.end = occ.start.addDays(1);
            } else {
                occ.start = QDateTime(m_targetDate, t, Qt::LocalTime);
                occ.end = occ.start.addSecs(durationSecs(src));
            }
            *occOut = occ;
        };

        const QDate dt0 = src.start.date();

        if (freq == "YEARLY") {
            if (dt0.month() != m_targetDate.month() || dt0.day() != m_targetDate.day()) return false;
            buildOccurrence(src.start.time());
            return withinUntil(occOut->start);
        }

        if (freq == "DAILY") {
            const int days = dt0.daysTo(m_targetDate);
            if (days < 0) return false;
            if ((days % interval) != 0) return false;
            buildOccurrence(src.start.time());
            return withinUntil(occOut->start);
        }

        if (freq == "WEEKLY") {
            QSet<int> allowedDays;
            const QString byday = rr.value("BYDAY").trimmed();
            if (!byday.isEmpty()) {
                const QStringList toks = byday.split(',', Qt::SkipEmptyParts);
                for (const QString &t : toks) {
                    const int d = bydayTokenToQt(t);
                    if (d != 0) allowedDays.insert(d);
                }
            }
            if (allowedDays.isEmpty()) allowedDays.insert(dt0.dayOfWeek());

            if (!allowedDays.contains(m_targetDate.dayOfWeek())) return false;

            const int wkst = wkstToQt(rr.value("WKST", "MO"));
            const QDate w0 = weekStart(dt0, wkst);
            const QDate wT = weekStart(m_targetDate, wkst);
            const int weeks = w0.daysTo(wT) / 7;
            if (weeks < 0) return false;
            if ((weeks % interval) != 0) return false;

            buildOccurrence(src.start.time());
            return withinUntil(occOut->start);
        }

        if (freq == "MONTHLY") {
            QSet<int> mdays;
            const QString bymd = rr.value("BYMONTHDAY").trimmed();
            if (!bymd.isEmpty()) {
                const QStringList toks = bymd.split(',', Qt::SkipEmptyParts);
                for (const QString &t : toks) {
                    bool ok = false;
                    const int v = t.trimmed().toInt(&ok);
                    if (ok) mdays.insert(v);
                }
            }
            if (mdays.isEmpty()) mdays.insert(dt0.day());

            if (!mdays.contains(m_targetDate.day())) return false;

            const int months = (m_targetDate.year() - dt0.year()) * 12 + (m_targetDate.month() - dt0.month());
            if (months < 0) return false;
            if ((months % interval) != 0) return false;

            buildOccurrence(src.start.time());
            return withinUntil(occOut->start);
        }

        return false;
    };

    int kept = 0;
    int droppedDedup = 0;
    int droppedOverlap = 0;

    int recurringMasters = 0;
    int recurringExpanded = 0;
    int recurringNoOccToday = 0;

    for (const auto &src : m_accumulated) {
        if (!src.rrule.trimmed().isEmpty()) {
            recurringMasters++;
            Event occ;
            if (occursOnDay(src, &occ)) {
                recurringExpanded++;
                pushIfUnique(occ, droppedDedup, kept);
            } else {
                recurringNoOccToday++;
            }
            continue;
        }

        Event e = src;
        if (!e.end.isValid()) e.end = e.allDay ? e.start.addDays(1) : e.start.addSecs(3600);

        if (e.start < dayEnd && e.end > dayStart) {
            if (e.allDay) {
                e.start = QDateTime(e.start.date(), QTime(0,0,0), Qt::LocalTime);
                e.end = e.start.addDays(1);
            }
            pushIfUnique(e, droppedDedup, kept);
        } else {
            droppedOverlap++;
        }
    }

    qCInfo(caldavLog) << "Filter results:"
                      << "kept=" << kept
                      << "droppedOverlap=" << droppedOverlap
                      << "droppedDedup=" << droppedDedup
                      << "recurringMasters=" << recurringMasters
                      << "recurringExpanded=" << recurringExpanded
                      << "recurringNoOccToday=" << recurringNoOccToday;

    if (kept == 0 && !m_accumulated.isEmpty()) {
        qCWarning(caldavLog) << "No events kept for day" << m_targetDate.toString(Qt::ISODate)
                             << "- dumping up to 10 raw events:";
        const int n = std::min<int>(10, m_accumulated.size());
        for (int i = 0; i < n; i++) {
            const auto &e = m_accumulated.at(i);
            qCWarning(caldavLog) << "RAW[" << i << "]"
                                 << "summary=" << e.summary
                                 << "uid=" << e.uid
                                 << "allDay=" << e.allDay
                                 << "start=" << dtStr(e.start)
                                 << "end=" << dtStr(e.end)
                                 << "rrule=" << e.rrule;
        }
    }

    beginResetModel();
    m_events = filtered;
    endResetModel();

    qCInfo(caldavLog) << "Model updated rowCount=" << m_events.size();

    setLastUpdated(QDateTime::currentDateTime());
    setLoading(false);
}

void CalDavModel::maybeStartNextRequests() {
    while (m_inFlight < m_maxConcurrent && m_nextIndex < m_calendarUrls.size()) {
        const QString urlStr = m_calendarUrls.at(m_nextIndex++);
        const QString fallbackColor = CalDavModel::stableColorForUrl(urlStr);

        qCInfo(caldavLog) << "Starting REPORT for" << urlStr
                          << "inFlight=" << (m_inFlight + 1)
                          << "nextIndex=" << m_nextIndex << "/" << m_calendarUrls.size();

        sendReportFetchOne(QUrl(urlStr), m_fetchFromUtc, m_fetchToUtc, fallbackColor, m_generation);
        m_inFlight++;
    }
    finishDayFetchIfDone();
}

void CalDavModel::sendReportFetchOne(const QUrl &calendarUrl,
                                    const QDateTime &fromUtc,
                                    const QDateTime &toUtc,
                                    const QString &fallbackColor,
                                    quint64 generation)
{
    QNetworkRequest req(calendarUrl);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/xml; charset=utf-8");
    req.setRawHeader("Depth", "1");

    const QString user = effectiveUsername();
    const QString pass = effectivePassword();
    if (!user.isEmpty()) {
        const QByteArray userpass = (user + ":" + pass).toUtf8();
        req.setRawHeader("Authorization", "Basic " + userpass.toBase64());
    }

    Q_UNUSED(fromUtc);
    Q_UNUSED(toUtc);

    const QByteArray body = buildReportBody(fromUtc, toUtc);

    QNetworkReply *reply = m_nam.sendCustomRequest(req, "REPORT", body);

    QTimer *timeout = new QTimer(reply);
    timeout->setSingleShot(true);
    timeout->start(kTimeoutMs);
    connect(timeout, &QTimer::timeout, reply, [reply]() { reply->abort(); });

    connect(reply, &QNetworkReply::finished, this, [this, reply, fallbackColor, generation, calendarUrl]() {
        m_inFlight--;

        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray bytes = (reply->error() == QNetworkReply::NoError) ? reply->readAll() : QByteArray();

        qCInfo(caldavLog) << "REPORT finished url=" << calendarUrl.toString()
                          << "status=" << status
                          << "error=" << reply->error() << reply->errorString()
                          << "bytes=" << bytes.size()
                          << "generation=" << generation << "currentGeneration=" << m_generation;

        if (generation != m_generation) {
            qCWarning(caldavLog) << "Stale generation; discarding REPORT result";
            reply->deleteLater();
            maybeStartNextRequests();
            return;
        }

        if (reply->error() == QNetworkReply::NoError) {
            if (bytes.size() > kMaxBytes) {
                qCWarning(caldavLog) << "REPORT response too large; dropping";
            } else if (bytes.isEmpty()) {
                qCWarning(caldavLog) << "REPORT returned 0 bytes (no data?)";
            } else {
                const int before = m_accumulated.size();
                const auto parsed = parseMultiStatusXmlForEvents(bytes, fallbackColor);
                m_accumulated.append(parsed);
                qCInfo(caldavLog) << "Accumulated events:" << before << "->" << m_accumulated.size()
                                  << "(added" << parsed.size() << ")";
            }
        } else {
            qCWarning(caldavLog) << "REPORT error; no events added";
        }

        reply->deleteLater();
        maybeStartNextRequests();
    });
}

void CalDavModel::refreshForDayAll(const QDateTime &dayLocal) {
    qCInfo(caldavLog) << "refreshForDayAll called dayLocal=" << dayLocal.toString(Qt::ISODate);

    if (m_calendarUrls.isEmpty()) {
        qCWarning(caldavLog) << "No calendars discovered yet; kicking discovery";
        discoverCalendars();
        setLastError("No calendars discovered yet — trying discovery. Click the day again in a moment.");
        return;
    }
    beginDayFetch(dayLocal);
}
