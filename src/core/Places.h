// SPDX-License-Identifier: BSD-2-Clause
#pragma once

// Geolocalização reversa OFFLINE: GPS -> cidade, estado, país. A tabela é a
// do GeoNames (cidades com 1.000+ habitantes, CC-BY 4.0), embutida no binário
// (src/core/data/places.qz, gerada por packaging/geonames.py). Nada sai da
// máquina.
//
// Thread-safe: carrega uma vez (na primeira busca) e só lê depois.

#include <QString>

namespace pix {

struct PlaceMatch
{
    bool found = false;
    QString countryCode;    ///< "BR"
    QString country;        ///< "Brazil"
    QString state;          ///< "São Paulo"
    QString city;           ///< "Campinas"
    double distanceKm = 0.0;
};

class Places
{
public:
    /// A cidade mais próxima de (lat, lon), até ``maxKm``.
    static PlaceMatch lookup(double latitude, double longitude, double maxKm = 150.0);
    /// Quantas cidades a tabela tem (carrega se preciso). 0 = tabela ausente.
    static int size();
};

} // namespace pix
