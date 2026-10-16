// SPDX-License-Identifier: BSD-2-Clause
// Catálogo: esquema, categorias, discos, lugares, árvore de hashes, pessoas e
// o pipeline inteiro (Searcher -> Indexer) numa pasta temporária.

#include "core/Catalog.h"
#include "core/Disks.h"
#include "core/HashTree.h"
#include "core/Paths.h"
#include "core/Pipeline.h"
#include "core/Places.h"
#include "core/Taxonomy.h"

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTest>

using namespace pix;

namespace {

QString data(const QString &name)
{
    return QStringLiteral(PHOTOIDX_TEST_DATA "/") + name;
}

imaging::Embedding vec(std::initializer_list<float> head, int dims = 512)
{
    imaging::Embedding v(size_t(dims), 0.0f);
    int i = 0;
    for (float x : head)
        v[size_t(i++)] = x;
    imaging::normalize(v);
    return v;
}

FaceResult faces(const QString &photoId, const QList<imaging::Embedding> &embeddings, float score = 0.9f)
{
    FaceResult r;
    r.photoId = photoId;
    r.ok = true;
    for (const auto &e : embeddings) {
        FoundFace f;
        f.faceId = Database::newId();
        f.x = f.y = 0.1f;
        f.w = f.h = 0.2f;
        f.score = score;
        f.embedding = e;
        r.faces.append(f);
    }
    return r;
}

} // namespace

class TestCore : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_home;    ///< a pasta de dados do app (PHOTOIDX_DATA_DIR)
    QTemporaryDir m_photos;  ///< as pastas "da pessoa"

    /// Catálogo em memória com uma pasta e ``n`` fotos falsas já indexadas.
    QStringList seedPhotos(Catalog &catalog, int n, const QString &folder)
    {
        QDir().mkpath(folder);
        const QString folderId = catalog.addFolder(folder);
        QList<FoundFile> files;
        for (int i = 0; i < n; ++i)
            files.append({folder + QStringLiteral("/p%1.jpg").arg(i), 100 + i, 1000 + i, QDateTime(), QDateTime()});
        catalog.ingest(folderId, files);
        QStringList ids;
        QSqlQuery q = catalog.db().exec(QStringLiteral("SELECT id FROM photo ORDER BY path"));
        while (q.next())
            ids << q.value(0).toString();
        return ids;
    }

