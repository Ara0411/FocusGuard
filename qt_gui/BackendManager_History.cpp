#include "BackendManager.h"
#include <QFile>
#include <QSaveFile>
#include <QDate>
#include <QTime>
#include <QDateTime>
#include <QDebug>
#include <nlohmann/json.hpp>
#include <fstream>
#include <algorithm>

using json = nlohmann::json;

// =============================================================================
//  장기 데이터 누적 보관 엔진 (history_daily.json)
// =============================================================================

void BackendManager::ensureTodayRecord()
{
    QString curDate = QDate::currentDate().toString("yyyy-MM-dd");
    if (m_todayDateStr != curDate) {
        m_todayDateStr = curDate;
        if (m_dailyHistory.contains(curDate)) {
            const auto &rec = m_dailyHistory[curDate];
            m_sessionStartFocusSecs       = rec.focusSecs;
            m_sessionStartBlockedAttempts = rec.blockedAttempts;
            m_sessionStartBlockedSessions = rec.blockedSessions;
            m_sessionStartPomodoroRounds  = rec.pomodoroRounds;
        } else {
            DailyRecord newRec;
            m_dailyHistory[curDate] = newRec;
            m_sessionStartFocusSecs       = 0;
            m_sessionStartBlockedAttempts = 0;
            m_sessionStartBlockedSessions = 0;
            m_sessionStartPomodoroRounds  = 0;
        }
    }
}

void BackendManager::loadHistory()
{
    m_dailyHistory.clear();
    std::ifstream file(m_historyFilePath.toStdString());
    if (!file.is_open()) {
        qDebug() << "[BackendManager] No existing history_daily.json found. Initializing fresh record.";
        ensureTodayRecord();
        refreshWeeklyMinutesFromHistory();
        return;
    }

    try {
        json j;
        file >> j;
        if (j.contains("days") && j["days"].is_object()) {
            for (auto &item : j["days"].items()) {
                QString dateStr = QString::fromStdString(item.key());
                const auto &val = item.value();
                DailyRecord rec;
                rec.focusSecs       = val.value("focusSecs", 0);
                rec.blockedAttempts = val.value("blockedAttempts", 0);
                rec.blockedSessions = val.value("blockedSessions", 0);
                rec.pomodoroRounds  = val.value("pomodoroRounds", 0);

                if (val.contains("hourlyFocusSecs") && val["hourlyFocusSecs"].is_array()) {
                    int idx = 0;
                    for (auto &h : val["hourlyFocusSecs"]) {
                        if (idx < 24) rec.hourlyFocusSecs[idx] = h.get<int>();
                        idx++;
                    }
                }
                if (val.contains("hourlyBlocks") && val["hourlyBlocks"].is_array()) {
                    int idx = 0;
                    for (auto &h : val["hourlyBlocks"]) {
                        if (idx < 24) rec.hourlyBlocks[idx] = h.get<int>();
                        idx++;
                    }
                }
                if (val.contains("topBlockedDomains") && val["topBlockedDomains"].is_object()) {
                    for (auto &dItem : val["topBlockedDomains"].items()) {
                        rec.topBlockedDomains[QString::fromStdString(dItem.key())] = dItem.value().get<int>();
                    }
                }
                m_dailyHistory[dateStr] = rec;
            }
        }
        ensureTodayRecord();
        refreshWeeklyMinutesFromHistory();
        emit historyDataChanged();
        qDebug() << "[BackendManager] Loaded history with" << m_dailyHistory.size() << "days of records.";
    } catch (const std::exception &e) {
        qWarning() << "[BackendManager] Failed to parse history_daily.json:" << e.what();
        file.close();
        QFile::copy(m_historyFilePath, m_historyFilePath + ".corrupted");
        ensureTodayRecord();
        refreshWeeklyMinutesFromHistory();
    }
}

