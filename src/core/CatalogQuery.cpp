// SPDX-License-Identifier: BSD-2-Clause
#include "core/Catalog.h"

#include "core/Taxonomy.h"
#include "imaging/Hashes.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QLocale>
#include <QMap>
#include <QSqlQuery>
#include <QSqlRecord>

#include <algorithm>
#include <functional>
#include <map>

// Chaves dos nós (o que o QML guarda e devolve para filtrar a grade):
//
//   all                       todas as fotos
//   missing                   arquivos que sumiram do disco
//   errors                    fotos com erro em alguma etapa
//   disk:<id>                 fotos de um disco
//   dir:<caminho>             uma pasta e as de dentro dela
//   year:2024  month:2024-05  day:2024-05-03  date:none
//   cat:<id>                  categoria e as de dentro dela
//   album:<id>                álbum e os de dentro dele
//   country:BR  state:BR|São Paulo  city:<place id>  place:none
//   person:<id>  faces:any
//
// Foto privada (photo.private = 1) só aparece nos nós de álbum privado e só
// com o cofre aberto; em todo o resto, nem nas contagens.

namespace pix {

namespace {

/// Minúsculas e sem acento ("São Paulo" -> "sao paulo"): a busca não depende
/// de como a pessoa digitou.
QString fold(const QString &text)
{
    const QString decomposed = text.normalized(QString::NormalizationForm_D);
    QString out;
    out.reserve(decomposed.size());
    for (const QChar c : decomposed)
        if (c.category() != QChar::Mark_NonSpacing)
            out += c.toLower();
    return out;
}

QVariantMap node(const QString &key, const QString &parent, const QString &label, int count, int depth,
                 const QString &icon, const QString &detail = {}, const QString &kind = {})
{
    return {{QStringLiteral("key"), key},     {QStringLiteral("parent"), parent},
            {QStringLiteral("label"), label}, {QStringLiteral("count"), count},
            {QStringLiteral("depth"), depth}, {QStringLiteral("hasChildren"), false},
            {QStringLiteral("icon"), icon},   {QStringLiteral("detail"), detail},
            {QStringLiteral("kind"), kind}};
}

/// Marca ``hasChildren`` de quem é pai de alguém na lista.
void markParents(QVariantList &nodes)
{
    QSet<QString> parents;
    for (const QVariant &n : nodes)
        parents.insert(n.toMap().value(QStringLiteral("parent")).toString());
    for (QVariant &n : nodes) {
        QVariantMap m = n.toMap();
        if (parents.contains(m.value(QStringLiteral("key")).toString())) {
            m.insert(QStringLiteral("hasChildren"), true);
            n = m;
        }
    }
}

QString diskIcon(const QString &kind)
{
    if (kind == QLatin1String("network"))
        return QStringLiteral("server");
    if (kind == QLatin1String("removable"))
        return QStringLiteral("usb");
    return QStringLiteral("hard-drive");
}

QString capitalized(const QString &s)
{
    return s.isEmpty() ? s : s.left(1).toUpper() + s.mid(1);
}

/// Nome exibido de uma categoria: a tradução da embutida, salvo se renomeada.
QString displayCategory(const QString &builtinKey, bool renamed, const QString &name)
{
    if (!builtinKey.isEmpty() && !renamed) {
        const QString translated = builtinCategoryName(builtinKey);
        if (!translated.isEmpty())
            return translated;
    }
    return name;
}

} // namespace

QString Catalog::categoryName(const QString &categoryId) const
{
    QSqlQuery q = m_db.exec(QStringLiteral("SELECT builtin_key, renamed, name FROM category WHERE id = ?"), {categoryId});
    return q.next() ? displayCategory(q.value(0).toString(), q.value(1).toBool(), q.value(2).toString()) : QString();
}

// --------------------------------------------------------------------------- //
// Árvores
// --------------------------------------------------------------------------- //

QVariantList Catalog::tree(const QString &tab) const
{
    QVariantList out;
    const QLocale locale;

    if (tab == QLatin1String("folders")) {
        // Disco -> pasta indexada -> subpastas (só as que têm foto, e os caminhos até elas).
        QHash<QString, int> ownCount;
        QSqlQuery d = m_db.exec(QStringLiteral("SELECT dir, count(*) FROM photo WHERE state != 'missing' AND private = 0 GROUP BY dir"));
        while (d.next())
            ownCount.insert(d.value(0).toString(), d.value(1).toInt());
        QSqlQuery disks = m_db.exec(QStringLiteral("SELECT id, label, kind, protocol, server, share, fs_type, mount_point, online "
                                                   "FROM disk ORDER BY kind, label"));
        while (disks.next()) {
            const QString diskId = disks.value(0).toString();
            const QString diskKey = QStringLiteral("disk:") + diskId;
            const bool online = disks.value(8).toBool();
            const QString protocol = disks.value(3).toString();
            const QString detail = !protocol.isEmpty() && !disks.value(4).toString().isEmpty()
                ? QStringLiteral("%1://%2/%3").arg(protocol, disks.value(4).toString(), disks.value(5).toString())
                : QStringLiteral("%1 · %2").arg(QDir::toNativeSeparators(disks.value(7).toString()), disks.value(6).toString());
            const qsizetype diskIndex = out.size();
            out << node(diskKey, QString(), disks.value(1).toString(), 0, 0, diskIcon(disks.value(2).toString()), detail,
                        online ? QStringLiteral("disk") : QStringLiteral("offline"));
            int diskTotal = 0;
            QSqlQuery folders = m_db.exec(QStringLiteral("SELECT path FROM folder WHERE disk_id = ? ORDER BY path"), {diskId});
            while (folders.next()) {
                const QString root = folders.value(0).toString();
                // Contagem acumulada de cada pasta (ela + as de dentro).
                std::map<QString, int> total;
                total[root] += 0;
                for (auto it = ownCount.constBegin(); it != ownCount.constEnd(); ++it) {
                    const QString &dir = it.key();
                    if (dir != root && !dir.startsWith(root + u'/'))
                        continue;
                    QString p = dir;
                    for (;;) {
                        total[p] += it.value();
                        if (p == root)
                            break;
                        p = p.left(p.lastIndexOf(u'/'));
                    }
                }
                diskTotal += total[root];
                // std::map já sai em ordem de caminho: é a pré-ordem da árvore.
                for (const auto &[path, count] : total) {
                    const bool isRoot = path == root;
                    const int depth = isRoot ? 1 : 1 + int(path.mid(root.size()).count(u'/'));
                    const QString parent = isRoot ? diskKey : QStringLiteral("dir:") + path.left(path.lastIndexOf(u'/'));
                    out << node(QStringLiteral("dir:") + path, parent,
                                isRoot ? QDir::toNativeSeparators(path) : path.section(u'/', -1), count, depth,
                                isRoot ? QStringLiteral("folder-open") : QStringLiteral("folder"),
                                isRoot ? QDir::toNativeSeparators(path) : QString(), QStringLiteral("dir"));
                }
            }
            QVariantMap diskNode = out[diskIndex].toMap();
            diskNode.insert(QStringLiteral("count"), diskTotal);
            out[diskIndex] = diskNode;
        }
    } else if (tab == QLatin1String("dates")) {
        QSqlQuery q = m_db.exec(QStringLiteral("SELECT year, month, day, count(*) FROM photo WHERE state != 'missing' "
                                               "AND private = 0 AND year IS NOT NULL GROUP BY year, month, day "
                                               "ORDER BY year DESC, month DESC, day DESC"));
        QMap<int, int> years;
        QMap<QPair<int, int>, int> months;
        QList<std::tuple<int, int, int, int>> days;
        while (q.next()) {
            const int y = q.value(0).toInt(), m = q.value(1).toInt(), d = q.value(2).toInt(), c = q.value(3).toInt();
            years[y] += c;
            months[{y, m}] += c;
            days.append({y, m, d, c});
        }
        QList<int> yearList = years.keys();
        std::reverse(yearList.begin(), yearList.end());
        for (int y : yearList) {
            const QString yk = QStringLiteral("year:%1").arg(y);
            out << node(yk, QString(), QString::number(y), years[y], 0, QStringLiteral("calendar"));
            for (int m = 12; m >= 1; --m) {
                if (!months.contains({y, m}))
                    continue;
                const QString mk = QStringLiteral("month:%1-%2").arg(y).arg(m, 2, 10, QLatin1Char('0'));
                out << node(mk, yk, capitalized(locale.standaloneMonthName(m)), months[{y, m}], 1, QStringLiteral("calendar"));
                for (const auto &[dy, dm, dd, dc] : days) {
                    if (dy != y || dm != m)
                        continue;
                    const QDate date(dy, dm, dd);
                    out << node(QStringLiteral("day:%1-%2-%3").arg(dy).arg(dm, 2, 10, QLatin1Char('0')).arg(dd, 2, 10, QLatin1Char('0')),
                                mk, date.isValid() ? capitalized(locale.toString(date, QStringLiteral("d · dddd"))) : QString::number(dd),
                                dc, 2, QStringLiteral("calendar"));
                }
            }
        }
        const int undated = m_db.scalar(QStringLiteral("SELECT count(*) FROM photo WHERE state != 'missing' AND private = 0 "
                                                       "AND year IS NULL")).toInt();
        if (undated > 0)
            out << node(QStringLiteral("date:none"), QString(), QCoreApplication::translate("Catalog", "No date"), undated, 0,
                        QStringLiteral("calendar"), {}, QStringLiteral("special"));
    } else if (tab == QLatin1String("categories") || tab == QLatin1String("albums")) {
        const bool categories = tab == QLatin1String("categories");
        const QString table = categories ? QStringLiteral("category") : QStringLiteral("album");
        const QString link = categories ? QStringLiteral("photo_category") : QStringLiteral("album_photo");
        const QString col = categories ? QStringLiteral("category_id") : QStringLiteral("album_id");
        // Categorias: só públicas. Álbuns: o público conta as públicas; o
        // privado, as privadas (e só com o cofre aberto).
        const QString keep = categories ? QStringLiteral("AND l.source != 'rejected' AND p.private = 0")
                                        : QStringLiteral("AND p.private = (SELECT private FROM album WHERE id = tree.root)");
        // Contagem de cada nó = fotos distintas nele e nos de dentro.
        QSqlQuery counts = m_db.exec(QStringLiteral(
            "WITH RECURSIVE tree(root, id) AS (SELECT id, id FROM %1 UNION ALL "
            "SELECT tree.root, c.id FROM %1 c JOIN tree ON c.parent_id = tree.id) "
            "SELECT tree.root, count(DISTINCT l.photo_id) FROM tree JOIN %2 l ON l.%3 = tree.id "
            "JOIN photo p ON p.id = l.photo_id WHERE p.state != 'missing' %4 GROUP BY tree.root")
                                         .arg(table, link, col, keep));
        QHash<QString, int> count;
        while (counts.next())
            count.insert(counts.value(0).toString(), counts.value(1).toInt());
        const QString cols = categories ? QStringLiteral("id, parent_id, name, builtin_key, renamed, 0")
                                        : QStringLiteral("id, parent_id, name, '', 0, private");
        QSqlQuery q = m_db.exec(QStringLiteral("SELECT %1 FROM %2 ORDER BY position, name COLLATE NOCASE").arg(cols, table));
        struct Item
        {
            QString id, parent, label;
            bool builtin;
            bool isPrivate;
        };
        const bool unlocked = privateUnlocked();
        QHash<QString, QList<Item>> children;
        while (q.next()) {
            const bool isPrivate = q.value(5).toBool();
            const bool privateRoot = isPrivate && q.value(1).isNull();
            // Trancado: o álbum privado aparece, mas sem o que tem dentro.
            if (isPrivate && !privateRoot && !unlocked)
                continue;
            const QString label = privateRoot ? QCoreApplication::translate("Catalog", "Private")
                                              : displayCategory(q.value(3).toString(), q.value(4).toBool(), q.value(2).toString());
            Item item{q.value(0).toString(), q.value(1).toString(), label, !q.value(3).toString().isEmpty(), isPrivate};
            // O privado vem primeiro.
            if (privateRoot)
                children[item.parent].prepend(item);
            else
                children[item.parent].append(item);
        }
        const QString prefix = categories ? QStringLiteral("cat:") : QStringLiteral("album:");
        const QString icon = categories ? QStringLiteral("tag") : QStringLiteral("book-image");
        std::function<void(const QString &, int)> walk = [&](const QString &parent, int depth) {
            for (const Item &item : children.value(parent)) {
                QString kind = item.builtin ? QStringLiteral("builtin") : QStringLiteral("custom");
                QString itemIcon = icon;
                int itemCount = count.value(item.id);
                if (item.isPrivate) {
                    const bool root = parent.isEmpty();
                    kind = root ? (unlocked ? QStringLiteral("private-open") : QStringLiteral("private-locked"))
                                : QStringLiteral("private");
                    itemIcon = root ? (unlocked ? QStringLiteral("lock-open") : QStringLiteral("lock")) : icon;
                    if (!unlocked)
                        itemCount = -1;  // trancado: nem a contagem
                }
                out << node(prefix + item.id, parent.isEmpty() ? QString() : prefix + parent, item.label, itemCount, depth,
                            itemIcon, {}, kind);
                walk(item.id, depth + 1);
            }
        };
        walk(QString(), 0);
    } else if (tab == QLatin1String("places")) {
        QSqlQuery q = m_db.exec(QStringLiteral("SELECT pl.id, pl.country_code, pl.country, pl.state, pl.city, count(*) "
                                               "FROM photo p JOIN place pl ON pl.id = p.place_id WHERE p.state != 'missing' AND p.private = 0 "
                                               "GROUP BY pl.id ORDER BY pl.country, pl.state, pl.city"));
        struct City
        {
            QString id, name;
            int count;
        };
        std::map<QString, std::pair<QString, std::map<QString, std::vector<City>>>> countries;  // nome -> (código, estados)
        while (q.next()) {
            auto &country = countries[q.value(2).toString()];
            country.first = q.value(1).toString();
            country.second[q.value(3).toString()].push_back({q.value(0).toString(), q.value(4).toString(), q.value(5).toInt()});
        }
        for (const auto &[country, data] : countries) {
            const QString ck = QStringLiteral("country:") + data.first;
            int ctotal = 0;
            const qsizetype ci = out.size();
            out << node(ck, QString(), country, 0, 0, QStringLiteral("globe"));
            for (const auto &[state, cities] : data.second) {
                const QString sk = QStringLiteral("state:%1|%2").arg(data.first, state);
                int stotal = 0;
                const qsizetype si = out.size();
                out << node(sk, ck, state.isEmpty() ? QCoreApplication::translate("Catalog", "(no state)") : state, 0, 1,
                            QStringLiteral("map"));
                for (const City &c : cities) {
                    out << node(QStringLiteral("city:") + c.id, sk, c.name, c.count, 2, QStringLiteral("map-pin"));
                    stotal += c.count;
                }
                QVariantMap s = out[si].toMap();
                s.insert(QStringLiteral("count"), stotal);
                out[si] = s;
                ctotal += stotal;
            }
            QVariantMap c = out[ci].toMap();
            c.insert(QStringLiteral("count"), ctotal);
            out[ci] = c;
        }
        const int without = m_db.scalar(QStringLiteral("SELECT count(*) FROM photo WHERE state != 'missing' AND private = 0 "
                                                       "AND place_id IS NULL")).toInt();
        if (without > 0)
            out << node(QStringLiteral("place:none"), QString(), QCoreApplication::translate("Catalog", "No location"), without, 0,
                        QStringLiteral("map-pin-off"), {}, QStringLiteral("special"));
    } else if (tab == QLatin1String("people")) {
        const int withFaces = m_db.scalar(QStringLiteral("SELECT count(*) FROM photo WHERE state != 'missing' AND private = 0 "
                                                         "AND face_count > 0")).toInt();
        out << node(QStringLiteral("faces:any"), QString(), QCoreApplication::translate("Catalog", "Photos with faces"), withFaces, 0,
                    QStringLiteral("scan-face"), {}, QStringLiteral("special"));
        QSqlQuery q = m_db.exec(QStringLiteral("SELECT id, name, photo_count FROM person WHERE face_count > 0 OR name != '' "
                                               "ORDER BY (name = ''), name COLLATE NOCASE, face_count DESC"));
        int unnamed = 0;
        while (q.next()) {
            QString label = q.value(1).toString();
            if (label.isEmpty())
                label = QCoreApplication::translate("Catalog", "Person %1").arg(++unnamed);
            out << node(QStringLiteral("person:") + q.value(0).toString(), QString(), label, q.value(2).toInt(), 0,
                        QStringLiteral("user"), {}, q.value(1).toString().isEmpty() ? QStringLiteral("unnamed") : QStringLiteral("named"));
        }
    }
    markParents(out);
    return out;
}

// --------------------------------------------------------------------------- //
// Grade
// --------------------------------------------------------------------------- //

QString Catalog::nodeFilter(const QString &key, QVariantList &params) const
{
    const QString type = key.section(u':', 0, 0);
    const QString value = key.section(u':', 1);
    if (key.isEmpty() || key == QLatin1String("all"))
        return QStringLiteral("p.state != 'missing' AND p.private = 0");
    if (key == QLatin1String("missing"))
        return QStringLiteral("p.state = 'missing' AND p.private = 0");
    if (key == QLatin1String("errors"))
        return QStringLiteral("(p.state = 'error' OR p.ai_state = 'error' OR p.faces_state = 'error') AND p.private = 0");
    // Álbum privado: só com o cofre aberto, e lá só há privadas.
    if (type == QLatin1String("album") && isPrivateAlbum(value)) {
        if (!privateUnlocked())
            return QStringLiteral("0 = 1");
        params << value;
        return QStringLiteral("p.state != 'missing' AND p.private = 1 AND p.id IN (WITH RECURSIVE sub(id) AS (SELECT ? "
                              "UNION ALL SELECT a.id FROM album a JOIN sub ON a.parent_id = sub.id) "
                              "SELECT l.photo_id FROM album_photo l WHERE l.album_id IN (SELECT id FROM sub))");
    }
    QString cond;
    if (type == QLatin1String("disk")) {
        cond = QStringLiteral("p.folder_id IN (SELECT id FROM folder WHERE disk_id = ?)");
        params << value;
    } else if (type == QLatin1String("dir")) {
        cond = QStringLiteral("(p.dir = ? OR substr(p.dir, 1, length(?) + 1) = ? || '/')");
        params << value << value << value;
    } else if (type == QLatin1String("year")) {
        cond = QStringLiteral("p.year = ?");
        params << value.toInt();
    } else if (type == QLatin1String("month")) {
        cond = QStringLiteral("p.year = ? AND p.month = ?");
        params << value.section(u'-', 0, 0).toInt() << value.section(u'-', 1, 1).toInt();
    } else if (type == QLatin1String("day")) {
        cond = QStringLiteral("p.year = ? AND p.month = ? AND p.day = ?");
        params << value.section(u'-', 0, 0).toInt() << value.section(u'-', 1, 1).toInt() << value.section(u'-', 2, 2).toInt();
    } else if (key == QLatin1String("date:none")) {
        cond = QStringLiteral("p.year IS NULL");
    } else if (type == QLatin1String("cat")) {
        cond = QStringLiteral("p.id IN (WITH RECURSIVE sub(id) AS (SELECT ? UNION ALL SELECT c.id FROM category c "
                              "JOIN sub ON c.parent_id = sub.id) SELECT l.photo_id FROM photo_category l "
                              "WHERE l.source != 'rejected' AND l.category_id IN (SELECT id FROM sub))");
        params << value;
    } else if (type == QLatin1String("album")) {
        cond = QStringLiteral("p.id IN (WITH RECURSIVE sub(id) AS (SELECT ? UNION ALL SELECT a.id FROM album a "
                              "JOIN sub ON a.parent_id = sub.id) SELECT l.photo_id FROM album_photo l "
                              "WHERE l.album_id IN (SELECT id FROM sub))");
        params << value;
    } else if (type == QLatin1String("country")) {
        cond = QStringLiteral("p.place_id IN (SELECT id FROM place WHERE country_code = ?)");
        params << value;
    } else if (type == QLatin1String("state")) {
        cond = QStringLiteral("p.place_id IN (SELECT id FROM place WHERE country_code = ? AND state = ?)");
        params << value.section(u'|', 0, 0) << value.section(u'|', 1);
    } else if (type == QLatin1String("city")) {
        cond = QStringLiteral("p.place_id = ?");
        params << value;
    } else if (key == QLatin1String("place:none")) {
        cond = QStringLiteral("p.place_id IS NULL");
    } else if (type == QLatin1String("person")) {
        cond = QStringLiteral("p.id IN (SELECT photo_id FROM face WHERE person_id = ?)");
        params << value;
    } else if (key == QLatin1String("faces:any")) {
        cond = QStringLiteral("p.face_count > 0");
    } else {
        cond = QStringLiteral("1 = 1");
    }
    return QStringLiteral("p.state != 'missing' AND p.private = 0 AND ") + cond;
}

QString Catalog::searchFilter(const QString &search, QVariantList &params) const
{
    const QStringList tokens = search.simplified().split(u' ', Qt::SkipEmptyParts);
    if (tokens.isEmpty())
        return {};
    // Nomes que a busca entende além das colunas: categorias (traduzidas e o
    // nome gravado), lugares, pessoas e álbuns, comparados sem acento.
    struct Named
    {
        QString id;
        QString text;
    };
    QList<Named> categories, places, people, albums;
    {
        QSqlQuery q = m_db.exec(QStringLiteral("SELECT id, builtin_key, renamed, name FROM category"));
        while (q.next())
            categories.append({q.value(0).toString(),
                               fold(displayCategory(q.value(1).toString(), q.value(2).toBool(), q.value(3).toString()) + u' '
                                    + q.value(3).toString())});
        QSqlQuery p = m_db.exec(QStringLiteral("SELECT id, country || ' ' || state || ' ' || city || ' ' || country_code FROM place"));
        while (p.next())
            places.append({p.value(0).toString(), fold(p.value(1).toString())});
        QSqlQuery r = m_db.exec(QStringLiteral("SELECT id, name FROM person WHERE name != ''"));
        while (r.next())
            people.append({r.value(0).toString(), fold(r.value(1).toString())});
        QSqlQuery a = m_db.exec(QStringLiteral("SELECT id, name FROM album WHERE private = 0 OR ?"), {privateUnlocked() ? 1 : 0});
        while (a.next())
            albums.append({a.value(0).toString(), fold(a.value(1).toString())});
    }
    auto matching = [](const QList<Named> &list, const QString &token) {
        QStringList ids;
        for (const Named &n : list)
            if (n.text.contains(token))
                ids << n.id;
        return ids;
    };
    auto inList = [&params](const QString &column, const QStringList &ids) {
        if (ids.isEmpty())
            return QString();
        for (const QString &id : ids)
            params << id;
        return QStringLiteral(" OR %1 IN (%2)").arg(column, QStringList(ids.size(), QStringLiteral("?")).join(u','));
    };

    QStringList clauses;
    for (const QString &raw : tokens) {
        const QString token = fold(raw);
        const QString like = u'%' + raw + u'%';
        QString c = QStringLiteral("(p.name LIKE ? OR p.dir LIKE ? OR p.camera LIKE ? OR p.lens LIKE ? OR p.make LIKE ? "
                                   "OR p.artist LIKE ? OR p.description LIKE ? OR p.format LIKE ?");
        for (int i = 0; i < 8; ++i)
            params << like;
        bool isYear = false;
        const int year = raw.toInt(&isYear);
        if (isYear && raw.size() == 4) {
            c += QStringLiteral(" OR p.year = ?");
            params << year;
        }
        const QStringList cats = matching(categories, token);
        if (!cats.isEmpty()) {
            c += QStringLiteral(" OR p.id IN (SELECT photo_id FROM photo_category WHERE source != 'rejected' AND category_id IN (%1))")
                     .arg(QStringList(cats.size(), QStringLiteral("?")).join(u','));
            for (const QString &id : cats)
                params << id;
        }
        c += inList(QStringLiteral("p.place_id"), matching(places, token));
        const QStringList persons = matching(people, token);
        if (!persons.isEmpty()) {
            c += QStringLiteral(" OR p.id IN (SELECT photo_id FROM face WHERE person_id IN (%1))")
                     .arg(QStringList(persons.size(), QStringLiteral("?")).join(u','));
            for (const QString &id : persons)
                params << id;
        }
        const QStringList albumIds = matching(albums, token);
        if (!albumIds.isEmpty()) {
            c += QStringLiteral(" OR p.id IN (SELECT photo_id FROM album_photo WHERE album_id IN (%1))")
                     .arg(QStringList(albumIds.size(), QStringLiteral("?")).join(u','));
            for (const QString &id : albumIds)
                params << id;
        }
        clauses << c + u')';
    }
    return clauses.join(QStringLiteral(" AND "));
}

namespace {

GridRow rowFrom(const QSqlQuery &q)
{
    GridRow r;
    r.id = q.value(0).toString();
    r.name = q.value(1).toString();
    r.thumbPath = q.value(2).toString();
    r.width = q.value(3).toInt();
    r.height = q.value(4).toInt();
    r.isRaw = q.value(5).toBool();
    r.takenAt = q.value(6).toString();
    r.state = q.value(7).toString();
    r.isPrivate = q.value(8).toBool();
    return r;
}

const char *const kGridColumns = "p.id, p.name, p.thumb_path, p.width, p.height, p.is_raw, p.taken_at, p.state, p.private";

} // namespace

QList<GridRow> Catalog::photos(const GridQuery &query) const
{
    QVariantList params;
    QString where = nodeFilter(query.node, params);
    const QString search = searchFilter(query.search, params);
    if (!search.isEmpty())
        where += QStringLiteral(" AND ") + search;
    const QString rules = rulesFilter(query.rules, query.matchAll, params);
    if (!rules.isEmpty())
        where += QStringLiteral(" AND ") + rules;
    static const QHash<QString, QString> orders{
        {QStringLiteral("date-desc"), QStringLiteral("p.taken_at IS NULL, p.taken_at DESC, p.name")},
        {QStringLiteral("date-asc"), QStringLiteral("p.taken_at IS NULL, p.taken_at ASC, p.name")},
        {QStringLiteral("name"), QStringLiteral("p.name COLLATE NOCASE, p.dir")},
        {QStringLiteral("added"), QStringLiteral("p.added_at DESC, p.name")},
        {QStringLiteral("size"), QStringLiteral("p.size DESC, p.name")},
        {QStringLiteral("resolution"), QStringLiteral("p.width * p.height DESC, p.name")},
        {QStringLiteral("folder"), QStringLiteral("p.dir, p.name COLLATE NOCASE")},
    };
    const QString order = orders.value(query.sort, orders.value(QStringLiteral("date-desc")));
    QSqlQuery q = m_db.exec(QStringLiteral("SELECT %1 FROM photo p WHERE %2 ORDER BY %3")
                                .arg(QLatin1String(kGridColumns), where, order),
                            params);
    QList<GridRow> out;
    while (q.next())
        out.append(rowFrom(q));
    return out;
}

QList<GridRow> Catalog::photosByIds(const QList<QPair<QString, float>> &ranked) const
{
    QHash<QString, GridRow> rows;
    for (qsizetype start = 0; start < ranked.size(); start += 500) {
        QVariantList params;
        const qsizetype n = std::min<qsizetype>(500, ranked.size() - start);
        for (qsizetype i = 0; i < n; ++i)
            params << ranked[start + i].first;
        QSqlQuery q = m_db.exec(QStringLiteral("SELECT %1 FROM photo p WHERE p.state != 'missing' AND p.private = 0 AND p.id IN (%2)")
                                    .arg(QLatin1String(kGridColumns), QStringList(n, QStringLiteral("?")).join(u',')),
                                params);
        while (q.next()) {
            GridRow r = rowFrom(q);
            rows.insert(r.id, r);
        }
    }
    QList<GridRow> out;
    for (const auto &[id, score] : ranked) {
        auto it = rows.find(id);
        if (it == rows.end())
            continue;
        it->score = score;
        out.append(*it);
        rows.erase(it);  // a mesma foto (duas faces) entra uma vez só
    }
    return out;
}

// --------------------------------------------------------------------------- //
// Detalhe
// --------------------------------------------------------------------------- //

QString Catalog::photoPath(const QString &photoId) const
{
    return m_db.scalar(QStringLiteral("SELECT path FROM photo WHERE id = ?"), {photoId}).toString();
}

QVariantMap Catalog::photoDetail(const QString &photoId) const
{
    QSqlQuery q = m_db.exec(QStringLiteral(
                                "SELECT p.*, d.label AS disk_label, d.kind AS disk_kind, d.protocol AS disk_protocol, "
                                "d.server AS disk_server, d.share AS disk_share, d.mount_point AS disk_mount, d.online AS disk_online, "
                                "d.disk_key AS disk_key, pl.country, pl.state AS place_state, pl.city, pl.country_code "
                                "FROM photo p JOIN folder f ON f.id = p.folder_id JOIN disk d ON d.id = f.disk_id "
                                "LEFT JOIN place pl ON pl.id = p.place_id WHERE p.id = ?"),
                            {photoId});
    if (!q.next())
        return {};
    // Privada com o cofre trancado: nada.
    if (q.value(QStringLiteral("private")).toInt() == 1 && !privateUnlocked())
        return {};
    QVariantMap out;
    const QSqlRecord rec = q.record();
    for (int i = 0; i < rec.count(); ++i) {
        // snake_case -> camelCase para o QML.
        QString name = rec.fieldName(i);
        QString camel;
        bool up = false;
        for (const QChar c : name) {
            if (c == u'_') {
                up = true;
                continue;
            }
            camel += up ? c.toUpper() : c;
            up = false;
        }
        out.insert(camel, q.value(i));
    }
    out.insert(QStringLiteral("path"), QDir::toNativeSeparators(q.value(QStringLiteral("path")).toString()));
    out.insert(QStringLiteral("dir"), QDir::toNativeSeparators(q.value(QStringLiteral("dir")).toString()));

    QVariantList categories;
    QSqlQuery c = m_db.exec(QStringLiteral("SELECT c.id, c.builtin_key, c.renamed, c.name, l.source, l.score FROM photo_category l "
                                           "JOIN category c ON c.id = l.category_id WHERE l.photo_id = ? AND l.source != 'rejected' "
                                           "ORDER BY l.score DESC"),
                            {photoId});
    while (c.next())
        categories << QVariantMap{{QStringLiteral("id"), c.value(0)},
                                  {QStringLiteral("name"), displayCategory(c.value(1).toString(), c.value(2).toBool(), c.value(3).toString())},
                                  {QStringLiteral("source"), c.value(4)},
                                  {QStringLiteral("score"), c.value(5)}};
    out.insert(QStringLiteral("categories"), categories);

    QVariantList albums;
    QSqlQuery a = m_db.exec(QStringLiteral("SELECT a.id, a.name FROM album_photo l JOIN album a ON a.id = l.album_id "
                                           "WHERE l.photo_id = ? ORDER BY a.name"),
                            {photoId});
    while (a.next())
        albums << QVariantMap{{QStringLiteral("id"), a.value(0)}, {QStringLiteral("name"), a.value(1)}};
    out.insert(QStringLiteral("albums"), albums);

    QVariantList faces;
    QSqlQuery f = m_db.exec(QStringLiteral("SELECT f.id, f.person_id, pe.name, f.x, f.y, f.w, f.h, f.score, f.crop_path, f.assignment "
                                           "FROM face f LEFT JOIN person pe ON pe.id = f.person_id WHERE f.photo_id = ? "
                                           "ORDER BY f.x"),
                            {photoId});
    while (f.next())
        faces << QVariantMap{{QStringLiteral("id"), f.value(0)},       {QStringLiteral("personId"), f.value(1)},
                             {QStringLiteral("personName"), f.value(2)}, {QStringLiteral("x"), f.value(3)},
                             {QStringLiteral("y"), f.value(4)},        {QStringLiteral("w"), f.value(5)},
                             {QStringLiteral("h"), f.value(6)},        {QStringLiteral("score"), f.value(7)},
                             {QStringLiteral("cropPath"), f.value(8)}, {QStringLiteral("assignment"), f.value(9)}};
    out.insert(QStringLiteral("faces"), faces);

    QVariantList exif;
    QSqlQuery e = m_db.exec(QStringLiteral("SELECT ifd, tag, name, value FROM exif_tag WHERE photo_id = ? ORDER BY "
                                           "CASE ifd WHEN 'IFD0' THEN 0 WHEN 'Exif' THEN 1 WHEN 'GPS' THEN 2 ELSE 3 END, ifd, tag"),
                            {photoId});
    while (e.next())
        exif << QVariantMap{{QStringLiteral("ifd"), e.value(0)},
                            {QStringLiteral("tag"), QStringLiteral("0x%1").arg(e.value(1).toInt(), 4, 16, QLatin1Char('0'))},
                            {QStringLiteral("name"), e.value(2)},
                            {QStringLiteral("value"), e.value(3)}};
    out.insert(QStringLiteral("exif"), exif);
    return out;
}

QVariantList Catalog::similarPhotos(const QString &photoId, int maxDistance) const
{
    QVariantList out;
    QSqlQuery me = m_db.exec(QStringLiteral("SELECT picture_hash, phash, private FROM photo WHERE id = ?"), {photoId});
    if (!me.next())
        return out;
    // Parecidas de uma pública: só públicas. De uma privada (cofre aberto): só privadas.
    const int scope = me.value(2).toInt();
    if (scope == 1 && !privateUnlocked())
        return out;
    const QString pictureHash = me.value(0).toString();
    QHash<QString, int> found;
    QSet<QString> identical;
    if (!pictureHash.isEmpty()) {
        QSqlQuery q = m_db.exec(QStringLiteral("SELECT id FROM photo WHERE picture_hash = ? AND id != ? AND state != 'missing' "
                                               "AND private = ?"),
                                {pictureHash, photoId, scope});
        while (q.next()) {
            found.insert(q.value(0).toString(), 0);
            identical.insert(q.value(0).toString());
        }
    }
    bool ok = false;
    const quint64 phash = imaging::hashFromHex(me.value(1).toString(), &ok);
    if (ok && !me.value(1).toString().isEmpty())
        for (const HashTree::Match &m : m_hashes.query(phash, maxDistance))
            if (m.id != photoId && !found.contains(m.id))
                found.insert(m.id, m.distance);
    QList<QPair<QString, int>> ordered;
    for (auto it = found.constBegin(); it != found.constEnd(); ++it)
        ordered.append({it.key(), it.value()});
    std::sort(ordered.begin(), ordered.end(), [](const auto &a, const auto &b) { return a.second < b.second; });
    for (const auto &[id, distance] : ordered) {
        QSqlQuery r = m_db.exec(QStringLiteral("SELECT name, thumb_path, path, private FROM photo WHERE id = ? AND state != 'missing' "
                                               "AND private = ?"),
                                {id, scope});
        if (!r.next())
            continue;
        out << QVariantMap{{QStringLiteral("id"), id},
                           {QStringLiteral("name"), r.value(0)},
                           {QStringLiteral("thumbPath"), r.value(1)},
                           {QStringLiteral("path"), QDir::toNativeSeparators(r.value(2).toString())},
                           {QStringLiteral("distance"), distance},
                           {QStringLiteral("identical"), identical.contains(id)},
                           {QStringLiteral("isPrivate"), r.value(3).toBool()}};
    }
    return out;
}

QVariantMap Catalog::stats() const
{
    QSqlQuery q = m_db.exec(QStringLiteral(
        "SELECT count(*), "
        "  sum(state = 'indexed'), sum(is_raw AND state != 'missing'), sum(state = 'error' OR ai_state = 'error' OR faces_state = 'error'), "
        "  sum(state = 'missing'), sum(ai_state = 'done'), sum(faces_state = 'done'), sum(face_count) "
        "FROM photo WHERE private = 0"));
    q.next();
    return {{QStringLiteral("photos"), q.value(0).toInt() - q.value(4).toInt()},
            {QStringLiteral("indexed"), q.value(1).toInt()},
            {QStringLiteral("raw"), q.value(2).toInt()},
            {QStringLiteral("errors"), q.value(3).toInt()},
            {QStringLiteral("missing"), q.value(4).toInt()},
            {QStringLiteral("categorized"), q.value(5).toInt()},
            {QStringLiteral("facesDone"), q.value(6).toInt()},
            {QStringLiteral("faces"), q.value(7).toInt()},
            {QStringLiteral("people"), m_db.scalar(QStringLiteral("SELECT count(*) FROM person WHERE face_count > 0")).toInt()},
            {QStringLiteral("named"), m_db.scalar(QStringLiteral("SELECT count(*) FROM person WHERE name != ''")).toInt()},
            {QStringLiteral("folders"), m_db.scalar(QStringLiteral("SELECT count(*) FROM folder")).toInt()},
            {QStringLiteral("disks"), m_db.scalar(QStringLiteral("SELECT count(*) FROM disk")).toInt()}};
}

QVariantList Catalog::errors(int limit) const
{
    QVariantList out;
    QSqlQuery q = m_db.exec(QStringLiteral("SELECT id, name, path, state, ai_state, faces_state, error, private_state FROM photo "
                                           "WHERE (state = 'error' OR ai_state = 'error' OR faces_state = 'error' "
                                           "OR private_state LIKE 'error:%') AND (private = 0 OR ?) "
                                           "ORDER BY indexed_at DESC LIMIT ?"),
                            {privateUnlocked() ? 1 : 0, limit});
    while (q.next()) {
        const QString stage = q.value(7).toString().startsWith(QLatin1String("error:")) ? QStringLiteral("privacy")
                              : q.value(3).toString() == QLatin1String("error")           ? QStringLiteral("index")
                              : q.value(4).toString() == QLatin1String("error")           ? QStringLiteral("categorize")
                                                                                          : QStringLiteral("faces");
        out << QVariantMap{{QStringLiteral("id"), q.value(0)},
                           {QStringLiteral("name"), q.value(1)},
                           {QStringLiteral("path"), QDir::toNativeSeparators(q.value(2).toString())},
                           {QStringLiteral("stage"), stage},
                           {QStringLiteral("error"), q.value(6)}};
    }
    return out;
}

} // namespace pix