private slots:
    void initTestCase()
    {
        QVERIFY(m_home.isValid());
        QVERIFY(m_photos.isValid());
        qputenv("PHOTOIDX_DATA_DIR", m_home.path().toUtf8());
        qputenv("PHOTOIDX_MODELS_DIR", PHOTOIDX_MODELS_DIR);
    }

    void schemaAndSeed()
    {
        Catalog catalog;
        catalog.open(QString());
        QCOMPARE(catalog.db().schemaVersion(), Database::latestSchemaVersion());
        const int categories = catalog.db().scalar(QStringLiteral("SELECT count(*) FROM category")).toInt();
        QCOMPARE(categories, int(builtinCategories().size()));
        // 16 raízes, como na lista pedida.
        QCOMPARE(catalog.db().scalar(QStringLiteral("SELECT count(*) FROM category WHERE parent_id IS NULL")).toInt(), 16);
        QVERIFY(!catalog.classifiableCategories().isEmpty());
        // As de regra não vão para o CLIP.
        for (const auto &[id, prompts] : catalog.classifiableCategories())
            QVERIFY(!prompts.isEmpty());
        QCOMPARE(QFileInfo(AppPaths::thumbnailPath(QStringLiteral("abc"), QDate(2024, 5, 3))).absolutePath(),
                 QDir(m_home.path()).filePath(QStringLiteral("thumbnail/2024/05")));
    }

    void places()
    {
        QVERIFY(Places::size() > 100000);
        const PlaceMatch sp = Places::lookup(-23.5505, -46.6333);
        QVERIFY(sp.found);
        QCOMPARE(sp.countryCode, QStringLiteral("BR"));
        QCOMPARE(sp.state, QStringLiteral("São Paulo"));
        const PlaceMatch paris = Places::lookup(48.8566, 2.3522);
        QCOMPARE(paris.countryCode, QStringLiteral("FR"));
        QVERIFY(!Places::lookup(0.0, -30.0).found);  // meio do Atlântico
    }

    void networkDevices_data()
    {
        QTest::addColumn<QString>("device");
        QTest::addColumn<QString>("server");
        QTest::addColumn<QString>("share");
        QTest::newRow("mac smbfs") << "//joao@nas.local/Fotos" << "nas.local" << "Fotos";
        QTest::newRow("linux cifs") << "//192.168.0.10/photos/2024" << "192.168.0.10" << "photos/2024";
        QTest::newRow("windows unc") << "\\\\NAS\\Fotos" << "NAS" << "Fotos";
        QTest::newRow("nfs") << "nas:/volume1/photos" << "nas" << "volume1/photos";
        QTest::newRow("sshfs") << "user@host:/srv/pics" << "host" << "srv/pics";
        QTest::newRow("webdav") << "https://dav.example.com/remote/fotos" << "dav.example.com" << "remote/fotos";
    }

    void networkDevices()
    {
        QFETCH(QString, device);
        QFETCH(QString, server);
        QFETCH(QString, share);
        QString s, sh;
        parseNetworkDevice(device, &s, &sh);
        QCOMPARE(s, server);
        QCOMPARE(sh, share);
    }

    void networkProtocols()
    {
        QCOMPARE(networkProtocol(QStringLiteral("smbfs"), {}), QStringLiteral("smb"));
        QCOMPARE(networkProtocol(QStringLiteral("cifs"), {}), QStringLiteral("smb"));
        QCOMPARE(networkProtocol(QStringLiteral("nfs4"), {}), QStringLiteral("nfs"));
        QCOMPARE(networkProtocol(QStringLiteral("afpfs"), {}), QStringLiteral("afp"));
        QCOMPARE(networkProtocol(QStringLiteral("fuse.sshfs"), {}), QStringLiteral("sshfs"));
        QCOMPARE(networkProtocol(QStringLiteral("ext4"), QStringLiteral("/dev/sda1")), QString());
        QCOMPARE(networkProtocol(QStringLiteral("apfs"), QStringLiteral("/dev/disk3s1")), QString());
    }

    void localDiskIdentity()
    {
        const DiskInfo disk = identifyDisk(m_home.path());
        QVERIFY(disk.isValid());
        QVERIFY(!disk.mountPoint.isEmpty());
        QCOMPARE(identifyDisk(m_home.path()).key, disk.key);  // estável
        QVERIFY(!relativeToMount(m_home.path(), disk.mountPoint).startsWith(u'/'));
    }

    void hashTree()
    {
        HashTree tree;
        tree.insert(QStringLiteral("a"), 0b0000);
        tree.insert(QStringLiteral("b"), 0b0001);
        tree.insert(QStringLiteral("c"), 0b0111);
        tree.insert(QStringLiteral("d"), 0xFFFFFFFFFFFFFFFFull);
        tree.insert(QStringLiteral("e"), 0b0000);
        auto ids = [](const QList<HashTree::Match> &m) {
            QStringList out;
            for (const auto &x : m)
                out << x.id;
            std::sort(out.begin(), out.end());
            return out;
        };
        QCOMPARE(ids(tree.query(0, 0)), (QStringList{QStringLiteral("a"), QStringLiteral("e")}));
        QCOMPARE(ids(tree.query(0, 1)), (QStringList{QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("e")}));
        QCOMPARE(ids(tree.query(0, 3)).size(), 4);
        tree.remove(QStringLiteral("a"));
        QCOMPARE(ids(tree.query(0, 0)), QStringList{QStringLiteral("e")});
        tree.insert(QStringLiteral("e"), 0b0111);  // troca o hash
        QCOMPARE(ids(tree.query(0b0111, 0)), (QStringList{QStringLiteral("c"), QStringLiteral("e")}));
    }

    void peopleClusteringAndCorrections()
    {
        Catalog catalog;
        catalog.open(QString());
        const QStringList photos = seedPhotos(catalog, 6, m_photos.path() + QStringLiteral("/people"));
        QCOMPARE(photos.size(), 6);
        const auto ana1 = vec({1, 0.1f}), ana2 = vec({1, 0.15f}), bob = vec({0, 0, 1});
        catalog.applyFaces(faces(photos[0], {ana1, bob}));
        catalog.applyFaces(faces(photos[1], {ana2}));
        // Duas pessoas: as faces parecidas juntas, a diferente à parte.
        QVariantList people = catalog.people();
        QCOMPARE(people.size(), 2);
        const QString anaId = people[0].toMap().value(QStringLiteral("faceCount")).toInt() == 2
                                  ? people[0].toMap().value(QStringLiteral("id")).toString()
                                  : people[1].toMap().value(QStringLiteral("id")).toString();
        catalog.renamePerson(anaId, QStringLiteral("Ana"));
        QCOMPARE(catalog.person(anaId).value(QStringLiteral("name")).toString(), QStringLiteral("Ana"));

        // Busca por rosto: acha as duas fotos da Ana.
        QSet<QString> found;
        for (const auto &m : catalog.searchFace(ana1, 0.45f))
            found.insert(m.entry.photoId);
        QCOMPARE(found, (QSet<QString>{photos[0], photos[1]}));

        // Falso positivo: a face sai da Ana e nunca volta para ela.
        const QVariantList anaFaces = catalog.personFaces(anaId);
        QCOMPARE(anaFaces.size(), 2);
        const QString wrong = anaFaces[1].toMap().value(QStringLiteral("id")).toString();
        catalog.rejectFaces({wrong});
        QCOMPARE(catalog.personFaces(anaId).size(), 1);
        QCOMPARE(catalog.unassignedFaces().size(), 1);
        catalog.reclusterPeople();
        QCOMPARE(catalog.personFaces(anaId).size(), 1);
        // Falso negativo: a pessoa põe de volta.
        catalog.assignFaces({wrong}, anaId);
        QCOMPARE(catalog.personFaces(anaId).size(), 2);
    }

    void autoMerge()
    {
        Catalog catalog;
        catalog.open(QString());
        const QStringList photos = seedPhotos(catalog, 8, m_photos.path() + QStringLiteral("/merge"));
        // Limiar de match alto: o mesmo rosto vira duas pessoas...
        catalog.setSetting(QStringLiteral("faces/matchSimilarity"), 0.999);
        catalog.setSetting(QStringLiteral("faces/mergeSimilarity"), 0.95);
        catalog.setSetting(QStringLiteral("faces/mergeMinFaces"), 2);
        for (int i = 0; i < 4; ++i)
            catalog.applyFaces(faces(photos[i], {vec({1, 0.06f * i})}));
        const int before = catalog.people().size();
        QVERIFY(before >= 2);
        // ... e o merge automático as junta (parecidas e com faces suficientes).
        catalog.setSetting(QStringLiteral("faces/matchSimilarity"), 0.45);
        for (int i = 4; i < 8; ++i)
            catalog.applyFaces(faces(photos[i], {vec({1, 0.06f * (i - 4)})}));
        QVERIFY(catalog.autoMergePeople() >= 0);
        catalog.setSetting(QStringLiteral("faces/mergeMinFaces"), 1);
        catalog.autoMergePeople();
        QCOMPARE(catalog.people().size(), 1);
        // Pouca confiança (detecção fraca) não junta.
        Catalog other;
        other.open(QString());
        const QStringList p2 = seedPhotos(other, 2, m_photos.path() + QStringLiteral("/merge2"));
        other.setSetting(QStringLiteral("faces/matchSimilarity"), 0.999);
        other.setSetting(QStringLiteral("faces/mergeMinFaces"), 1);
        other.applyFaces(faces(p2[0], {vec({1, 0})}, 0.4f));
        other.applyFaces(faces(p2[1], {vec({1, 0.01f})}, 0.4f));
        QCOMPARE(other.autoMergePeople(), 0);
    }

    void categoriesAndAlbums()
    {
        Catalog catalog;
        catalog.open(QString());
        const QStringList photos = seedPhotos(catalog, 3, m_photos.path() + QStringLiteral("/org"));
        const QString trips = catalog.createAlbum({}, QStringLiteral("Viagens"));
        const QString rio = catalog.createAlbum(trips, QStringLiteral("Rio 2024"));
        catalog.addPhotosToAlbum({photos[0], photos[1]}, rio);
        GridQuery q;
        q.node = QStringLiteral("album:") + trips;  // o pai mostra o que está nos filhos
        QCOMPARE(catalog.photos(q).size(), 2);
        q.node = QStringLiteral("all");
        q.search = QStringLiteral("rio");
        QCOMPARE(catalog.photos(q).size(), 2);
        QVERIFY_THROWS_EXCEPTION(ActionError, catalog.moveAlbum(trips, rio));

        const QString cat = catalog.createCategory({}, QStringLiteral("Minha categoria"));
        catalog.addPhotosToCategory({photos[2]}, cat);
        q.search.clear();
        q.node = QStringLiteral("cat:") + cat;
        QCOMPARE(catalog.photos(q).size(), 1);
        catalog.removePhotosFromCategory({photos[2]}, cat);
        QCOMPARE(catalog.photos(q).size(), 0);
        // Sem categoria de conteúdo: cai em "Sem categoria".
        QVERIFY(!catalog.classifiableCategories().isEmpty());
    }

    void customFilters()
    {
        Catalog catalog;
        catalog.open(QString());
        const QStringList photos = seedPhotos(catalog, 4, m_photos.path() + QStringLiteral("/filters"));
        auto set = [&](int i, int w, int h, const char *camera, qint64 size) {
            catalog.db().exec(QStringLiteral("UPDATE photo SET width = ?, height = ?, camera = ?, size = ?, taken_at = ? WHERE id = ?"),
                              {w, h, QString::fromLatin1(camera), size, QStringLiteral("2024-0%1-10T10:00:00.000").arg(i + 1), photos[i]});
        };
        set(0, 6000, 4000, "Canon EOS R5", 30 << 20);
        set(1, 4000, 6000, "Canon EOS R5", 5 << 20);
        set(2, 1920, 1080, "NIKON D850", 2 << 20);
        set(3, 1000, 1000, "iPhone 15", 1 << 20);
        auto count = [&](const QVariantList &rules, bool all = true) {
            GridQuery q;
            q.rules = Catalog::rulesFromVariant(rules);
            q.matchAll = all;
            return int(catalog.photos(q).size());
        };
        auto rule = [](const char *field, const char *op, const QVariant &v = {}, const QVariant &v2 = {}) {
            return QVariant(QVariantMap{{QStringLiteral("field"), QString::fromLatin1(field)},
                                        {QStringLiteral("op"), QString::fromLatin1(op)},
                                        {QStringLiteral("value"), v}, {QStringLiteral("value2"), v2}});
        };
        QCOMPARE(count({rule("width", "gte", 4000)}), 2);
        QCOMPARE(count({rule("camera", "contains", QStringLiteral("canon")), rule("shape", "eq", QStringLiteral("portrait"))}), 1);
        QCOMPARE(count({rule("camera", "contains", QStringLiteral("nikon")), rule("shape", "eq", QStringLiteral("square"))}, false), 2);
        QCOMPARE(count({rule("sizeMb", "between", 1.5, 10)}), 2);
        QCOMPARE(count({rule("megapixels", "gt", 20)}), 2);
        QCOMPARE(count({rule("taken", "before", QStringLiteral("2024-03-01"))}), 2);
        QCOMPARE(count({rule("hasGps", "isTrue")}), 0);
        QCOMPARE(count({rule("camera", "contains", QStringLiteral("50%_"))}), 0);  // curingas escapados
        QCOMPARE(count({rule("nope", "eq", 1)}), 4);   // campo desconhecido: não filtra
        const QString album = catalog.createAlbum({}, QStringLiteral("A"));
        catalog.addPhotosToAlbum({photos[0]}, album);
        QCOMPARE(count({rule("album", "has", album), rule("width", "gt", 100)}), 1);
        QCOMPARE(count({rule("album", "none")}), 3);
        const QString id = catalog.saveFilter(QStringLiteral("Grandes"), true, {rule("megapixels", "gt", 20)});
        QCOMPARE(catalog.savedFilters().size(), 1);
        QCOMPARE(catalog.saveFilter(QStringLiteral("grandes"), true, {}), id);  // mesmo nome substitui
        catalog.deleteFilter(id);
        QVERIFY(catalog.savedFilters().isEmpty());
        QVERIFY(!Catalog::filterFields().isEmpty());
    }

    void appDataIsNeverIndexed()
    {
        Catalog catalog;
        catalog.open(QString());
        QVERIFY_THROWS_EXCEPTION(ActionError, catalog.addFolder(m_home.path()));
        QVERIFY_THROWS_EXCEPTION(ActionError, catalog.addFolder(AppPaths::thumbnailsDir()));

        // Indexar a "raiz" que contém o cache do app (como C:\): o cache é pulado.
        const QString root = m_photos.path() + QStringLiteral("/root");
        const QString cache = root + QStringLiteral("/PhotoIDXData");
        QDir().mkpath(cache + QStringLiteral("/thumbnail/2024/01"));
        QVERIFY(QFile::copy(data(QStringLiteral("exif/o1.jpg")), root + QStringLiteral("/a.jpg")));
        QVERIFY(QFile::copy(data(QStringLiteral("exif/o1.jpg")), cache + QStringLiteral("/thumbnail/2024/01/t.jpg")));
        const QByteArray previous = qgetenv("PHOTOIDX_DATA_DIR");
        qputenv("PHOTOIDX_DATA_DIR", cache.toUtf8());
        const QString folderId = catalog.addFolder(root);
        Pipeline pipeline(&catalog);
        pipeline.pauseAll();
        QSignalSpy scanned(&pipeline, &Pipeline::scanFinished);
        pipeline.scan({folderId});
        QTRY_COMPARE_WITH_TIMEOUT(scanned.size(), 1, 30000);
        qputenv("PHOTOIDX_DATA_DIR", previous);
        QCOMPARE(scanned.first().at(1).toInt(), 1);
    }

    void pipelineEndToEnd()
    {
        // Pasta com fotos de teste (uma duplicada) e um arquivo que não é foto.
        const QString folder = m_photos.path() + QStringLiteral("/library/2024/viagem");
        QDir().mkpath(folder);
        QDir().mkpath(m_photos.path() + QStringLiteral("/library/.hidden"));
        QDir().mkpath(m_photos.path() + QStringLiteral("/library/@eaDir"));
        for (const QString &name : {QStringLiteral("o1.jpg"), QStringLiteral("o6.jpg"), QStringLiteral("shooting.jpg")})
            QVERIFY(QFile::copy(data(QStringLiteral("exif/") + name), folder + u'/' + name));
        QVERIFY(QFile::copy(data(QStringLiteral("exif/o1.jpg")), folder + QStringLiteral("/copy-of-o1.JPG")));
        QVERIFY(QFile::copy(data(QStringLiteral("sample.jpg")), m_photos.path() + QStringLiteral("/library/.hidden/x.jpg")));
        QVERIFY(QFile::copy(data(QStringLiteral("sample.jpg")), m_photos.path() + QStringLiteral("/library/@eaDir/x.jpg")));
        QFile txt(folder + QStringLiteral("/notes.txt"));
        QVERIFY(txt.open(QIODevice::WriteOnly));
        txt.write("x");
        txt.close();

        Catalog catalog;
        catalog.open(QString());
        const QString folderId = catalog.addFolder(m_photos.path() + QStringLiteral("/library"));
        QVERIFY_THROWS_EXCEPTION(ActionError, catalog.addFolder(folder));  // já está dentro
        Pipeline pipeline(&catalog);
        pipeline.pause(QStringLiteral("categorize"));
        pipeline.pause(QStringLiteral("faces"));
        QSignalSpy scanned(&pipeline, &Pipeline::scanFinished);
        pipeline.start(QStringLiteral("index"));
        pipeline.scan({folderId});
        QTRY_COMPARE_WITH_TIMEOUT(scanned.size(), 1, 30000);
        QCOMPARE(scanned.first().at(1).toInt(), 4);  // só as 4 fotos visíveis
        QTRY_VERIFY_WITH_TIMEOUT(catalog.pendingCount(QStringLiteral("index")) == 0 && !pipeline.busy(), 60000);

        const QVariantMap stats = catalog.stats();
        QCOMPARE(stats.value(QStringLiteral("photos")).toInt(), 4);
        QCOMPARE(stats.value(QStringLiteral("indexed")).toInt(), 4);
        // Miniatura em thumbnail/YYYY/MM/<uuid>.jpg.
        QSqlQuery q = catalog.db().exec(QStringLiteral("SELECT id, thumb_path, exif_count, picture_hash, taken_source FROM photo"));
        while (q.next()) {
            const QString thumb = q.value(1).toString();
            QVERIFY2(QFileInfo::exists(thumb), qPrintable(thumb));
            QVERIFY(thumb.endsWith(q.value(0).toString() + QStringLiteral(".jpg")));
            QVERIFY(thumb.startsWith(QDir(m_home.path()).filePath(QStringLiteral("thumbnail/"))));
            QCOMPARE(q.value(3).toString().size(), 64);
        }
        // A cópia com os mesmos pixels entra em "Duplicata", junto com o original.
        GridQuery dup;
        dup.node = QStringLiteral("cat:") + catalog.db().scalar(
            QStringLiteral("SELECT id FROM category WHERE builtin_key = 'other.duplicate'")).toString();
        QCOMPARE(catalog.photos(dup).size(), 2);
        // Árvore de pastas: disco -> pasta indexada -> 2024 -> viagem.
        const QVariantList folders = catalog.tree(QStringLiteral("folders"));
        QVERIFY(folders.size() >= 4);
        QVERIFY(folders.first().toMap().value(QStringLiteral("key")).toString().startsWith(QStringLiteral("disk:")));
        GridQuery byDir;
        byDir.node = QStringLiteral("dir:") + QDir::fromNativeSeparators(folder);
        QCOMPARE(catalog.photos(byDir).size(), 4);
        // Datas: EXIF quando há, senão a do arquivo — toda foto tem ano.
        QVERIFY(!catalog.tree(QStringLiteral("dates")).isEmpty());
        // Busca por nome.
        GridQuery search;
        search.search = QStringLiteral("shooting");
        QCOMPARE(catalog.photos(search).size(), 1);
        const QString shootingId = catalog.photos(search).first().id;
        const QVariantMap detail = catalog.photoDetail(shootingId);
        QVERIFY(!detail.value(QStringLiteral("exif")).toList().isEmpty());
        QCOMPARE(detail.value(QStringLiteral("takenSource")).toString(), QStringLiteral("exif"));

        // Arquivo removido do disco: vira "missing" no próximo scan.
        QVERIFY(QFile::remove(folder + QStringLiteral("/o6.jpg")));
        pipeline.scan({folderId});
        QTRY_COMPARE_WITH_TIMEOUT(scanned.size(), 2, 30000);
        QCOMPARE(scanned.last().at(3).toInt(), 1);
        QCOMPARE(catalog.stats().value(QStringLiteral("photos")).toInt(), 3);

        // Tirar a pasta apaga as miniaturas e o catálogo dela (as fotos ficam).
        catalog.removeFolder(folderId);
        QCOMPARE(catalog.stats().value(QStringLiteral("photos")).toInt(), 0);
        QVERIFY(QFileInfo::exists(folder + QStringLiteral("/o1.jpg")));
    }
};

QTEST_GUILESS_MAIN(TestCore)
#include "tst_core.moc"
