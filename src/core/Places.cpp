// SPDX-License-Identifier: BSD-2-Clause
#include "core/Places.h"

#include <QFile>
#include <QHash>
#include <QLoggingCategory>

#include <cmath>
#include <mutex>
#include <vector>

namespace pix {

namespace {

Q_LOGGING_CATEGORY(lcPlaces, "photoidx.places")

struct City
{
    float lat;
    float lon;
    int name;       ///< índices em Table::strings
    int state;
    int countryCode;
    int country;
};

/// Cidades em células de 1° x 1°: a busca olha a célula do ponto e as vizinhas.
struct Table
{
    std::vector<City> cities;
    QStringList strings;
    QHash<int, std::vector<int>> cells;

    static int cellOf(double lat, double lon)
    {
        const int la = int(std::floor(lat)) + 90;
        const int lo = (int(std::floor(lon)) + 180 + 360) % 360;
        return la * 360 + lo;
    }
};

const Table &table()
{
    static Table t;
    static std::once_flag once;
    std::call_once(once, [] {
        QFile f(QStringLiteral(":/photoidx/data/places.qz"));
        if (!f.open(QIODevice::ReadOnly)) {
            qCWarning(lcPlaces) << "Places table missing";
            return;
        }
        const QByteArray raw = qUncompress(f.readAll());
        QHash<QString, int> interned;
        auto intern = [&](const QString &s) {
            const auto it = interned.constFind(s);
            if (it != interned.constEnd())
                return *it;
            const int id = int(t.strings.size());
            t.strings << s;
            interned.insert(s, id);
            return id;
        };
        for (const QByteArray &line : raw.split('\n')) {
            if (line.isEmpty() || line.startsWith('#'))
                continue;
            const QList<QByteArray> p = line.split('\t');
            if (p.size() < 6)
                continue;
            City c;
            c.lat = p[0].toFloat();
            c.lon = p[1].toFloat();
            c.name = intern(QString::fromUtf8(p[2]));
            c.state = intern(QString::fromUtf8(p[3]));
            c.countryCode = intern(QString::fromUtf8(p[4]));
            c.country = intern(QString::fromUtf8(p[5]));
            t.cells[Table::cellOf(c.lat, c.lon)].push_back(int(t.cities.size()));
            t.cities.push_back(c);
        }
        qCInfo(lcPlaces) << "Places table loaded:" << t.cities.size() << "cities";
    });
    return t;
}

double haversineKm(double lat1, double lon1, double lat2, double lon2)
{
    constexpr double kEarthKm = 6371.0;
    constexpr double kRad = M_PI / 180.0;
    const double dLat = (lat2 - lat1) * kRad, dLon = (lon2 - lon1) * kRad;
    const double a = std::sin(dLat / 2) * std::sin(dLat / 2)
                     + std::cos(lat1 * kRad) * std::cos(lat2 * kRad) * std::sin(dLon / 2) * std::sin(dLon / 2);
    return 2 * kEarthKm * std::asin(std::min(1.0, std::sqrt(a)));
}

} // namespace

PlaceMatch Places::lookup(double latitude, double longitude, double maxKm)
{
    PlaceMatch out;
    const Table &t = table();
    if (t.cities.empty() || !std::isfinite(latitude) || !std::isfinite(longitude))
        return out;
    // Células vizinhas: 1° de latitude ~ 111 km; perto dos polos a longitude
    // encolhe, então o raio em células cresce.
    const int latRing = std::max(1, int(std::ceil(maxKm / 111.0)));
    const double cosLat = std::max(0.05, std::cos(latitude * M_PI / 180.0));
    const int lonRing = std::min(180, std::max(1, int(std::ceil(maxKm / (111.0 * cosLat)))));
    int best = -1;
    double bestKm = maxKm;
    for (int dy = -latRing; dy <= latRing; ++dy) {
        const double lat = latitude + dy;
        if (lat < -90 || lat >= 90)
            continue;
        for (int dx = -lonRing; dx <= lonRing; ++dx) {
            const auto it = t.cells.constFind(Table::cellOf(lat, longitude + dx));
            if (it == t.cells.constEnd())
                continue;
            for (int i : *it) {
                const City &c = t.cities[size_t(i)];
                const double km = haversineKm(latitude, longitude, c.lat, c.lon);
                if (km < bestKm) {
                    bestKm = km;
                    best = i;
                }
            }
        }
    }
    if (best < 0)
        return out;
    const City &c = t.cities[size_t(best)];
    out.found = true;
    out.city = t.strings[c.name];
    out.state = t.strings[c.state];
    out.countryCode = t.strings[c.countryCode];
    out.country = t.strings[c.country];
    out.distanceKm = bestKm;
    return out;
}

int Places::size()
{
    return int(table().cities.size());
}

} // namespace pix
