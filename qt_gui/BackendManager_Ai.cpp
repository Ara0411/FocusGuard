#include "BackendManager.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QDebug>
#include <algorithm>

// =============================================================================
//  Gemini AI 인터넷 몰입 유형 분석 및 프롬프트 빌더 (REST API)
// =============================================================================

void BackendManager::setGeminiApiKey(const QString &key)
{
    QString trimmed = key.trimmed();
    if (m_geminiApiKey != trimmed) {
        m_geminiApiKey = trimmed;
        emit geminiApiKeyChanged();
        saveSettings();
    }
}

QString BackendManager::maskedGeminiApiKey() const
{
    if (m_geminiApiKey.isEmpty()) return "미설정";
    if (m_geminiApiKey.length() <= 8) return "******";
    return m_geminiApiKey.left(4) + "..." + m_geminiApiKey.right(4);
}

void BackendManager::requestAiUserAnalysis()
{
    if (m_isAiAnalyzing) return;

    // 1. 최소 가드레일 조건 검증
    if (!canRequestAiAnalysis()) {
        QString reason = getAiAnalysisBlockReason();
        m_aiUserType = "안내";
        m_aiTypeIcon = "info";
        m_aiTypeColor = "#818CF8";
        m_aiAnalysisReport = reason;
        m_aiActionPlan.clear();
        m_aiActionPlan << "타이머를 시작하고 최소 1분 이상 집중을 진행해보세요."
                       << "유혹 사이트 차단 활동 데이터가 기록되면 AI 정밀 분석이 활성화됩니다.";
        emit aiAnalysisFinished();
        return;
    }

    m_isAiAnalyzing = true;
    emit isAiAnalyzingChanged();

    // 2. 경량 요약본 및 Gemini REST 요청 본문 생성
    QJsonObject digest = generateAiPayloadDigest();
    QJsonObject reqBody = buildGeminiRequestBody(digest);
    QByteArray postData = QJsonDocument(reqBody).toJson(QJsonDocument::Compact);

    // 3. Gemini REST API 엔드포인트 URL (신규 지원 모델: gemini-3.8-flash)
    QString endpoint = QString("https://generativelanguage.googleapis.com/v1beta/models/gemini-3.8-flash:generateContent?key=%1")
                       .arg(m_geminiApiKey);
    QUrl url(endpoint);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    qDebug() << "[BackendManager] Requesting Gemini API (gemini-3.8-flash)...";

    // 4. 비동기 HTTP POST 전송 (UI 블로킹 완전 차단)
    QNetworkReply *reply = m_netManager->post(request, postData);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        m_isAiAnalyzing = false;
        emit isAiAnalyzingChanged();

        if (reply->error() != QNetworkReply::NoError) {
            QString errStr = reply->errorString();
            QByteArray errBytes = reply->readAll();
            qWarning() << "[BackendManager] Gemini API Network Error:" << errStr << errBytes;

            // API 키가 화면 에러 텍스트에 노출되지 않도록 URL 쿼리 파라미터 마스킹
            QString safeErr = errStr;
            int keyIdx = safeErr.indexOf("key=");
            if (keyIdx != -1) {
                int endIdx = safeErr.indexOf('&', keyIdx);
                if (endIdx == -1) endIdx = safeErr.indexOf(' ', keyIdx);
                if (endIdx == -1) endIdx = safeErr.length();
                safeErr.replace(keyIdx, endIdx - keyIdx, "key=******");
            }

            m_aiUserType = "⚠️ AI 진단 실패";
            m_aiTypeIcon = "alert-circle";
            m_aiTypeColor = "#F43F5E";
            m_aiAnalysisReport = QString("Gemini API 호출 중 네트워크 오류가 발생했습니다: %1 (API 키 상태 및 네트워크 연결을 확인해주세요.)").arg(safeErr);
            m_aiActionPlan.clear();
            m_aiActionPlan << "인터넷 연결이 활성화되어 있는지 확인하세요."
                           << "Gemini API 키 권한이 올바른지 확인하세요."
                           << "잠시 후 [AI 진단 실행]을 다시 시도해주세요.";
            emit aiAnalysisFinished();
            return;
        }

        QByteArray respBytes = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(respBytes);
        QJsonObject root = doc.object();

        QJsonArray candidates = root.value("candidates").toArray();
        if (candidates.isEmpty()) {
            qWarning() << "[BackendManager] Gemini response has no candidates:" << respBytes;
            m_aiUserType = "⚠️ 응답 비어있음";
            m_aiTypeIcon = "alert-circle";
            m_aiTypeColor = "#F43F5E";
            m_aiAnalysisReport = "AI 모델로부터 적절한 응답을 수신하지 못했습니다.";
            emit aiAnalysisFinished();
            return;
        }

        QJsonObject cand0 = candidates[0].toObject();
        QJsonObject contentObj = cand0.value("content").toObject();
        QJsonArray parts = contentObj.value("parts").toArray();
        if (parts.isEmpty()) {
            qWarning() << "[BackendManager] Gemini candidates content parts is empty";
            emit aiAnalysisFinished();
            return;
        }

        QString aiText = parts[0].toObject().value("text").toString().trimmed();

        // 마크다운 코드블록 방어 (```json ... ```)
        if (aiText.startsWith("```json")) {
            aiText = aiText.mid(7);
        } else if (aiText.startsWith("```")) {
            aiText = aiText.mid(3);
        }
        if (aiText.endsWith("```")) {
            aiText.chop(3);
        }
        aiText = aiText.trimmed();

        QJsonDocument aiDoc = QJsonDocument::fromJson(aiText.toUtf8());
        if (!aiDoc.isObject()) {
            qWarning() << "[BackendManager] AI response text is not valid JSON:" << aiText;
            m_aiUserType = "⚠️ 응답 해석 실패";
            m_aiTypeIcon = "alert-circle";
            m_aiTypeColor = "#F43F5E";
            m_aiAnalysisReport = "AI 응답을 JSON 객체로 변환할 수 없습니다: " + aiText.left(120);
            emit aiAnalysisFinished();
            return;
        }

        QJsonObject aiRes = aiDoc.object();
        m_aiUserType = aiRes.value("persona_title").toString("🌱 잠재력 발휘형");
        m_aiTypeIcon = aiRes.value("persona_icon").toString("sparkles");
        m_aiTypeColor = aiRes.value("theme_color").toString("#818CF8");
        m_aiAnalysisReport = aiRes.value("core_insight").toString("오늘의 네트워크 몰입 패턴 심층 분석이 완료되었습니다.");

        m_aiActionPlan.clear();
        QJsonArray plans = aiRes.value("action_plans").toArray();
        for (const auto &p : plans) {
            m_aiActionPlan.append(p.toString());
        }

        // 4단계: AI 추천 차단 도메인 수집 및 정제
        m_aiRecommendedDomains.clear();
        QJsonArray recDomains = aiRes.value("recommended_block_domains").toArray();
        for (const auto &d : recDomains) {
            QString domStr = d.toString().trimmed().toLower();
            domStr.remove("https://").remove("http://").remove("*.");
            if (domStr.startsWith("www.")) domStr = domStr.mid(4);
            int slashIdx = domStr.indexOf('/');
            if (slashIdx != -1) domStr = domStr.left(slashIdx);

            if (!domStr.isEmpty() && !m_blacklistDomains.contains(domStr) && !m_aiRecommendedDomains.contains(domStr)) {
                m_aiRecommendedDomains.append(domStr);
            }
        }

        // 만약 AI가 중복된 도메인을 추천했거나 비어있는 경우, 상위 유혹/연관 도메인 기반 스마트 보조 추천 Fallback
        if (m_aiRecommendedDomains.isEmpty()) {
            QStringList fallbacks;
            QString top = m_topDomain.toLower();
            if (top.contains("youtube") || top.contains("googlevideo") || top.contains("gvt2")) {
                fallbacks << "googlevideo.com" << "ytimg.com" << "beacons.gvt2.com";
            } else if (top.contains("instagram") || top.contains("facebook")) {
                fallbacks << "cdninstagram.com" << "threads.net";
            } else if (top.contains("chzzk") || top.contains("naver")) {
                fallbacks << "live-api.chzzk.naver.com" << "apis.naver.com";
            }
            fallbacks << "tiktok.com" << "twitch.tv" << "netflix.com" << "x.com" << "reddit.com";

            for (const QString &fb : fallbacks) {
                if (!m_blacklistDomains.contains(fb) && !m_aiRecommendedDomains.contains(fb)) {
                    m_aiRecommendedDomains.append(fb);
                    if (m_aiRecommendedDomains.size() >= 2) break;
                }
            }
        }

        if (!m_aiRecommendedDomains.isEmpty()) {
            qDebug() << "[BackendManager] AI recommended block domains:" << m_aiRecommendedDomains.join(", ");
        }
        emit aiRecommendedDomainsChanged();

        // 쿨다운 대기 시간 (즉시 재실행 가능하도록 0초 유지)
        m_aiCooldownSecs = 0;
        emit aiCooldownSecsChanged();

        qDebug() << "[BackendManager] Gemini AI Analysis Success:" << m_aiUserType;
        emit aiAnalysisFinished();
    });
}

