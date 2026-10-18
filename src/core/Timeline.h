// SPDX-License-Identifier: BSD-2-Clause
#pragma once

// "Ao longo dos anos" (tela Pessoas): uma foto da pessoa por período (mês,
// trimestre, semestre ou ano), para montar uma imagem só. A regra inteira,
// sem banco: agrupar as faces por período, ordenar as candidatas e decidir
// qual fica em cada período a partir das escolhas guardadas
// (person_timeline). Roda em qualquer thread.

#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

namespace pix::timeline {

/// Uma face da pessoa numa foto datada (Catalog::timelineFaces).
struct Face
{
    QString faceId;
    QString photoId;
    QString path;        ///< o original
    QString thumbPath;   ///< a miniatura (disco desconectado)
    QString cropPath;    ///< o recorte pequeno do rosto
    QString photoName;
    int year = 0;
    int month = 0;       ///< 0 = sem mês (só entra na frequência anual)
    double x = 0, y = 0, w = 0, h = 0;   ///< caixa normalizada na foto orientada
    int width = 0, height = 0;           ///< da foto orientada
    double score = 0;                    ///< confiança da detecção
    double similarity = 0;               ///< ao centroide da pessoa
};

/// A escolha guardada de um período.
struct Pick
{
    QString period;
    QString faceId;      ///< vazio = sem foto (o período some da montagem)
    bool user = false;   ///< a pessoa escolheu (senão, automática)
    bool hidden = false; ///< tirado da montagem
};

/// Um período da montagem, já resolvido.
struct Entry
{
    QString period;
    QString faceId;
    bool user = false;
    bool hidden = false;
    int candidates = 0;  ///< quantas fotos da pessoa no período
};

/// "month" | "quarter" | "semester" | "year".
bool validFrequency(const QString &frequency);
/// "2016", "2016-S1", "2016-Q3", "2016-07"; vazio se a data não basta.
QString periodOf(const QString &frequency, int year, int month);
/// Nota de uma face para representar o período (maior = melhor): parecida
/// com a pessoa (rosto de frente, nítido), grande e bem detectada.
double rank(const Face &face);

/// Os períodos em ordem cronológica, cada um com as faces (índices em
/// ``faces``) da melhor para a pior.
QList<QPair<QString, QList<int>>> group(const QList<Face> &faces, const QString &frequency);

/// Resolve cada período: a escolha guardada se ainda vale (a face continua
/// no período, ou o período foi tirado da montagem); senão, a melhor face.
/// As escolhas novas ou refeitas vão para ``toSave`` (quem chama grava).
QList<Entry> resolve(const QList<Face> &faces, const QList<QPair<QString, QList<int>>> &groups,
                     const QHash<QString, Pick> &picks, QList<Pick> *toSave);

/// Colunas da montagem para ``count`` fotos (perto de um quadrado, como as
/// montagens "ao longo dos anos" das redes: 3x3, 4x2...).
int autoColumns(int count);

} // namespace pix::timeline
