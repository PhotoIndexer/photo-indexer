// SPDX-License-Identifier: BSD-2-Clause
#pragma once

// Leitura da grade fora da thread principal. A consulta da grade de um
// catálogo grande (dezenas de milhares de fotos, ordenadas) leva centenas de
// ms; na thread principal isso travava cada clique na árvore. Com o banco em
// WAL, uma segunda conexão SÓ DE LEITURA, numa thread própria, lê em paralelo
// com as gravações. O SQL vem pronto do Catalog (photosSql: lista fechada,
// valores ligados) — aqui só se executa.

#include "core/Catalog.h"

namespace pix {

/// Roda ``sql`` numa conexão de leitura desta thread (aberta na primeira vez).
/// Lança DatabaseError se a consulta falhar.
QList<GridRow> readGridRows(const QString &databasePath, const QString &sql, const QVariantList &params);

} // namespace pix