bool BackendManager::canRequestAiAnalysis() const
{
    if (m_geminiApiKey.isEmpty()) return false;
    // 최소 1분 이상 집중했거나 차단 시도 횟수가 1회 이상이어야 분석 가치 있음
    if (m_elapsedSecs < 60 && m_blockedAttempts == 0) return false;
    return true;
}

QString BackendManager::getAiAnalysisBlockReason() const
{
    if (m_geminiApiKey.isEmpty()) {
        return "Gemini API 키가 설정되지 않았습니다. 설정 페이지에서 API 키를 등록해주세요.";
    }
    if (m_elapsedSecs < 60 && m_blockedAttempts == 0) {
        return "최소 1분 이상 집중하거나 유혹 차단 활동 데이터가 수집된 후 정밀 진단이 가능합니다.";
    }
    return QString();
}

QJsonObject BackendManager::generateAiPayloadDigest() const
{
    QJsonObject digest;

    // 1. 세션 메트릭 요약
    QJsonObject sessionMetrics;
    sessionMetrics["elapsed_minutes"] = m_elapsedSecs / 60;

    // 목표 시간(HH:mm:ss) 파싱 -> 분 단위 환산
    int targetMins = 0;
    QStringList tParts = m_targetTime.split(":");
    if (tParts.size() == 3) {
        targetMins = tParts[0].toInt() * 60 + tParts[1].toInt();
    }
    sessionMetrics["target_minutes"] = targetMins;
    sessionMetrics["focus_score"] = m_focusScore;
    sessionMetrics["is_paused"] = m_isPaused;
    sessionMetrics["total_paused_minutes"] = m_totalPausedSecs / 60;
    sessionMetrics["goal_completion_rate"] = targetMins > 0 ? qMin(100.0, ((double)(m_elapsedSecs / 60) / targetMins) * 100.0) : 0.0;
    digest["session_metrics"] = sessionMetrics;

    // 2. 유혹 및 차단 패턴 요약 (의도 세션 및 백그라운드 재시도 버스트 맥락 정제)
    QJsonObject blockingSummary;
    blockingSummary["total_raw_blocked_packets"] = m_blockedAttempts;
    int estSessions = qMax(1, m_blockedSessions);
    blockingSummary["estimated_user_sessions"] = estSessions;

    // 패킷 수 대비 세션 수 차이가 크면(예: 세션당 3개 이상 또는 총 10개 이상) 백그라운드 탭 방치 버스트로 판단
    bool isBurst = (m_blockedAttempts >= 5 && (m_blockedAttempts >= estSessions * 2));
    blockingSummary["is_idle_retry_burst_detected"] = isBurst;
    if (isBurst) {
        blockingSummary["retry_burst_note"] = QString("사용자가 직접 %1회 접속 시도한 것이 아니라, 브라우저 탭/앱이 켜져 있어 백그라운드 재연결(Keep-Alive/Beacon)이 %1회 자동 발생한 패턴입니다.")
                                              .arg(m_blockedAttempts);
    }

    // 첫 이탈/차단 발생 시점 (분)
    int firstTemptationMin = -1;
    if (m_firstBlockTime > m_startTime && m_startTime > 0) {
        firstTemptationMin = (int)((m_firstBlockTime - m_startTime) / 60);
    }
    blockingSummary["first_temptation_elapsed_minute"] = firstTemptationMin;

    // 상위 차단 도메인 (최대 5개) 및 카테고리 집계
    QJsonArray topDomainsArray;
    int videoCount = 0, snsCount = 0, msgCount = 0, gameCount = 0, etcCount = 0;

    int domainLimit = qMin(5, m_topBlockedDomains.size());
    for (int i = 0; i < domainLimit; ++i) {
        QVariantMap m = m_topBlockedDomains[i].toMap();
        QString name = m.value("name").toString().toLower();
        int cnt = m.value("count").toInt();

        QJsonObject dObj;
        dObj["domain"] = name;
        dObj["count"] = cnt;

        QString cat = "other";
        if (name.contains("youtube") || name.contains("googlevideo") || name.contains("tiktok") || name.contains("twitch") || name.contains("netflix") || name.contains("chzzk") || name.contains("gvt2.com")) {
            cat = "video/streaming";
            videoCount += cnt;
        } else if (name.contains("instagram") || name.contains("facebook") || name.contains("twitter") || name.contains("x.com")) {
            cat = "sns";
            snsCount += cnt;
        } else if (name.contains("discord") || name.contains("kakao") || name.contains("telegram")) {
            cat = "messenger";
            msgCount += cnt;
        } else if (name.contains("steam") || name.contains("nexon") || name.contains("riot") || name.contains("blizzard") || name.contains("league")) {
            cat = "game";
            gameCount += cnt;
        } else {
            etcCount += cnt;
        }
        dObj["category"] = cat;
        topDomainsArray.append(dObj);
    }
    blockingSummary["top_blocked_domains"] = topDomainsArray;

    QJsonObject catDist;
    catDist["video"] = videoCount;
    catDist["sns"] = snsCount;
    catDist["messenger"] = msgCount;
    catDist["game"] = gameCount;
    catDist["other"] = etcCount;
    blockingSummary["category_distribution"] = catDist;
    digest["blocking_summary"] = blockingSummary;

    // 3. 시간대별 취약 구간 (타임라인 상위 2개 시간대)
    QJsonArray peakHours;
    QList<QPair<int, int>> hourCounts; // count, hour
    for (int h = 0; h < m_timelineData.size() && h < 24; ++h) {
        int cnt = m_timelineData[h].toInt();
        if (cnt > 0) hourCounts.append(qMakePair(cnt, h));
    }
    std::sort(hourCounts.begin(), hourCounts.end(), [](const QPair<int, int> &a, const QPair<int, int> &b) {
        return a.first > b.first;
    });
    for (int i = 0; i < qMin(2, hourCounts.size()); ++i) {
        peakHours.append(QString("%1시 (%2회 차단)").arg(hourCounts[i].second).arg(hourCounts[i].first));
    }
    digest["temporal_peak_hours"] = peakHours;

    // 4. 모드 및 뽀모도로 설정 맥락
    QJsonObject configContext;
    configContext["block_mode"] = m_blockMode == 0 ? "Strict (Default-Deny)" : "Flexible (Blacklist)";
    configContext["pomodoro_focus_min"] = m_pomodoroFocus;
    configContext["pomodoro_rest_min"] = m_pomodoroRest;
    digest["context"] = configContext;

    // 5. 현재 활성 블랙리스트 목록
    QJsonArray blArray;
    for (const auto &b : m_blacklistDomains) {
        blArray.append(b.toString());
    }
    digest["current_blacklist"] = blArray;

    return digest;
}

