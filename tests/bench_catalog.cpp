// SPDX-License-Identifier: BSD-2-Clause
// Mede, num catálogo de verdade (uma CÓPIA), quanto custa cada operação que o
// app faz na thread principal — a que desenha a interface. Fora do ctest:
//
//   bench_catalog <cópia do data.sqlite3>
//
// Cada linha: milissegundos (mediana de N rodadas) e o nome. Tudo acima de
// ~50 ms, repetido com frequência, aparece como "travada".

#include "core/Catalog.h"
#include "core/GridReader.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QSqlQuery>
#include <QTextStream>

#include <algorithm>
#include <functional>
#include <vector>

using namespace pix;

namespace {

QTextStream out(stdout);

void bench(const char *name, const std::function<void()> &fn, int runs = 3)
{
    std::vector<double> ms;
    for (int i = 0; i < runs; ++i) {
        QElapsedTimer t;
        t.start();
        fn();
        ms.push_back(t.nsecsElapsed() / 1e6);
    }
    std::sort(ms.begin(), ms.end());
    out << QStringLiteral("%1 ms  %2\n").arg(ms[ms.size() / 2], 9, 'f', 1).arg(QLatin1String(name));
    out.flush();
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    if (argc < 2) {
        out << "usage: bench_catalog <copy of data.sqlite3>\n";
        return 2;
    }
    const QString path = QString::fromLocal8Bit(argv[1]);
    Catalog catalog;
    bench("open (migrar, semear, índices, pessoas)", [&] { catalog.open(path); }, 1);

    bench("stats()", [&] { catalog.stats(); });
    for (const char *stage : {"index", "categorize", "faces", "privacy"})
        bench(qPrintable(QStringLiteral("pendingCount(%1)").arg(QLatin1String(stage))),
              [&] { catalog.pendingCount(QString::fromLatin1(stage)); });
    bench("stageTotals()", [&] { catalog.stageTotals(); });
    bench("nextIndexJobs(16)", [&] { catalog.nextIndexJobs(16, {}); });
    bench("nextCategorizeJobs(4)", [&] { catalog.nextCategorizeJobs(4, {}); });
    bench("nextFaceJobs(4)", [&] { catalog.nextFaceJobs(4, {}); });
    for (const char *tab : {"folders", "dates", "categories", "albums", "places", "people"})
        bench(qPrintable(QStringLiteral("tree(%1)").arg(QLatin1String(tab))), [&] { catalog.tree(QString::fromLatin1(tab)); });
    GridQuery all;
    bench("photos(all) na thread principal", [&] { catalog.photos(all); });
    QVariantList params;
    const QString sql = catalog.photosSql(all, params);
    bench("photos(all) pelo GridReader", [&] { readGridRows(path, sql, params); });
    bench("people()", [&] { catalog.people(); });
    bench("unassignedFaces()", [&] { catalog.unassignedFaces(); });
    bench("folders()", [&] { catalog.folders(); });
    bench("disks()", [&] { catalog.disks(); });
    bench("errors(200)", [&] { catalog.errors(200); });
    bench("privacyStatus()", [&] { catalog.privacyStatus(); });
    bench("refreshDisks()", [&] { catalog.refreshDisks(); });

    // Uma face nova (o que applyFaces faz a cada foto com rosto).
    QSqlQuery anyPhoto = catalog.db().exec(QStringLiteral("SELECT p.id, f.embedding FROM photo p JOIN face f ON f.photo_id = p.id LIMIT 1"));
    if (anyPhoto.next()) {
        const QString photoId = anyPhoto.value(0).toString();
        const imaging::Embedding e = imaging::fromBlob(anyPhoto.value(1).toByteArray());
        bench("applyFaces(1 rosto)", [&] {
            FaceResult r;
            r.photoId = photoId;
            r.ok = true;
            FoundFace f;
            f.faceId = Database::newId();
            f.x = f.y = 0.1f;
            f.w = f.h = 0.2f;
            f.score = 0.9f;
            f.embedding = e;
            r.faces.append(f);
            catalog.applyFaces(r);
        });
        QSqlQuery emb = catalog.db().exec(QStringLiteral("SELECT vector FROM photo_embedding LIMIT 1"));
        if (emb.next()) {
            const imaging::Embedding text = imaging::fromBlob(emb.value(0).toByteArray());
            bench("rankByEmbedding (busca por IA)", [&] { catalog.rankByEmbedding(text, 0.2f, 500); });
            bench("similarPhotos()", [&] { catalog.similarPhotos(photoId, 8); });
            bench("photoDetail()", [&] { catalog.photoDetail(photoId); });
        }
    }
    bench("autoMergePeople()", [&] { catalog.autoMergePeople(); }, 1);
    return 0;
}
