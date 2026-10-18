// SPDX-License-Identifier: BSD-2-Clause
// Catálogo: esquema, categorias, discos, lugares, árvore de hashes, pessoas e
// o pipeline inteiro (Searcher -> Indexer) numa pasta temporária; fotos
// privadas (criptografia, cofre, cifrar/decifrar os arquivos de verdade).

#include "app/KeyedListModel.h"
#include "core/Catalog.h"
#include "core/CatalogReader.h"
#include "core/Classifier.h"
#include "core/Crypto.h"
#include "core/Disks.h"
#include "core/HashTree.h"
#include "core/Paths.h"
#include "core/FileOps.h"
#include "core/Pipeline.h"
#include "core/Places.h"
#include "core/Privacy.h"
#include "core/Taxonomy.h"
#include "core/Vault.h"

#include <QAbstractItemModelTester>
#include <QBuffer>
#include <QRandomGenerator>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QMap>
#include <QSignalSpy>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTest>
#include <QThread>

#include <cmath>
#include <limits>

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

QByteArray sha(const QByteArray &bytes)
{
    return QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex();
}

QByteArray readAll(const QString &path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

void writeFile(const QString &path, const QByteArray &bytes)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(bytes);
}

/// Roda um lote da etapa "privacy" e grava os resultados.
int runPrivacyJobs(Catalog &catalog)
{
    int ok = 0;
    for (const PrivacyJob &job : catalog.nextPrivacyJobs(100, {})) {
        const PrivacyResult r = runPrivacy(job);
        catalog.applyPrivacy(r);
        ok += r.ok ? 1 : 0;
    }
    return ok;
}