QString BackendManager::getAiPayloadDigestJson() const
{
    return QString::fromUtf8(QJsonDocument(generateAiPayloadDigest()).toJson(QJsonDocument::Indented));
}

QJsonObject BackendManager::buildGeminiRequestBody(const QJsonObject &digest) const
{
    QString digestStr = QString::fromUtf8(QJsonDocument(digest).toJson(QJsonDocument::Compact));

    QString prompt = QString(
        "당신은 FocusGuard(네트워크 패킷 기반 집중력 수호 데스크톱 앱)의 전문 '디지털 몰입 인지행동 코치 AI'입니다.\n\n"
        "아래는 사용자가 오늘 진행한 집중 세션의 로컬 요약 통계(JSON Digest)입니다:\n"
        "%1\n\n"
        "[중요: 네트워크 특성 및 딴짓 세션 해석 지침]\n"
        "1. 'total_raw_blocked_packets'는 사용자가 직접 누른 횟수가 아니라, 브라우저의 TCP RST 재연결 루프와 백그라운드 추적 비콘(beacons*.gvt2.com 등)으로 인해 급격히 부풀려질 수 있습니다.\n"
        "2. 실제 사용자가 딴짓을 시도한 횟수는 'estimated_user_sessions'를 기준으로 파악하세요.\n"
        "3. 만약 'is_idle_retry_burst_detected'가 true라면, 사용자가 반복해서 딴짓을 시도한 것이 아니라 탭이나 앱을 닫지 않고 화면 뒤에 켜둔 채 방치한 상태입니다.\n"
        "4. 따라서 사용자를 무리하게 반복적인 도파민 중독자로 비난하지 마시고, '백그라운드에 열려 있는 탭/앱을 작업표시줄이나 브라우저 탭 닫기로 완전히 종료할 것'을 실질적인 처방 중 하나로 안내하세요.\n\n"
        "[분석 다양성 및 심층 진단 지침]\n"
        "- 예시 명칭을 그대로 복사하지 마세요. 실제 차단된 최상위 도메인(유튜브, 치지직, 인스타, 네이버, 게임 등)과 취약 시간대, 집중 경과 시간을 바탕으로 매번 참신하고 개성 있는 페르소나 명칭을 자유롭게 창작하세요.\n"
        "- 진단 결과가 매번 똑같이 나오지 않도록, 사용자의 세션 데이터(집중 지속력, 이탈 사이트의 성격, 시간대별 패턴)에 집중하여 다양한 인지 심리적 관점(주의력 피로, 작업 도피성 검색, 알고리즘 도파민 보상, 루틴 불균형 등)에서 날카롭고 신선한 리포트를 작성하세요.\n"
        "- 실천 계획(action_plans)은 단순한 '탭 닫기' 외에도 카테고리별 맞춤 처방(환경 통제, 인지적 파킹랏 기법, 뽀모도로 인터벌 재설계, 휴식 루틴 등)을 3가지 서로 다른 관점으로 다채롭게 제시하세요.\n\n"
        "반드시 아래 JSON 스키마를 엄격히 준수하여 순수 JSON 객체로만 응답하세요:\n"
        "{\n"
        "  \"persona_title\": \"실제 차단 데이터와 세션 특성을 반영한 참신하고 위트 있는 페르소나 명칭 (예: '🎬 숏폼 알고리즘 포획형', '🔍 지식 탐색형 딴짓러', '⏳ 마감 직전 초집중 러너', '🎮 심야 보상 탐색형' 등 맥락에 맞게 자유 작명)\",\n"
        "  \"persona_icon\": \"zap, shield-check, target, cpu, refresh, sparkles 중 가장 어울리는 영문 아이콘명 1개\",\n"
        "  \"theme_color\": \"#F43F5E (주의/도파민), #10B981 (안정/우수), #818CF8 (보통/성장), #A855F7 (탈출/게임), #38BDF8 (소통/SNS) 중 1개 색상 헥스코드\",\n"
        "  \"core_insight\": \"왜 이 시간대에 이 사이트로 이탈하는지, 사용자의 세션 패턴과 심리적 원인을 2~3문장으로 날카롭게 짚어주는 심층 진단 리포트\",\n"
        "  \"action_plans\": [\n"
        "    \"오늘 당장 실천할 수 있는 구체적이고 현실적인 환경 제어 처방\",\n"
        "    \"집중 유지 및 이탈 충동을 다스리는 인지 심리 기법 처방\",\n"
        "    \"세션 지속성 및 회복력을 높여주는 루틴 처방\"\n"
        "  ],\n"
        "  \"recommended_block_domains\": [\n"
        "    \"현재 차단 목록(current_blacklist)에 아직 없고, 함께 차단하면 좋은 연관 서브도메인, CDN, 비콘 또는 유사 유혹 사이트 (반드시 2~3개 추천, 소문자 도메인만, 예: googlevideo.com, ytimg.com, gvt2.com, tiktok.com, x.com)\"\n"
        "  ]\n"
        "}"
    ).arg(digestStr);

    QJsonObject root;
    QJsonArray contents;
    QJsonObject contentObj;
    contentObj["role"] = "user";

    QJsonArray parts;
    QJsonObject partObj;
    partObj["text"] = prompt;
    parts.append(partObj);

    contentObj["parts"] = parts;
    contents.append(contentObj);
    root["contents"] = contents;

    QJsonObject genConfig;
    genConfig["responseMimeType"] = "application/json";
    genConfig["temperature"] = 0.85;
    genConfig["topP"] = 0.95;
    genConfig["topK"] = 40;
    root["generationConfig"] = genConfig;

    return root;
}

QString BackendManager::getGeminiRequestBodyJson() const
{
    return QString::fromUtf8(QJsonDocument(buildGeminiRequestBody(generateAiPayloadDigest())).toJson(QJsonDocument::Indented));
}