void BackendManager::updateTodayRecordFromSession()
{
    ensureTodayRecord();
    if (m_todayDateStr.isEmpty()) return;

    auto &rec = m_dailyHistory[m_todayDateStr];
    rec.focusSecs       = m_sessionStartFocusSecs + m_elapsedSecs;
    rec.blockedAttempts = m_sessionStartBlockedAttempts + m_blockedAttempts;
    rec.blockedSessions = m_sessionStartBlockedSessions + m_blockedSessions;
    rec.pomodoroRounds  = qMax(rec.pomodoroRounds, m_sessionStartPomodoroRounds + m_pomodoroRound);

    int curHour = QTime::currentTime().hour();
    if (curHour >= 0 && curHour < 24) {
        rec.hourlyFocusSecs[curHour] = qMax(rec.hourlyFocusSecs[curHour], m_elapsedSecs % 3600);
        rec.hourlyBlocks[curHour]    = qMax(rec.hourlyBlocks[curHour], m_blockedSessions);
    }

    if (m_topDomain != "없음" && !m_topDomain.isEmpty() && m_topDomainCount > 0) {
        rec.topBlockedDomains[m_topDomain] = qMax(rec.topBlockedDomains.value(m_topDomain, 0), m_topDomainCount);
    }

    for (const auto &item : m_topBlockedDomains) {
        QVariantMap map = item.toMap();
        QString domain = map.value("domain").toString();
        int count = map.value("count").toInt();
        if (!domain.isEmpty() && count > 0) {
            rec.topBlockedDomains[domain] = qMax(rec.topBlockedDomains.value(domain, 0), count);
        }
    }
}

void BackendManager::saveHistory()
{
    updateTodayRecordFromSession();

    json rootJ;
    rootJ["version"] = 1;
    rootJ["lastUpdated"] = QDateTime::currentDateTime().toString(Qt::ISODate).toStdString();

    long long totalSecs = 0;
    json daysJ = json::object();
    for (auto it = m_dailyHistory.begin(); it != m_dailyHistory.end(); ++it) {
        const QString &dStr = it.key();
        const DailyRecord &rec = it.value();
        totalSecs += rec.focusSecs;

        json dObj;
        dObj["focusSecs"]       = rec.focusSecs;
        dObj["blockedAttempts"] = rec.blockedAttempts;
        dObj["blockedSessions"] = rec.blockedSessions;
        dObj["pomodoroRounds"]  = rec.pomodoroRounds;

        json hFocus = json::array();
        for (int v : rec.hourlyFocusSecs) hFocus.push_back(v);
        dObj["hourlyFocusSecs"] = hFocus;

        json hBlocks = json::array();
        for (int v : rec.hourlyBlocks) hBlocks.push_back(v);
        dObj["hourlyBlocks"] = hBlocks;

        json topD = json::object();
        for (auto dit = rec.topBlockedDomains.begin(); dit != rec.topBlockedDomains.end(); ++dit) {
            topD[dit.key().toStdString()] = dit.value();
        }
        dObj["topBlockedDomains"] = topD;

        daysJ[dStr.toStdString()] = dObj;
    }

    rootJ["totalAccumulatedSecs"] = totalSecs;
    rootJ["days"] = daysJ;

    QSaveFile sFile(m_historyFilePath);
    if (sFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        std::string dumped = rootJ.dump(2);
        sFile.write(dumped.data(), (qint64)dumped.size());
        if (sFile.commit()) {
            refreshWeeklyMinutesFromHistory();
            emit historyDataChanged();
        } else {
            qWarning() << "[BackendManager] Failed to commit history_daily.json save file.";
        }
    } else {
        qWarning() << "[BackendManager] Cannot open QSaveFile for history_daily.json.";
    }
}

void BackendManager::refreshWeeklyMinutesFromHistory()
{
    QDate today = QDate::currentDate();
    int dayOfWeek = today.dayOfWeek(); // 1 = Monday, 7 = Sunday
    QDate monday = today.addDays(-(dayOfWeek - 1));

    while (m_weeklyMinutes.size() < 7) {
        m_weeklyMinutes.append(0);
    }

    bool changed = false;
    for (int i = 0; i < 7; ++i) {
        QDate d = monday.addDays(i);
        QString ds = d.toString("yyyy-MM-dd");
        int mins = 0;
        if (m_dailyHistory.contains(ds)) {
            mins = m_dailyHistory[ds].focusSecs / 60;
        }
        if (m_weeklyMinutes[i].toInt() != mins) {
            m_weeklyMinutes[i] = mins;
            changed = true;
        }
    }

    if (changed) {
        emit weeklyMinutesChanged();
    }
}

int BackendManager::totalAccumulatedMinutes() const
{
    long long totalSecs = 0;
    for (const auto &d : m_dailyHistory) {
        totalSecs += d.focusSecs;
    }
    return (int)(totalSecs / 60);
}