/// KeyedListModel mínimo: linhas "chave=valor" (a chave antes do "=").
class KeyedStrings : public KeyedListModel
{
public:
    int rowCount(const QModelIndex &parent = {}) const override { return parent.isValid() ? 0 : int(rows.size()); }
    QVariant data(const QModelIndex &index, int role) const override
    {
        return index.isValid() && role == Qt::DisplayRole ? QVariant(rows.at(index.row())) : QVariant();
    }
    void set(const QStringList &next)
    {
        updateRows(rows, QList<QString>(next), [](const QString &r) { return r.section(u'=', 0, 0); });
    }
    QList<QString> rows;
};

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
        for (int i = 0; i < n; ++i) {
            // O ingest ignora o que já não está no disco: o arquivo existe.
            FoundFile f;
            f.path = folder + QStringLiteral("/p%1.jpg").arg(i);
            f.size = 100 + i;
            f.mtimeMs = 1000 + i;
            QFile touch(f.path);
            touch.open(QIODevice::WriteOnly);
            files.append(f);
        }
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
        for (const Catalog::Classifiable &c : catalog.classifiableCategories())
            QVERIFY(!c.prompts.isEmpty());
        // Ao abrir, a frase de uma versão anterior vira a atual; a que a
        // pessoa escreveu fica.
        const QString file = m_home.filePath(QStringLiteral("prompts.sqlite3"));
        {
            Catalog old;
            old.open(file);
            old.db().exec(QStringLiteral("UPDATE category SET prompt = 'people playing basketball' WHERE builtin_key = 'sports.basketball'"));
            old.db().exec(QStringLiteral("UPDATE category SET prompt = 'minha frase' WHERE builtin_key = 'sports.soccer'"));
        }
        Catalog reopened;
        reopened.open(file);
        QVERIFY(reopened.db().scalar(QStringLiteral("SELECT prompt FROM category WHERE builtin_key = 'sports.basketball'"))
                    .toString().contains(QLatin1String("hoop")));
        QCOMPARE(reopened.db().scalar(QStringLiteral("SELECT prompt FROM category WHERE builtin_key = 'sports.soccer'")).toString(),
                 QStringLiteral("minha frase"));
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
        catalog.setSetting(QStringLiteral("people/minPhotos"), 1);  // pessoas de 1-2 fotos aparecem
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
        catalog.setSetting(QStringLiteral("people/minPhotos"), 1);  // pessoas de 1-2 fotos aparecem
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
        catalog.setSetting(QStringLiteral("scan/minBytes"), 0);
        const QString folderId = catalog.addFolder(root);
        Pipeline pipeline(&catalog);
        pipeline.pauseAll();
        QSignalSpy scanned(&pipeline, &Pipeline::scanFinished);
        pipeline.scan({folderId});
        QTRY_COMPARE_WITH_TIMEOUT(scanned.size(), 1, 30000);
        qputenv("PHOTOIDX_DATA_DIR", previous);
        QCOMPARE(scanned.first().at(1).toInt(), 1);
    }

    void findCopies()
    {
        // A mesma imagem em outros lugares: hash perceptual a poucos bits
        // (marca d'água, recompressão), o CLIP separando os falsos.
        const QString file = m_home.filePath(QStringLiteral("copies.sqlite3"));
        QStringList p;
        {
            Catalog seed;
            seed.open(file);
            p = seedPhotos(seed, 6, m_photos.path() + QStringLiteral("/copies"));
            const quint64 base = 0x6297d1ed76c6c260ULL;
            auto flip = [base](int bits) {
                quint64 h = base;
                for (int i = 0; i < bits; ++i)
                    h ^= quint64(1) << (i * 7 % 64);
                return QStringLiteral("%1").arg(h, 16, 16, QLatin1Char('0'));
            };
            const int bits[] = {0, 2, 5, 10, 20, 2};
            for (int i = 0; i < 6; ++i)
                seed.db().exec(QStringLiteral("UPDATE photo SET phash = ?, picture_hash = ? WHERE id = ?"),
                               {flip(bits[i]), QStringLiteral("px%1").arg(i), p[i]});
            // CLIP: 0–4 a mesma cena; 5 tem o hash perto, mas é outra imagem.
            const imaging::Embedding scene = vec({1, 0.05f}), other = vec({0, 1});
            for (int i = 0; i < 6; ++i)
                seed.db().exec(QStringLiteral("INSERT INTO photo_embedding(photo_id, model, vector) VALUES(?, 'test', ?)"),
                               {p[i], imaging::toBlob(i == 5 ? other : i == 4 ? vec({1, 0.07f}) : scene)});
            seed.db().exec(QStringLiteral("UPDATE photo SET picture_hash = 'px0' WHERE id = ?"), {p[1]});
        }
        Catalog catalog;
        catalog.open(file);
        auto ids = [&](int tolerance) {
            QStringList out;
            for (const auto &[id, score] : catalog.findCopies(p[0], tolerance))
                out << id;
            return out;
        };
        // Idêntica: mesmos pixels (p1). A própria vem primeiro.
        QCOMPARE(ids(0), (QStringList{p[0], p[1]}));
        // Parecida: até 6 bits com o CLIP de acordo; p5 (CLIP diferente) fica fora.
        QCOMPARE(ids(1), (QStringList{p[0], p[1], p[2]}));
        // Ampla: até 12 bits (p3) e só pelo CLIP (p4, o "corte").
        const QStringList loose = ids(2);
        QVERIFY(loose.contains(p[3]) && loose.contains(p[4]));
        QVERIFY(!loose.contains(p[5]));
        // Privada: nada.
        catalog.db().exec(QStringLiteral("UPDATE photo SET private = 1 WHERE id = ?"), {p[0]});
        QVERIFY(catalog.findCopies(p[0], 1).isEmpty());
    }

    void fileOps()
    {
        // Modo Pastas: mover nunca sobrescreve, pasta não entra nela mesma,
        // nome livre "x (2).jpg".
        const QString root = m_photos.path() + QStringLiteral("/fileops");
        QDir().mkpath(root + QStringLiteral("/a/sub"));
        QDir().mkpath(root + QStringLiteral("/b"));
        QVERIFY(QFile::copy(data(QStringLiteral("sample.jpg")), root + QStringLiteral("/a/x.jpg")));
        QVERIFY(QFile::copy(data(QStringLiteral("sample.jpg")), root + QStringLiteral("/b/x.jpg")));
        QCOMPARE(fileops::moveFile(root + QStringLiteral("/a/x.jpg"), root + QStringLiteral("/b/x.jpg")), QStringLiteral("files.exists"));
        const QString free = fileops::freeName(root + QStringLiteral("/b"), QStringLiteral("x.jpg"));
        QCOMPARE(free, QStringLiteral("x (2).jpg"));
        QCOMPARE(fileops::moveFile(root + QStringLiteral("/a/x.jpg"), root + QStringLiteral("/b/") + free), QString());
        QVERIFY(!QFileInfo::exists(root + QStringLiteral("/a/x.jpg")));
        QVERIFY(QFileInfo::exists(root + QStringLiteral("/b/x (2).jpg")));
        QCOMPARE(fileops::moveDir(root + QStringLiteral("/a"), root + QStringLiteral("/a/sub/a"), nullptr), QStringLiteral("files.intoItself"));
        QList<fileops::Moved> moved;
        QCOMPARE(fileops::moveDir(root + QStringLiteral("/a"), root + QStringLiteral("/b/a"), &moved), QString());
        QVERIFY(QFileInfo(root + QStringLiteral("/b/a/sub")).isDir());
        QString created;
        QCOMPARE(fileops::makeDir(root + QStringLiteral("/b"), QStringLiteral("../x"), &created), QStringLiteral("files.badName"));
        QCOMPARE(fileops::makeDir(root + QStringLiteral("/b"), QStringLiteral("Nova"), &created), QString());
        QVERIFY(fileops::isInside(root + QStringLiteral("/b/Nova/y.jpg"), root + QStringLiteral("/b")));
        QVERIFY(!fileops::isInside(root + QStringLiteral("/bb/y.jpg"), root + QStringLiteral("/b")));
    }

    void catalogFollowsFiles()
    {
        // Modo Pastas: a foto continua a mesma (id) e o caminho acompanha.
        Catalog catalog;
        catalog.open(QString());
        const QString root = m_photos.path() + QStringLiteral("/follow");
        const QStringList photos = seedPhotos(catalog, 2, root);
        QString created;
        QCOMPARE(fileops::makeDir(root, QStringLiteral("b"), &created), QString());
        catalog.addExtraDir(created);
        auto keys = [&]() {
            QStringList out;
            for (const QVariant &v : catalog.tree(QStringLiteral("folders")))
                out << v.toMap().value(QStringLiteral("key")).toString();
            return out;
        };
        QVERIFY(keys().contains(QStringLiteral("dir:") + root + QStringLiteral("/b")));  // vazia, mas aparece
        QVERIFY(catalog.isRootFolder(root));
        QVERIFY(!catalog.rootFolderOf(root + QStringLiteral("/b")).isEmpty());
        QVERIFY(catalog.rootFolderOf(m_home.path()).isEmpty());
        QVERIFY(!catalog.onNetworkDisk(root + QStringLiteral("/b")));  // disco local
        // Foto para b/.
        const QString from = catalog.photoPaths({photos[0]}).first().second;
        const QString to = root + QStringLiteral("/b/p0.jpg");
        QCOMPARE(fileops::moveFile(from, to), QString());
        catalog.applyMoved({{from, to}});
        QCOMPARE(catalog.photoPaths({photos[0]}).first().second, to);
        QCOMPARE(catalog.db().scalar(QStringLiteral("SELECT dir FROM photo WHERE id = ?"), {photos[0]}).toString(), root + QStringLiteral("/b"));
        // Renomear a pasta b -> c: a foto vai junto.
        QList<fileops::Moved> moved;
        QCOMPARE(fileops::moveDir(root + QStringLiteral("/b"), root + QStringLiteral("/c"), &moved), QString());
        catalog.applyMoved(moved);
        QCOMPARE(catalog.photoPaths({photos[0]}).first().second, root + QStringLiteral("/c/p0.jpg"));
        QVERIFY(keys().contains(QStringLiteral("dir:") + root + QStringLiteral("/c")));
        QVERIFY(!keys().contains(QStringLiteral("dir:") + root + QStringLiteral("/b")));
        // Lixeira (aqui só o catálogo): a foto sai, a outra fica.
        catalog.removePaths({root + QStringLiteral("/c")});
        QVERIFY(catalog.photoPaths({photos[0]}).isEmpty());
        QCOMPARE(catalog.photoPaths({photos[1]}).size(), 1);
    }

    void personGroups()
    {
        // Grupos da pessoa na aba Pessoas: "Família" com a Ana; quem não tem
        // grupo fica em "Com nome"/"Sem nome". O reagrupamento não desfaz.
        Catalog catalog;
        catalog.open(QString());
        catalog.setSetting(QStringLiteral("people/minPhotos"), 1);
        const QStringList photos = seedPhotos(catalog, 3, m_photos.path() + QStringLiteral("/groups"));
        catalog.applyFaces(faces(photos[0], {vec({1, 0.01f})}));
        catalog.applyFaces(faces(photos[1], {vec({0, 1})}));
        catalog.applyFaces(faces(photos[2], {vec({0, 0, 1})}));
        auto personOf = [&](const QString &photo) {
            return catalog.db().scalar(QStringLiteral("SELECT person_id FROM face WHERE photo_id = ?"), {photo}).toString();
        };
        const QString ana = personOf(photos[0]), bob = personOf(photos[1]), kid = personOf(photos[2]);
        catalog.renamePerson(ana, QStringLiteral("Ana"));
        catalog.renamePerson(bob, QStringLiteral("Bob"));
        const QString family = catalog.createPersonGroup(QStringLiteral("Família"));
        catalog.setPersonGroup(ana, family);
        catalog.setPersonGroup(kid, family);  // sem nome, no grupo
        QCOMPARE(catalog.personGroupOf(ana), family);
        QHash<QString, QString> parentOf;
        for (const QVariant &v : catalog.tree(QStringLiteral("people"))) {
            const QVariantMap n = v.toMap();
            parentOf.insert(n.value(QStringLiteral("key")).toString(), n.value(QStringLiteral("parent")).toString());
        }
        QCOMPARE(parentOf.value(QStringLiteral("person:") + ana), QStringLiteral("pgroup:") + family);
        QCOMPARE(parentOf.value(QStringLiteral("person:") + kid), QStringLiteral("pgroup:") + family);
        QCOMPARE(parentOf.value(QStringLiteral("person:") + bob), QStringLiteral("people:named"));
        GridQuery q;
        q.node = QStringLiteral("pgroup:") + family;
        QCOMPARE(catalog.photos(q).size(), 2);
        q.node = QStringLiteral("people:named");
        QCOMPARE(catalog.photos(q).size(), 1);
        // Mesmo com o mínimo de fotos alto, quem está num grupo aparece.
        catalog.setSetting(QStringLiteral("people/minPhotos"), 50);
        bool kidListed = false;
        for (const QVariant &v : catalog.people())
            kidListed |= v.toMap().value(QStringLiteral("id")).toString() == kid;
        QVERIFY(kidListed);
        // Reagrupar mantém a pessoa sem nome do grupo.
        catalog.reclusterPeople();
        QCOMPARE(personOf(photos[2]), kid);
        // Apagar o grupo solta as pessoas.
        catalog.deletePersonGroup(family);
        QCOMPARE(catalog.personGroupOf(ana), QString());
        QVERIFY_THROWS_EXCEPTION(ActionError, catalog.createPersonGroup(QStringLiteral("  ")));
    }

    void photoPeopleAndFacesFirst()
    {
        // O menu da grade e o visualizador: as pessoas da foto e o estado da
        // etapa; "procurar rostos agora" só pega quem ainda está na fila.
        Catalog catalog;
        catalog.open(QString());
        catalog.setSetting(QStringLiteral("people/minPhotos"), 1);
        const QStringList photos = seedPhotos(catalog, 2, m_photos.path() + QStringLiteral("/photopeople"));
        catalog.db().exec(QStringLiteral("UPDATE photo SET state = 'indexed', thumb_path = 'x.jpg'"));
        QCOMPARE(catalog.photoPeople(photos[0]).value(QStringLiteral("facesState")).toString(), QStringLiteral("pending"));
        QCOMPARE(catalog.faceJobsFor(photos).size(), 2);
        QSignalSpy done(&catalog, &Catalog::facesDone);
        catalog.applyFaces(faces(photos[0], {vec({1, 0.01f}), vec({0, 1})}));
        QCOMPARE(done.size(), 1);
        const QVariantMap p = catalog.photoPeople(photos[0]);
        QCOMPARE(p.value(QStringLiteral("facesState")).toString(), QStringLiteral("done"));
        QCOMPARE(p.value(QStringLiteral("faces")).toInt(), 2);
        QCOMPARE(p.value(QStringLiteral("people")).toList().size(), 2);
        QCOMPARE(catalog.faceJobsFor(photos).size(), 1);  // a feita não volta
    }

    void vanishedFileWaitsForDisk()
    {
        // O original some entre a fila e a leitura (disco desconectado antes
        // de o app perceber): a foto volta para a fila, não vira erro.
        const QString root = m_photos.path() + QStringLiteral("/vanish");
        QDir().mkpath(root);
        QVERIFY(QFile::copy(data(QStringLiteral("sample.jpg")), root + QStringLiteral("/a.jpg")));
        Catalog catalog;
        catalog.open(QString());
        catalog.setSetting(QStringLiteral("scan/minBytes"), 0);
        const QString folderId = catalog.addFolder(root);
        Pipeline pipeline(&catalog);
        pipeline.pauseAll();
        QSignalSpy scanned(&pipeline, &Pipeline::scanFinished);
        QSignalSpy vanished(&pipeline, &Pipeline::filesVanished);
        pipeline.scan({folderId});
        QTRY_COMPARE_WITH_TIMEOUT(scanned.size(), 1, 30000);
        QVERIFY(QFile::remove(root + QStringLiteral("/a.jpg")));
        pipeline.start(QStringLiteral("index"));
        QTRY_VERIFY_WITH_TIMEOUT(vanished.size() >= 1, 30000);
        QTRY_VERIFY_WITH_TIMEOUT(!pipeline.busy(), 30000);
        QCOMPARE(catalog.db().scalar(QStringLiteral("SELECT state FROM photo")).toString(), QStringLiteral("pending"));
        QCOMPARE(vanished.size(), 1);  // estacionada: não tenta de novo em laço
    }

    void notThisPerson()
    {
        // Grade de uma pessoa, "Não é a pessoa": as faces dela nas fotos
        // escolhidas saem e não voltam com o reagrupamento.
        Catalog catalog;
        catalog.open(QString());
        catalog.setSetting(QStringLiteral("people/minPhotos"), 1);
        const QStringList photos = seedPhotos(catalog, 3, m_photos.path() + QStringLiteral("/notperson"));
        for (const QString &photo : photos)
            catalog.applyFaces(faces(photo, {vec({1, 0.05f})}));
        const QString person = catalog.people().value(0).toMap().value(QStringLiteral("id")).toString();
        QCOMPARE(catalog.personFaces(person).size(), 3);
        catalog.renamePerson(person, QStringLiteral("Ana"));  // com nome, o id sobrevive ao reagrupar
        QCOMPARE(catalog.rejectPersonInPhotos(person, {photos[0], photos[1]}), 2);
        QCOMPARE(catalog.personFaces(person).size(), 1);
        catalog.reclusterPeople();
        QCOMPARE(catalog.personFaces(person).size(), 1);
        QCOMPARE(catalog.rejectPersonInPhotos(person, {photos[0]}), 0);  // já não é dela
    }

    void scanSmallFoldersFirst()
    {
        // A varredura geral começa pelas pastas menores: uma pasta nova não
        // fica atrás de um compartilhamento de rede enorme.
        Catalog catalog;
        catalog.open(QString());
        seedPhotos(catalog, 5, m_photos.path() + QStringLiteral("/big"));
        const QString small = m_photos.path() + QStringLiteral("/small");
        QDir().mkpath(small);
        const QString smallId = catalog.addFolder(small);
        const QStringList ids = catalog.folderIds();
        QCOMPARE(ids.size(), 2);
        QCOMPARE(ids.first(), smallId);
    }

    void keyedListModelUpdates()
    {
        // Atualização sem reset (as grades não voltam ao topo): qualquer
        // troca termina nas linhas novas, com sinais coerentes (o tester do
        // Qt confere cada begin/end) e sem reset no meio.
        KeyedStrings model;
        QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
        QSignalSpy resets(&model, &QAbstractItemModel::modelReset);
        QSignalSpy changes(&model, &QAbstractItemModel::dataChanged);
        model.set({QStringLiteral("a=1"), QStringLiteral("b=1"), QStringLiteral("c=1")});
        QCOMPARE(resets.size(), 1);  // a primeira carga pode
        model.set({QStringLiteral("c=1"), QStringLiteral("a=2"), QStringLiteral("d=1")});
        QCOMPARE(model.rows, (QList<QString>{QStringLiteral("c=1"), QStringLiteral("a=2"), QStringLiteral("d=1")}));
        QCOMPARE(changes.size(), 1);  // só o "a" mudou de valor
        QRandomGenerator rng(42);
        for (int round = 0; round < 200; ++round) {
            QStringList keys;
            for (int k = 0; k < 30; ++k)
                if (rng.bounded(3) != 0)
                    keys << QString::number(k);
            std::shuffle(keys.begin(), keys.end(), rng);
            QStringList next;
            for (const QString &k : std::as_const(keys))
                next << k + u'=' + QString::number(rng.bounded(2));
            if (next.isEmpty())
                next << QStringLiteral("x=0");
            model.set(next);
            QCOMPARE(model.rows, QList<QString>(next));
        }
        QCOMPARE(resets.size(), 1);
    }

    void personTimeline()
    {
        // "Ao longo dos anos" (schema/0008.sql, core/Timeline): uma foto por
        // período, a melhor automática; a escolha da pessoa e o período tirado
        // ficam guardados; privadas e datas impossíveis ficam fora.
        Catalog catalog;
        catalog.open(QString());
        catalog.setSetting(QStringLiteral("people/minPhotos"), 1);
        const QStringList photos = seedPhotos(catalog, 6, m_photos.path() + QStringLiteral("/timeline"));
        for (int i = 0; i < photos.size(); ++i)
            catalog.applyFaces(faces(photos[i], {vec({1, 0.02f * float(i)})}));
        const QString person = catalog.people().value(0).toMap().value(QStringLiteral("id")).toString();
        QVERIFY(!person.isEmpty());
        QCOMPARE(catalog.personFaces(person).size(), 6);
        const int dates[6][2] = {{2015, 2}, {2015, 11}, {2016, 3}, {2016, 4}, {2076, 1}, {2017, 8}};
        for (int i = 0; i < photos.size(); ++i)
            catalog.db().exec(QStringLiteral("UPDATE photo SET year = ?, month = ?, width = 1000, height = 1000 WHERE id = ?"),
                              {dates[i][0], dates[i][1], photos[i]});
        catalog.db().exec(QStringLiteral("UPDATE photo SET private = 1 WHERE id = ?"), {photos[5]});
        // A foto 1 é a "melhor" de 2015 (mais parecida com a pessoa).
        catalog.db().exec(QStringLiteral("UPDATE face SET similarity = 0.9 WHERE photo_id = ?"), {photos[1]});
        catalog.db().exec(QStringLiteral("UPDATE face SET similarity = 0.6 WHERE photo_id <> ?"), {photos[1]});

        QList<timeline::Face> list = catalog.timelineFaces(person);
        QCOMPARE(list.size(), 4);  // sem a de 2076 e sem a privada
        auto groups = timeline::group(list, QStringLiteral("year"));
        QCOMPARE(groups.size(), 2);
        QCOMPARE(groups[0].first, QStringLiteral("2015"));
        QList<timeline::Pick> toSave;
        QList<timeline::Entry> entries = timeline::resolve(list, groups, catalog.timelinePicks(person, QStringLiteral("year")), &toSave);
        QCOMPARE(toSave.size(), 2);
        const auto faceOf = [&](const QString &photoId) {
            for (const auto &f : list)
                if (f.photoId == photoId)
                    return f.faceId;
            return QString();
        };
        QCOMPARE(entries[0].faceId, faceOf(photos[1]));
        QCOMPARE(entries[0].candidates, 2);
        catalog.saveTimelinePicks(person, QStringLiteral("year"), toSave);

        // A pessoa troca 2015 e tira 2016; relido do banco, nada muda.
        catalog.saveTimelinePicks(person, QStringLiteral("year"), {{QStringLiteral("2015"), faceOf(photos[0]), true, false},
                                                                   {QStringLiteral("2016"), entries[1].faceId, false, true}});
        toSave.clear();
        entries = timeline::resolve(list, groups, catalog.timelinePicks(person, QStringLiteral("year")), &toSave);
        QVERIFY(toSave.isEmpty());
        QCOMPARE(entries[0].faceId, faceOf(photos[0]));
        QVERIFY(entries[0].user);
        QVERIFY(entries[1].hidden);
        // Cada frequência tem as suas escolhas.
        QVERIFY(catalog.timelinePicks(person, QStringLiteral("quarter")).isEmpty());
        QCOMPARE(timeline::group(list, QStringLiteral("quarter")).size(), 4);
        QCOMPARE(timeline::periodOf(QStringLiteral("semester"), 2016, 7), QStringLiteral("2016-S2"));
        QCOMPARE(timeline::periodOf(QStringLiteral("month"), 2016, 3), QStringLiteral("2016-03"));

        // A foto escolhida fica privada: o período volta à escolha automática.
        catalog.db().exec(QStringLiteral("UPDATE photo SET private = 1 WHERE id = ?"), {photos[0]});
        list = catalog.timelineFaces(person);
        groups = timeline::group(list, QStringLiteral("year"));
        toSave.clear();
        entries = timeline::resolve(list, groups, catalog.timelinePicks(person, QStringLiteral("year")), &toSave);
        QCOMPARE(entries[0].faceId, faceOf(photos[1]));
        QVERIFY(!entries[0].user);
        QCOMPARE(toSave.size(), 1);

        // "Escolher de novo" esquece tudo; apagar a pessoa apaga as escolhas.
        catalog.resetTimeline(person, QStringLiteral("year"));
        QVERIFY(catalog.timelinePicks(person, QStringLiteral("year")).isEmpty());
        catalog.saveTimelinePicks(person, QStringLiteral("year"), toSave);
        catalog.deletePerson(person);
        QCOMPARE(catalog.db().scalar(QStringLiteral("SELECT count(*) FROM person_timeline")).toInt(), 0);
        QCOMPARE(timeline::autoColumns(9), 3);
        QCOMPARE(timeline::autoColumns(8), 3);
        QCOMPARE(timeline::autoColumns(4), 2);
    }

    void facesUnreadableRequeued()
    {
        // schema/0006.sql: o "image unreadable" das faces (disco fora) volta
        // para a fila; os outros erros ficam. (O passo roda sozinho: voltar o
        // user_version refaria também os passos seguintes.)
        Catalog reopened;
        reopened.open(QString());
        const QStringList photos = seedPhotos(reopened, 2, m_photos.path() + QStringLiteral("/requeue"));
        reopened.db().exec(QStringLiteral("UPDATE photo SET faces_state = 'error', error = 'Faces: image unreadable' WHERE id = ?"), {photos[0]});
        reopened.db().exec(QStringLiteral("UPDATE photo SET faces_state = 'error', error = 'Faces: internal error: x' WHERE id = ?"), {photos[1]});
        QFile step(QStringLiteral(":/photoidx/schema/0006.sql"));
        QVERIFY(step.open(QIODevice::ReadOnly));
        const QString sql = QString::fromUtf8(step.readAll());
        reopened.db().exec(sql.mid(sql.indexOf(QLatin1String("UPDATE"))).trimmed().chopped(1));
        QCOMPARE(reopened.db().scalar(QStringLiteral("SELECT count(*) FROM photo WHERE faces_state = 'pending' AND error = ''")).toInt(), 1);
        QCOMPARE(reopened.db().scalar(QStringLiteral("SELECT count(*) FROM photo WHERE faces_state = 'error'")).toInt(), 1);
    }

    void scanSkipsNoise()
    {
        // Ícones, recursos de programa e dependências não são fotos.
        const QString root = m_photos.path() + QStringLiteral("/noise");
        for (const char *dir : {"node_modules/pkg", "Foo.app/Contents/Resources", "Assets.xcassets/x.imageset", "fotos"})
            QDir().mkpath(root + u'/' + QString::fromLatin1(dir));
        QVERIFY(QFile::copy(data(QStringLiteral("sample.jpg")), root + QStringLiteral("/fotos/real.jpg")));
        QVERIFY(QFile::copy(data(QStringLiteral("exif/o1.jpg")), root + QStringLiteral("/fotos/icon.jpg")));  // 854 bytes
        for (const char *f : {"node_modules/pkg/a.jpg", "Foo.app/Contents/Resources/b.jpg", "Assets.xcassets/x.imageset/c.jpg"})
            QVERIFY(QFile::copy(data(QStringLiteral("sample.jpg")), root + u'/' + QString::fromLatin1(f)));
        // Extensão de imagem, conteúdo que não é: uma "imagem de disco" .raw e
        // um texto renomeado para .jpg.
        for (const char *f : {"fotos/disk.raw", "fotos/notes.jpg"}) {
            QFile fake(root + u'/' + QString::fromLatin1(f));
            QVERIFY(fake.open(QIODevice::WriteOnly));
            fake.write(QByteArray(64 * 1024, f[6] == 'd' ? '\0' : 'x'));
        }

        Catalog catalog;
        catalog.open(QString());
        catalog.setSetting(QStringLiteral("scan/minBytes"), 0);
        const QString folderId = catalog.addFolder(root);
        Pipeline pipeline(&catalog);
        pipeline.pauseAll();
        QSignalSpy scanned(&pipeline, &Pipeline::scanFinished);
        pipeline.scan({folderId});
        QTRY_COMPARE_WITH_TIMEOUT(scanned.size(), 1, 30000);
        QCOMPARE(scanned.last().at(1).toInt(), 2);  // real.jpg e icon.jpg; as pastas de programa, não
        // Com o tamanho mínimo padrão, o ícone some — e, como nunca foi
        // indexado, sai do catálogo (não fica como "sumido").
        catalog.setSetting(QStringLiteral("scan/minBytes"), 16 * 1024);
        pipeline.scan({folderId});
        QTRY_COMPARE_WITH_TIMEOUT(scanned.size(), 2, 30000);
        QCOMPARE(scanned.last().at(1).toInt(), 1);
        QCOMPARE(catalog.db().scalar(QStringLiteral("SELECT count(*) FROM photo")).toInt(), 1);
        QCOMPARE(catalog.stats().value(QStringLiteral("missing")).toInt(), 0);
    }

    void nestedTransactions()
    {
        // O lote do Indexer é uma transação com uma por foto dentro: a de
        // dentro que falha volta sozinha; a de fora grava o resto.
        Catalog catalog;
        catalog.open(QString());
        Database &db = catalog.db();
        {
            Transaction outer(db);
            catalog.setSetting(QStringLiteral("t/a"), 1);
            {
                Transaction inner(db);
                catalog.setSetting(QStringLiteral("t/b"), 2);
            }  // sem commit: desfeita
            {
                Transaction inner(db);
                catalog.setSetting(QStringLiteral("t/c"), 3);
                inner.commit();
            }
            outer.commit();
        }
        QCOMPARE(catalog.setting(QStringLiteral("t/a")).toInt(), 1);
        QVERIFY(!catalog.setting(QStringLiteral("t/b")).isValid());
        QCOMPARE(catalog.setting(QStringLiteral("t/c")).toInt(), 3);
        // As filas usam os índices parciais (schema/0003.sql).
        QSqlQuery plan = db.exec(QStringLiteral("EXPLAIN QUERY PLAN SELECT p.id FROM photo p WHERE p.state = 'pending' "
                                                "AND (p.private = 0 OR 0) ORDER BY p.added_at LIMIT 4"));
        QString detail;
        while (plan.next())
            detail += plan.value(3).toString() + u' ';
        QVERIFY2(detail.contains(QLatin1String("photo_queue_index")), qPrintable(detail));
    }

    void classifierRule()
    {
        // Cossenos do CLIP: 0,01 a mais = e¹ (~2,7×) na probabilidade.
        const ClassifierParams params;
        auto pick = [&](QList<CategoryScore> scores, float background = std::numeric_limits<float>::quiet_NaN()) {
            QStringList ids;
            for (const auto &[id, p] : pickCategories(scores, background, params))
                ids << id;
            return ids;
        };
        // Vencedora clara.
        QCOMPARE(pick({{"basketball", "sports", 0.30f}, {"volleyball", "sports", 0.28f}, {"beach", "nature", 0.24f}}),
                 QStringList{"basketball"});
        // A "nenhuma" vence: sem categoria (em vez de um palpite).
        QCOMPARE(pick({{"basketball", "sports", 0.25f}, {"beach", "nature", 0.24f}}, 0.29f), QStringList());
        // Empate entre irmãs (basquete × vôlei): o grupo.
        QCOMPARE(pick({{"basketball", "sports", 0.280f}, {"volleyball", "sports", 0.278f}, {"beach", "nature", 0.20f}}),
                 QStringList{"sports"});
        // Fraca sozinha, mas o grupo inteiro é forte: o grupo.
        QCOMPARE(pick({{"a", "event", 0.270f}, {"b", "event", 0.262f}, {"c", "event", 0.262f}, {"d", "event", 0.262f},
                       {"x", "nature", 0.20f}}),
                 QStringList{"event"});
        // Dois grupos diferentes acima do limiar: os dois; nunca duas irmãs.
        QCOMPARE(pick({{"basketball", "sports", 0.300f}, {"group", "people", 0.299f}, {"volleyball", "sports", 0.20f}}),
                 (QStringList{"basketball", "group"}));
        // Categorias na raiz (sem pai), cada uma abaixo do limiar: não há
        // grupo para cair.
        QCOMPARE(pick({{"mine", "", 0.25f}, {"other", "", 0.249f}, {"third", "", 0.248f}}), QStringList());
        // Nada abaixo do limiar entra (não há mais o "melhor palpite").
        QList<CategoryScore> flat;
        for (int i = 0; i < 20; ++i)
            flat.append({QString::number(i), QStringLiteral("g%1").arg(i), 0.25f});
        QCOMPARE(pick(flat), QStringList());
    }

    void moveToCategory()
    {
        Catalog catalog;
        catalog.open(QString());
        const QStringList photos = seedPhotos(catalog, 2, m_photos.path() + QStringLiteral("/move"));
        auto cat = [&](const char *key) {
            return catalog.db().scalar(QStringLiteral("SELECT id FROM category WHERE builtin_key = ?"), {QString::fromLatin1(key)}).toString();
        };
        const QString volley = cat("sports.volleyball"), soccer = cat("sports.soccer"), basket = cat("sports.basketball"),
                      sports = cat("sports"), group = cat("people.group"), mine = catalog.createCategory(QString(), QStringLiteral("Minha"));
        // A IA pôs vôlei e futebol; a pessoa pôs "Minha" e "Grupo".
        CategorizeResult r;
        r.photoId = photos[0];
        r.ok = true;
        r.categories = {{volley, 0.6f}, {soccer, 0.3f}};
        catalog.applyCategorize(r);
        catalog.addPhotosToCategory({photos[0]}, mine);
        catalog.addPhotosToCategory({photos[0]}, group);
        auto sources = [&](const QString &photo) {
            QMap<QString, QString> out;
            QSqlQuery q = catalog.db().exec(QStringLiteral("SELECT category_id, source FROM photo_category WHERE photo_id = ?"), {photo});
            while (q.next())
                out.insert(q.value(0).toString(), q.value(1).toString());
            return out;
        };
        // Visto dentro de "Esportes": sai de Esportes e das de dentro, vai
        // para basquete; as outras da pessoa ficam.
        catalog.movePhotosToCategory({photos[0]}, basket, sports);
        auto s0 = sources(photos[0]);
        QCOMPARE(s0.value(basket), QStringLiteral("user"));
        QCOMPARE(s0.value(volley), QStringLiteral("rejected"));
        QCOMPARE(s0.value(soccer), QStringLiteral("rejected"));
        QCOMPARE(s0.value(mine), QStringLiteral("user"));
        QCOMPARE(s0.value(group), QStringLiteral("user"));
        // A IA não devolve o vôlei rejeitado.
        catalog.applyCategorize(r);
        QCOMPARE(sources(photos[0]).value(volley), QStringLiteral("rejected"));
        GridQuery q;
        q.node = QStringLiteral("cat:") + volley;
        QCOMPARE(catalog.photos(q).size(), 0);
        q.node = QStringLiteral("cat:") + basket;
        QCOMPARE(catalog.photos(q).size(), 1);
        // Fora de uma categoria: sai de todas as que a IA pôs.
        CategorizeResult r1;
        r1.photoId = photos[1];
        r1.ok = true;
        r1.categories = {{volley, 0.7f}, {group, 0.5f}};
        catalog.applyCategorize(r1);
        catalog.movePhotosToCategory({photos[1]}, basket);
        auto s1 = sources(photos[1]);
        QCOMPARE(s1.value(basket), QStringLiteral("user"));
        QCOMPARE(s1.value(volley), QStringLiteral("rejected"));
        QCOMPARE(s1.value(group), QStringLiteral("rejected"));
    }

    void diskComesBackOnline()
    {
        // Disco que estava desconectado ao abrir e foi montado depois: o
        // refresh o marca online e diz qual voltou (o app revarre as pastas).
        Catalog catalog;
        catalog.open(QString());
        const QString dir = m_photos.path() + QStringLiteral("/back");
        QDir().mkpath(dir);
        const QString folderId = catalog.addFolder(dir);
        const QString diskId = catalog.db().scalar(QStringLiteral("SELECT disk_id FROM folder WHERE id = ?"), {folderId}).toString();
        catalog.db().exec(QStringLiteral("UPDATE disk SET online = 0 WHERE id = ?"), {diskId});
        QSignalSpy changed(&catalog, &Catalog::disksChanged);
        QStringList back, gone;
        QVERIFY(catalog.refreshDisks(&back, &gone) >= 1);
        QCOMPARE(back, QStringList{diskId});
        QVERIFY(gone.isEmpty());
        QCOMPARE(changed.size(), 1);
        QCOMPARE(catalog.db().scalar(QStringLiteral("SELECT online FROM disk WHERE id = ?"), {diskId}).toInt(), 1);
        QCOMPARE(catalog.folderIdsOnDisk(diskId), QStringList{folderId});
        // Nada mudou: nem aviso, nem sinal.
        back.clear();
        QCOMPARE(catalog.refreshDisks(&back), 0);
        QVERIFY(back.isEmpty());
        QCOMPARE(changed.size(), 1);
    }

    void readerReadsInAnotherThread()
    {
        // Catálogo em arquivo: o leitor abre outra conexão, só de leitura, numa
        // thread própria; vê o que a principal já gravou e não a bloqueia.
        const QString file = m_home.filePath(QStringLiteral("reader.sqlite3"));
        Catalog catalog;
        catalog.open(file);
        catalog.createAlbum(QString(), QStringLiteral("Viagem"));
        CatalogReader reader(catalog);
        QVERIFY(reader.threaded());
        QThread *mainThread = QThread::currentThread();
        QThread *workThread = nullptr;
        QVariantList albums;
        bool done = false;
        reader.read<QVariantList>(
            this,
            [&](const Catalog &c) {
                workThread = QThread::currentThread();
                return c.albumChoices();
            },
            [&](QVariantList list) {
                albums = list;
                done = QThread::currentThread() == mainThread;
            });
        QTRY_VERIFY(done);
        QVERIFY(workThread && workThread != mainThread);
        bool found = false;
        for (const QVariant &a : albums)
            found = found || a.toMap().value(QStringLiteral("name")).toString() == QLatin1String("Viagem");
        QVERIFY(found);
        // Em memória (sem segunda conexão possível): lê no próprio catálogo.
        Catalog memory;
        memory.open(QString());
        CatalogReader inline_(memory);
        QVERIFY(!inline_.threaded());
        int stats = -1;
        inline_.read<int>(this, [](const Catalog &c) { return c.stats().value(QStringLiteral("photos")).toInt(); },
                          [&](int n) { stats = n; });
        QCOMPARE(stats, 0);
    }

    void peopleListHidesSmallUnnamed()
    {
        // Sem nome e com menos de people/minPhotos fotos (padrão 20): fora da
        // lista (e do painel); com nome, sempre, e primeiro.
        Catalog catalog;
        catalog.open(QString());
        const QStringList photos = seedPhotos(catalog, 24, m_photos.path() + QStringLiteral("/many"));
        const auto big = vec({1, 0.1f}), small = vec({0, 1}), named = vec({0, 0, 1});
        for (int i = 0; i < 21; ++i)
            catalog.applyFaces(faces(photos[i], {big}));
        catalog.applyFaces(faces(photos[21], {small}));
        catalog.applyFaces(faces(photos[22], {named}));
        auto ids = [&]() {
            QStringList out;
            for (const QVariant &v : catalog.people())
                out << v.toMap().value(QStringLiteral("name")).toString() + u':' + v.toMap().value(QStringLiteral("photoCount")).toString();
            return out;
        };
        QCOMPARE(ids(), QStringList{QStringLiteral(":21")});
        // Dar nome a quem tem 1 foto: aparece, e na frente.
        QSqlQuery q = catalog.db().exec(QStringLiteral("SELECT person_id FROM face WHERE photo_id = ?"), {photos[22]});
        QVERIFY(q.next());
        catalog.renamePerson(q.value(0).toString(), QStringLiteral("Ana"));
        QCOMPARE(ids(), (QStringList{QStringLiteral("Ana:1"), QStringLiteral(":21")}));
        // O painel lateral segue a mesma regra.
        int persons = 0;
        for (const QVariant &n : catalog.tree(QStringLiteral("people")))
            persons += n.toMap().value(QStringLiteral("key")).toString().startsWith(QLatin1String("person:"));
        QCOMPARE(persons, 2);
        // Limite menor: as pequenas voltam.
        catalog.setSetting(QStringLiteral("people/minPhotos"), 1);
        QCOMPARE(catalog.people().size(), 3);
    }

    void faceClustererRules()
    {
        auto face = [](int photo, imaging::Embedding e, int fixed = -1) {
            ClusterFace f;
            f.photo = photo;
            f.embedding = std::move(e);
            f.fixed = fixed;
            return f;
        };
        // Sem cadeia: faces que "giram" aos poucos (vizinhas parecidas, as
        // pontas não) não viram uma pessoa só.
        std::vector<ClusterFace> chain;
        for (int i = 0; i < 40; ++i) {
            const float a = float(i) * 0.06f;  // ~3,4° por face, ~130° no total
            chain.push_back(face(i, vec({std::cos(a), std::sin(a)})));
        }
        ClusterParams params;
        params.mergeMinFaces = 1;
        params.mergeEvery = 5;
        const std::vector<int> l = clusterFaces(chain, 0, params);
        QVERIFY(l.front() != l.back());
        // Uma pessoa por foto: duas faces iguais na mesma foto, dois grupos.
        const std::vector<int> same = clusterFaces({face(7, vec({1})), face(7, vec({1})), face(8, vec({1}))}, 0, params);
        QVERIFY(same[0] != same[1]);
        QVERIFY(same[2] == same[0] || same[2] == same[1]);
        // Grupos fixos: recebem faces, mas dois fixos nunca se juntam.
        const std::vector<int> fixed = clusterFaces({face(1, vec({1}), 0), face(2, vec({1}), 1), face(3, vec({1}))}, 2, params);
        QCOMPARE(fixed[0], 0);
        QCOMPARE(fixed[1], 1);
        QVERIFY(fixed[2] == 0 || fixed[2] == 1);
    }

    void onePersonPerPhoto()
    {
        // Dois rostos iguais na mesma foto (gêmeos, reflexo, cartaz): nunca a
        // mesma pessoa. E o reagrupamento mantém quem tem nome.
        Catalog catalog;
        catalog.open(QString());
        catalog.setSetting(QStringLiteral("people/minPhotos"), 1);
        const QStringList photos = seedPhotos(catalog, 3, m_photos.path() + QStringLiteral("/twins"));
        catalog.applyFaces(faces(photos[0], {vec({1, 0.01f}), vec({1, 0.02f})}));
        QSqlQuery q = catalog.db().exec(QStringLiteral("SELECT count(DISTINCT person_id) FROM face WHERE photo_id = ?"), {photos[0]});
        QVERIFY(q.next());
        QCOMPARE(q.value(0).toInt(), 2);
        catalog.applyFaces(faces(photos[1], {vec({0, 1})}));
        QSqlQuery named = catalog.db().exec(QStringLiteral("SELECT person_id FROM face WHERE photo_id = ?"), {photos[1]});
        QVERIFY(named.next());
        const QString ana = named.value(0).toString();
        catalog.renamePerson(ana, QStringLiteral("Ana"));
        catalog.reclusterPeople();
        QCOMPARE(catalog.db().scalar(QStringLiteral("SELECT person_id FROM face WHERE photo_id = ?"), {photos[1]}).toString(), ana);
        QSqlQuery after = catalog.db().exec(QStringLiteral("SELECT count(DISTINCT person_id) FROM face WHERE photo_id = ?"), {photos[0]});
        QVERIFY(after.next());
        QCOMPARE(after.value(0).toInt(), 2);
        // Detecção fraca não forma pessoa (fica na foto e na busca).
        catalog.applyFaces(faces(photos[2], {vec({0.3f, 0.3f, 1})}, 0.5f));
        QVERIFY(catalog.db().scalar(QStringLiteral("SELECT person_id FROM face WHERE photo_id = ?"), {photos[2]}).isNull());
    }

    void cryptoFormat()
    {
        const QByteArray key = crypto::randomBytes(crypto::kKeyBytes);
        const QHash<QByteArray, QByteArray> keys{{crypto::keyId(key), key}};
        // Vazio, pequeno e com vários blocos (o bloco é 1 MiB).
        for (const qsizetype size : {qsizetype(0), qsizetype(5), qsizetype(3 * 1024 * 1024 + 17)}) {
            const QByteArray plain = crypto::randomBytes(int(size));
            const QByteArray sealed = crypto::encryptBytes(key, plain, QStringLiteral("IMG_0001.CR3"));
            QVERIFY(crypto::isEncrypted(sealed));
            QString name;
            const auto back = crypto::decryptBytes(keys, sealed, &name);
            QVERIFY(back);
            QCOMPARE(*back, plain);
            QCOMPARE(name, QStringLiteral("IMG_0001.CR3"));
            if (size == 0)
                continue;
            // Um bit trocado em qualquer lugar, ou o fim cortado: não abre.
            QByteArray tampered = sealed;
            tampered[tampered.size() / 2] = char(tampered[tampered.size() / 2] ^ 1);
            QVERIFY(!crypto::decryptBytes(keys, tampered));
            QVERIFY(!crypto::decryptBytes(keys, sealed.left(sealed.size() - 20)));
            QVERIFY(!crypto::decryptBytes(keys, sealed + "x"));
        }
        // Outra chave: não abre (e o erro diz que é a chave).
        const QByteArray other = crypto::randomBytes(crypto::kKeyBytes);
        QByteArray sealed = crypto::encryptBytes(key, "abc", {});
        QBuffer in(&sealed);
        in.open(QIODevice::ReadOnly);
        const crypto::StreamResult r = crypto::decryptStream({{crypto::keyId(other), other}}, in, nullptr);
        QVERIFY(!r.ok);
        QCOMPARE(r.error, QStringLiteral("crypto.wrongKey"));
        // A chave para guardar: ida e volta, tolerante a caixa e espaços; um
        // caractere errado é recusado pela verificação.
        const QString text = crypto::formatKey(key);
        QCOMPARE(text.size(), 14 * 4 + 13);
        QCOMPARE(crypto::parseKey(text), key);
        QCOMPARE(crypto::parseKey(text.toLower().replace(u'-', u' ')), key);
        QString typo = text;
        typo[0] = typo[0] == u'A' ? u'B' : u'A';
        QVERIFY(crypto::parseKey(typo).isEmpty());
        // PBKDF2-HMAC-SHA512, vetor conhecido (RFC 6070 adaptado ao SHA-512).
        QCOMPARE(crypto::deriveKey(QStringLiteral("password"), "salt", 1).toHex(),
                 QByteArray("867f70cf1ade02cff3752599a3a53dc4af34c7a669815ae5d513554e1c8cf252"));
    }

    void vaultPasswordAndRecovery()
    {
        Catalog catalog;
        catalog.open(QString());
        // A chave nasce com o catálogo, em claro (sem senha ainda).
        vault::Record record = catalog.vaultRecord();
        QVERIFY(!record.plain.isEmpty());
        QVERIFY(!catalog.hasVaultPassword());
        const auto ring = catalog.currentRing();
        QVERIFY(ring);
        const QByteArray key = ring->current();
        QCOMPARE(key.size(), crypto::kKeyBytes);
        QCOMPARE(catalog.vaultRecord().currentId, crypto::keyId(key));
        // Sem senha não dá para tornar privada.
        bool threw = false;
        try {
            catalog.makePrivate({QStringLiteral("x")});
        } catch (const ActionError &e) {
            threw = e.key() == QLatin1String("vault.noPassword");
        }
        QVERIFY(threw);
        // Senha definida: o chaveiro em claro some do banco.
        QByteArray kek;
        catalog.storeVault(vault::protect(*ring, QStringLiteral("s3nha forte"), 1000, &kek));
        record = catalog.vaultRecord();
        QVERIFY(catalog.hasVaultPassword());
        QVERIFY(record.plain.isEmpty());
        QVERIFY(!catalog.db().scalar(QStringLiteral("SELECT value FROM setting WHERE key = 'vault/wrapped'")).toString()
                     .contains(QString::fromLatin1(key.toBase64())));
        QVERIFY(!catalog.currentRing());  // trancado
        // Senha errada não abre; a certa devolve a mesma chave.
        QVERIFY(!vault::unwrap(record, vault::passwordKey(record, QStringLiteral("errada"))));
        const auto opened = vault::unwrap(record, vault::passwordKey(record, QStringLiteral("s3nha forte")));
        QVERIFY(opened);
        QCOMPARE(opened->current(), key);
        // Esqueceu a senha: a chave guardada confere pelo id e recebe senha nova.
        const QByteArray typed = crypto::parseKey(crypto::formatKey(key));
        QCOMPARE(crypto::keyId(typed), record.currentId);
        catalog.storeVault(vault::protect(vault::ringWith(typed), QStringLiteral("nova"), 1000));
        record = catalog.vaultRecord();
        QCOMPARE(vault::unwrap(record, vault::passwordKey(record, QStringLiteral("nova")))->current(), key);
    }

    void foreignKeyPhotos()
    {
        // .photoidxe cifrado por OUTRO catálogo: entra "trancado" (aparece na
        // pasta, não no resto), a etapa "privacy" não insiste nele; com a
        // chave daquele catálogo, é recifrado com a chave deste.
        Catalog other;
        other.open(QString());
        const QByteArray otherKey = other.currentRing()->current();
        const QString folder = m_photos.path() + QStringLiteral("/foreign");
        const QString file = folder + QStringLiteral("/image1.photoidxe");
        const QByteArray plain = crypto::randomBytes(5000);
        writeFile(file, crypto::encryptBytes(otherKey, plain, QStringLiteral("image1.jpg")));

        Catalog catalog;
        catalog.open(QString());
        QByteArray kek;
        const vault::Ring ring = *catalog.currentRing();
        catalog.storeVault(vault::protect(ring, QStringLiteral("pw"), 1000, &kek));
        catalog.openVault(ring, kek);
        const QString folderId = catalog.addFolder(folder);
        FoundFile f;
        f.path = file;
        f.size = QFileInfo(file).size();
        f.mtimeMs = 1000;
        f.keyId = QString::fromLatin1(crypto::keyId(otherKey).toHex());
        QCOMPARE(catalog.ingest(folderId, {f}), 1);

        auto rows = [&](const QString &node) {
            GridQuery q;
            q.node = node;
            return catalog.photos(q);
        };
        const QList<GridRow> inFolder = rows(QStringLiteral("dir:") + folder);
        QCOMPARE(inFolder.size(), 1);
        QVERIFY(inFolder[0].locked);
        QCOMPARE(rows(QStringLiteral("all")).size(), 0);  // só na pasta
        QCOMPARE(catalog.lockedCount(), 1);
        QVERIFY(catalog.nextPrivacyJobs(10, {}).isEmpty());  // sem a chave, não tenta

        // Chave errada (de nenhuma foto) e a certa.
        bool unused = false;
        try {
            catalog.addKey(crypto::randomBytes(32));
        } catch (const ActionError &e) {
            unused = e.key() == QLatin1String("vault.keyUnused");
        }
        QVERIFY(unused);
        QCOMPARE(catalog.addKey(otherKey), 1);
        QCOMPARE(runPrivacyJobs(catalog), 1);
        // Recifrado com a chave deste catálogo: abre só com ela.
        const QByteArray mineId = catalog.currentRing()->currentId;
        QCOMPARE(catalog.db().scalar(QStringLiteral("SELECT key_id FROM photo")).toString(), QString::fromLatin1(mineId.toHex()));
        const auto opened = crypto::decryptFile({{mineId, catalog.currentRing()->current()}}, file);
        QVERIFY(opened.has_value());
        QCOMPARE(*opened, plain);
        QCOMPARE(catalog.lockedCount(), 0);
    }

    void privatePhotoLifecycle()
    {
        Catalog catalog;
        catalog.open(QString());
        catalog.setSetting(QStringLiteral("people/minPhotos"), 1);  // pessoas de 1-2 fotos aparecem
        const QString folder = m_photos.path() + QStringLiteral("/private");
        const QStringList ids = seedPhotos(catalog, 2, folder);
        QCOMPARE(ids.size(), 2);
        const QString id = ids[0];
        const QString original = folder + QStringLiteral("/p0.jpg");
        const QByteArray originalBytes = crypto::randomBytes(150000);
        writeFile(original, originalBytes);
        // Indexada, com miniatura.
        const QString thumb = AppPaths::thumbnailPath(id, QDate(2024, 5, 3));
        const QByteArray thumbBytes = "\xFF\xD8\xFF fake jpeg thumbnail";
        writeFile(thumb, thumbBytes);
        catalog.db().exec(QStringLiteral("UPDATE photo SET state = 'indexed', thumb_path = ?, year = 2024, month = 5, day = 3, "
                                         "faces_state = 'done', size = ? WHERE id = ?"),
                          {thumb, originalBytes.size(), id});
        catalog.applyFaces(faces(id, {vec({1, 0.2f})}));
        QCOMPARE(catalog.people().size(), 1);

        // Senha e cofre aberto.
        QByteArray kek;
        const vault::Ring ring = *catalog.currentRing();
        catalog.storeVault(vault::protect(ring, QStringLiteral("pw"), 1000, &kek));
        catalog.openVault(ring, kek);
        QVERIFY(catalog.privateUnlocked());

        auto count = [&](const QString &node) {
            GridQuery q;
            q.node = node;
            return catalog.photos(q).size();
        };
        const QString privateNode = QStringLiteral("album:") + catalog.privateAlbumId();
        QCOMPARE(count(QStringLiteral("all")), 2);

        // Tornar privada: some de tudo na hora, aparece no álbum privado, as
        // faces saem das pessoas.
        QCOMPARE(catalog.makePrivate({id}), 1);
        QCOMPARE(count(QStringLiteral("all")), 1);
        QCOMPARE(count(QStringLiteral("year:2024")), 0);
        QCOMPARE(count(privateNode), 1);
        QCOMPARE(catalog.people().size(), 0);

        // A etapa "privacy" cifra: .photoidxe no mesmo lugar, original apagado.
        QCOMPARE(runPrivacyJobs(catalog), 1);
        const QString encrypted = folder + QStringLiteral("/p0.photoidxe");
        QVERIFY(!QFileInfo::exists(original));
        QVERIFY(QFileInfo::exists(encrypted));
        QSqlQuery row = catalog.db().exec(QStringLiteral("SELECT path, file_sha256, encrypted_sha256, thumb_path, private_state, "
                                                         "original_name, size FROM photo WHERE id = ?"),
                                          {id});
        QVERIFY(row.next());
        QCOMPARE(row.value(0).toString(), encrypted);
        QCOMPARE(row.value(1).toByteArray(), sha(originalBytes));            // hash antes
        QCOMPARE(row.value(2).toByteArray(), sha(readAll(encrypted)));       // hash depois
        QCOMPARE(row.value(4).toString(), QStringLiteral("done"));
        QCOMPARE(row.value(5).toString(), QStringLiteral("p0.jpg"));
        QCOMPARE(row.value(6).toLongLong(), QFileInfo(encrypted).size());
        // Miniatura cifrada (.pidx); a .jpg sumiu.
        const QString encryptedThumb = row.value(3).toString();
        QVERIFY(encryptedThumb.endsWith(QLatin1String(".pidx")));
        QVERIFY(!QFileInfo::exists(thumb));
        QVERIFY(!readAll(encryptedThumb).contains("fake jpeg"));
        QCOMPARE(*crypto::decryptBytes(catalog.keySnapshot().keys, readAll(encryptedThumb)), thumbBytes);
        QVERIFY(!readAll(encrypted).contains(originalBytes.left(64)));

        // Trancado: nem o álbum privado mostra, nem o detalhe, nem a contagem.
        catalog.lockVault();
        QVERIFY(!catalog.privateUnlocked());
        QCOMPARE(count(privateNode), 0);
        QVERIFY(catalog.photoDetail(id).isEmpty());
        bool hiddenCount = false;
        for (const QVariant &n : catalog.tree(QStringLiteral("albums")))
            if (n.toMap().value(QStringLiteral("key")) == privateNode)
                hiddenCount = n.toMap().value(QStringLiteral("count")).toInt() < 0;
        QVERIFY(hiddenCount);
        QVERIFY(catalog.nextPrivacyJobs(10, {}).isEmpty());

        // Abre de novo e troca a chave: a foto é recifrada; a chave velha sai.
        vault::Record record = catalog.vaultRecord();
        const QByteArray pk = vault::passwordKey(record, QStringLiteral("pw"));
        catalog.openVault(*vault::unwrap(record, pk), pk);
        const QByteArray oldId = catalog.keySnapshot().currentId;
        const QByteArray fresh = catalog.rotateKey();
        QCOMPARE(catalog.keySnapshot().keys.size(), 2);
        const QList<PrivacyJob> rekey = catalog.nextPrivacyJobs(10, {});
        QCOMPARE(rekey.size(), 1);
        QCOMPARE(rekey[0].op, QStringLiteral("rekey"));
        QCOMPARE(runPrivacyJobs(catalog), 1);
        QCOMPARE(catalog.keySnapshot().keys.size(), 1);
        QCOMPARE(catalog.keySnapshot().currentId, crypto::keyId(fresh));
        QVERIFY(catalog.keySnapshot().currentId != oldId);
        QVERIFY(crypto::decryptFile({{crypto::keyId(fresh), fresh}}, encrypted));

        // Pública de novo: o arquivo volta com o nome e os bytes originais.
        QCOMPARE(catalog.makePublic({id}), 1);
        QCOMPARE(runPrivacyJobs(catalog), 1);
        QVERIFY(QFileInfo::exists(original));
        QVERIFY(!QFileInfo::exists(encrypted));
        QCOMPARE(readAll(original), originalBytes);
        QCOMPARE(count(QStringLiteral("all")), 2);
        QCOMPARE(count(privateNode), 0);
        QCOMPARE(catalog.db().scalar(QStringLiteral("SELECT faces_state FROM photo WHERE id = ?"), {id}).toString(),
                 QStringLiteral("pending"));
        QVERIFY(QFileInfo::exists(thumb));

        // Queda no meio: o .photoidxe foi gravado mas o original não saiu e
        // o banco não soube. A rodada seguinte reaproveita o cifrado.
        QCOMPARE(catalog.makePrivate({id}), 1);
        const QList<PrivacyJob> jobs = catalog.nextPrivacyJobs(10, {});
        QCOMPARE(jobs.size(), 1);
        QVERIFY(runPrivacy(jobs[0]).ok);   // resultado "perdido"
        writeFile(original, originalBytes);  // e o original "ainda lá"
        QCOMPARE(runPrivacyJobs(catalog), 1);
        QVERIFY(!QFileInfo::exists(original));
        QVERIFY(QFileInfo::exists(encrypted));
        QVERIFY(!QFileInfo::exists(folder + QStringLiteral("/p0 (2).photoidxe")));

        // Um .photoidxe que o catálogo não conhece entra como privada.
        const QString foreign = folder + QStringLiteral("/other.photoidxe");
        writeFile(foreign, crypto::encryptBytes(fresh, "x", QStringLiteral("other.nef")));
        FoundFile f;
        f.path = foreign;
        f.size = QFileInfo(foreign).size();
        f.keyId = QString::fromLatin1(crypto::keyId(fresh).toHex());
        catalog.ingest(catalog.db().scalar(QStringLiteral("SELECT folder_id FROM photo WHERE id = ?"), {id}).toString(), {f});
        QCOMPARE(catalog.db().scalar(QStringLiteral("SELECT private FROM photo WHERE path = ?"), {foreign}).toInt(), 1);
        QCOMPARE(count(privateNode), 2);
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
        catalog.setSetting(QStringLiteral("scan/minBytes"), 0);  // os JPEGs de teste são pequenos
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
