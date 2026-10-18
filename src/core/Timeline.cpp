// SPDX-License-Identifier: BSD-2-Clause
#include "Timeline.h"

#include <algorithm>
#include <cmath>
#include <map>

namespace pix::timeline {

namespace {

/// Abaixo disso (lado do rosto em pixels) o recorte sai borrado.
constexpr double kSmallFace = 64.0;
/// A partir disso o tamanho não conta mais.
constexpr double kBigFace = 256.0;

} // namespace

bool validFrequency(const QString &frequency)
{
    return frequency == QLatin1String("month") || frequency == QLatin1String("quarter")
        || frequency == QLatin1String("semester") || frequency == QLatin1String("year");
}

QString periodOf(const QString &frequency, int year, int month)
{
    if (year <= 0)
        return {};
    const QString y = QString::number(year);
    if (frequency == QLatin1String("year"))
        return y;
    if (month < 1 || month > 12)
        return {};
    if (frequency == QLatin1String("semester"))
        return y + QStringLiteral("-S") + QString::number(month <= 6 ? 1 : 2);
    if (frequency == QLatin1String("quarter"))
        return y + QStringLiteral("-Q") + QString::number((month - 1) / 3 + 1);
    if (frequency == QLatin1String("month"))
        return y + u'-' + QStringLiteral("%1").arg(month, 2, 10, QLatin1Char('0'));
    return {};
}

double rank(const Face &face)
{
    // Parecida com o centroide = o rosto "típico" da pessoa: de frente,
    // nítido, sem óculos escuros. É o que mais pesa.
    const double side = std::min(face.w * face.width, face.h * face.height);
    const double size = std::clamp((side - kSmallFace) / (kBigFace - kSmallFace), 0.0, 1.0);
    double r = face.similarity + 0.25 * size + 0.15 * face.score;
    if (side < kSmallFace)
        r -= 0.5;
    return r;
}

QList<QPair<QString, QList<int>>> group(const QList<Face> &faces, const QString &frequency)
{
    // A chave do período ordena como texto ("2016-Q1" < "2016-Q2" < "2017").
    std::map<QString, QList<int>> byPeriod;
    for (int i = 0; i < faces.size(); ++i) {
        const QString period = periodOf(frequency, faces[i].year, faces[i].month);
        if (!period.isEmpty())
            byPeriod[period].append(i);
    }
    QList<QPair<QString, QList<int>>> out;
    out.reserve(int(byPeriod.size()));
    for (auto &[period, list] : byPeriod) {
        std::stable_sort(list.begin(), list.end(), [&faces](int a, int b) {
            const double ra = rank(faces[a]), rb = rank(faces[b]);
            return ra != rb ? ra > rb : faces[a].faceId < faces[b].faceId;
        });
        out.append({period, list});
    }
    return out;
}

QList<Entry> resolve(const QList<Face> &faces, const QList<QPair<QString, QList<int>>> &groups,
                     const QHash<QString, Pick> &picks, QList<Pick> *toSave)
{
    QList<Entry> out;
    out.reserve(groups.size());
    for (const auto &[period, list] : groups) {
        Entry e;
        e.period = period;
        e.candidates = int(list.size());
        const auto it = picks.constFind(period);
        bool kept = false;
        if (it != picks.constEnd()) {
            const bool stillThere = std::any_of(list.begin(), list.end(),
                                                [&](int i) { return faces[i].faceId == it->faceId; });
            if (stillThere || (it->hidden && it->faceId.isEmpty())) {
                e.faceId = it->faceId;
                e.user = it->user;
                e.hidden = it->hidden;
                kept = true;
            } else if (it->hidden) {
                // A foto escolhida saiu (apagada, privada, outra pessoa), mas
                // o período continua fora da montagem.
                e.faceId = faces[list.first()].faceId;
                e.hidden = true;
                kept = true;
                if (toSave)
                    toSave->append({period, e.faceId, false, true});
            }
        }
        if (!kept) {
            e.faceId = faces[list.first()].faceId;
            if (toSave)
                toSave->append({period, e.faceId, false, false});
        }
        out.append(e);
    }
    return out;
}

int autoColumns(int count)
{
    if (count <= 3)
        return std::max(count, 1);
    if (count == 4)
        return 2;
    if (count <= 9)
        return 3;
    if (count <= 16)
        return 4;
    if (count <= 30)
        return 5;
    if (count <= 48)
        return 6;
    return int(std::ceil(std::sqrt(double(count) * 1.2)));
}

} // namespace pix::timeline