int BackendManager::totalAccumulatedSessions() const
{
    int total = 0;
    for (const auto &d : m_dailyHistory) {
        total += d.blockedSessions;
    }
    return total;
}

int BackendManager::totalAccumulatedRounds() const
{
    int total = 0;
    for (const auto &d : m_dailyHistory) {
        total += d.pomodoroRounds;
    }
    return total;
}

QVariantMap BackendManager::getDailyStats(const QString &dateStr)
{
    QVariantMap res;
    if (m_dailyHistory.contains(dateStr)) {
        const auto &rec = m_dailyHistory[dateStr];
        res["focusSecs"]       = rec.focusSecs;
        res["focusMinutes"]    = rec.focusSecs / 60;
        res["blockedAttempts"] = rec.blockedAttempts;
        res["blockedSessions"] = rec.blockedSessions;
        res["pomodoroRounds"]  = rec.pomodoroRounds;

        QVariantList hFocus;
        for (int v : rec.hourlyFocusSecs) hFocus.append(v);
        res["hourlyFocusSecs"] = hFocus;

        QVariantList hBlocks;
        for (int v : rec.hourlyBlocks) hBlocks.append(v);
        res["hourlyBlocks"] = hBlocks;

        QVariantMap topD;
        for (auto it = rec.topBlockedDomains.begin(); it != rec.topBlockedDomains.end(); ++it) {
            topD[it.key()] = it.value();
        }
        res["topBlockedDomains"] = topD;
    }
    return res;
}

QVariantList BackendManager::getWeeklyMinutesFromHistory()
{
    refreshWeeklyMinutesFromHistory();
    return m_weeklyMinutes;
}

QVariantList BackendManager::getTrendRange(int daysCount)
{
    updateTodayRecordFromSession();
    int count = qBound(1, daysCount, 90);
    QDate today = QDate::currentDate();
    QVariantList res;

    for (int i = count - 1; i >= 0; --i) {
        QDate d = today.addDays(-i);
        QString ds = d.toString("yyyy-MM-dd");
        QVariantMap item;
        item["fullDate"] = ds;
        item["date"] = d.toString("MM/dd");

        QString wName = "월";
        switch (d.dayOfWeek()) {
        case 1: wName = "월"; break;
        case 2: wName = "화"; break;
        case 3: wName = "수"; break;
        case 4: wName = "목"; break;
        case 5: wName = "금"; break;
        case 6: wName = "토"; break;
        case 7: wName = "일"; break;
        }
        item["weekday"] = wName;

        int mins = 0;
        int blocks = 0;
        int rounds = 0;
        if (m_dailyHistory.contains(ds)) {
            const auto &rec = m_dailyHistory[ds];
            mins = rec.focusSecs / 60;
            blocks = rec.blockedSessions;
            rounds = rec.pomodoroRounds;
        }
        item["minutes"] = mins;
        item["blocks"]  = blocks;
        item["rounds"]  = rounds;

        res.append(item);
    }
    return res;
}

QVariantList BackendManager::getTopBlockedDomainsHistorical(int daysCount, int limit)
{
    updateTodayRecordFromSession();
    int count = qBound(1, daysCount, 90);
    QDate today = QDate::currentDate();
    QMap<QString, int> agg;

    for (int i = 0; i < count; ++i) {
        QString ds = today.addDays(-i).toString("yyyy-MM-dd");
        if (m_dailyHistory.contains(ds)) {
            const auto &rec = m_dailyHistory[ds];
            for (auto it = rec.topBlockedDomains.begin(); it != rec.topBlockedDomains.end(); ++it) {
                agg[it.key()] += it.value();
            }
        }
    }

    // 카운트 기준 내림차순 정렬
    QList<QPair<int, QString>> sortedList;
    int grandTotal = 0;
    for (auto it = agg.begin(); it != agg.end(); ++it) {
        if (it.value() > 0) {
            sortedList.append(qMakePair(it.value(), it.key()));
            grandTotal += it.value();
        }
    }

    std::sort(sortedList.begin(), sortedList.end(), [](const QPair<int, QString> &a, const QPair<int, QString> &b) {
        return a.first > b.first;
    });

    QStringList palette = { "#F43F5E", "#8B5CF6", "#3B82F6", "#10B981", "#F59E0B" };
    QVariantList res;
    int takeCount = qMin((int)sortedList.size(), qMax(1, limit));
    for (int i = 0; i < takeCount; ++i) {
        QVariantMap item;
        item["name"]    = sortedList[i].second;
        item["count"]   = sortedList[i].first;
        item["percent"] = (grandTotal > 0) ? (sortedList[i].first * 100 / grandTotal) : 0;
        item["color"]   = palette[i % palette.size()];
        res.append(item);
    }
    return res;
}

QVariantList BackendManager::getWeekdayAverages()
{
    updateTodayRecordFromSession();
    int totalMins[7] = {0};
    int totalBlks[7] = {0};
    int dayCounts[7] = {0};

    for (auto it = m_dailyHistory.begin(); it != m_dailyHistory.end(); ++it) {
        QDate d = QDate::fromString(it.key(), "yyyy-MM-dd");
        if (d.isValid()) {
            int dIdx = d.dayOfWeek() - 1; // 0..6
            if (dIdx >= 0 && dIdx < 7) {
                totalMins[dIdx] += it.value().focusSecs / 60;
                totalBlks[dIdx] += it.value().blockedSessions;
                dayCounts[dIdx]++;
            }
        }
    }

    QStringList wNames = { "월", "화", "수", "목", "금", "토", "일" };
    QVariantList res;
    for (int i = 0; i < 7; ++i) {
        QVariantMap item;
        item["dayName"]    = wNames[i];
        item["avgMinutes"] = (dayCounts[i] > 0) ? (totalMins[i] / dayCounts[i]) : 0;
        item["totalBlocks"]= totalBlks[i];
        item["sampleDays"] = dayCounts[i];
        res.append(item);
    }
    return res;
}

int BackendManager::getHeatValue(int dayIdx, int hourIdx)
{
    if (dayIdx < 0 || dayIdx >= 7 || hourIdx < 0 || hourIdx >= 24) return 0;
    QDate today = QDate::currentDate();
    int todayDayOfWeek = today.dayOfWeek(); // 1..7 (1=Mon)
    int todayIdx = todayDayOfWeek - 1;       // 0..6

    // 미래 요일은 0 반환
    if (dayIdx > todayIdx) return 0;

    QDate targetDate = today.addDays(dayIdx - todayIdx);
    QString ds = targetDate.toString("yyyy-MM-dd");

    if (m_dailyHistory.contains(ds)) {
        const auto &rec = m_dailyHistory[ds];
        int secs = rec.hourlyFocusSecs[hourIdx];

        // 오늘 현재 세션의 시간대별 집중 시간 실시간 합산
        if (dayIdx == todayIdx && hourIdx == QTime::currentTime().hour()) {
            secs = qMax(secs, m_elapsedSecs % 3600);
        }

        if (secs <= 0)   return 0;
        if (secs < 900)  return 1; // 15분 미만
        if (secs < 1800) return 2; // 30분 미만
        if (secs < 2700) return 3; // 45분 미만
        return 4;                  // 45분 이상
    }
    return 0;
}

QString BackendManager::getMostProductiveWeekday()
{
    QVariantList avgs = getWeekdayAverages();
    int bestIdx = -1;
    int bestVal = 0;
    for (int i = 0; i < avgs.size(); ++i) {
        int m = avgs[i].toMap().value("avgMinutes").toInt();
        if (m > bestVal) {
            bestVal = m;
            bestIdx = i;
        }
    }
    if (bestIdx >= 0 && bestVal > 0) {
        QStringList wNames = { "월요일", "화요일", "수요일", "목요일", "금요일", "토요일", "일요일" };
        int h = bestVal / 60;
        int min = bestVal % 60;
        return QString("%1 (평균 %2시간 %3분)").arg(wNames[bestIdx]).arg(h).arg(min);
    }
    return "데이터 수집 중";
}

QString BackendManager::getMostDistractedWeekday()
{
    QVariantList avgs = getWeekdayAverages();
    int worstIdx = -1;
    int worstVal = 0;
    for (int i = 0; i < avgs.size(); ++i) {
        int b = avgs[i].toMap().value("totalBlocks").toInt();
        if (b > worstVal) {
            worstVal = b;
            worstIdx = i;
        }
    }
    if (worstIdx >= 0 && worstVal > 0) {
        QStringList wNames = { "월요일", "화요일", "수요일", "목요일", "금요일", "토요일", "일요일" };
        return QString("%1 (총 %2회 차단)").arg(wNames[worstIdx]).arg(worstVal);
    }
    return "데이터 수집 중";
}

void BackendManager::flushHistory()
{
    saveHistory();
}
