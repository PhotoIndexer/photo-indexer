// SPDX-License-Identifier: BSD-2-Clause
#include "core/Catalog.h"
#include "core/Perf.h"

#include <QSqlQuery>

#include <functional>

namespace pix {

namespace {

/// ``candidate`` está dentro de ``ancestor`` (ou é ele)?
bool isDescendant(const Database &db, const QString &table, const QString &candidate, const QString &ancestor)
{
    QString current = candidate;
    for (int guard = 0; !current.isEmpty() && guard < 1000; ++guard) {
        if (current == ancestor)
            return true;
        current = db.scalar(QStringLiteral("SELECT parent_id FROM %1 WHERE id = ?").arg(table), {current}).toString();
    }
    return false;
}

QVariant parentOrNull(const QString &id)
{
    return id.isEmpty() ? QVariant() : QVariant(id);
}

} // namespace

void Catalog::guardAlbum(const QString &albumId) const
{
    if (albumId == privateAlbumId())
        throw ActionError(QStringLiteral("album.privateRoot"));
    if (isPrivateAlbum(albumId) && !privateUnlocked())
        throw ActionError(QStringLiteral("vault.locked"));
}

// --------------------------------------------------------------------------- //
// Categorias
// --------------------------------------------------------------------------- //

QString Catalog::createCategory(const QString &parentId, const QString &name)
{
    const QString clean = name.trimmed();
    if (clean.isEmpty())
        throw ActionError(QStringLiteral("category.emptyName"));
    const QString id = Database::newId();
    const int position = m_db.scalar(QStringLiteral("SELECT coalesce(max(position), 0) + 1 FROM category")).toInt();
    m_db.exec(QStringLiteral("INSERT INTO category(id, parent_id, name, renamed, prompt, auto, position, created_at) "
                             "VALUES(?, ?, ?, 1, '', 1, ?, ?)"),
              {id, parentOrNull(parentId), clean, position, Database::timestamp()});
    // A IA reavalia as fotos com a categoria nova (rápido: o embedding de cada
    // foto já está guardado; só a comparação é refeita).
    requeue(QStringLiteral("categorize"), QStringLiteral("all"));
    emit treeChanged(QStringLiteral("categories"));
    return id;
}

void Catalog::renameCategory(const QString &categoryId, const QString &name)
{
    const QString clean = name.trimmed();
    if (clean.isEmpty())
        throw ActionError(QStringLiteral("category.emptyName"));
    m_db.exec(QStringLiteral("UPDATE category SET name = ?, renamed = 1 WHERE id = ?"), {clean, categoryId});
    // Sem frase própria, o nome é a frase do CLIP: muda o que a IA procura.
    if (m_db.scalar(QStringLiteral("SELECT prompt FROM category WHERE id = ?"), {categoryId}).toString().isEmpty())
        requeue(QStringLiteral("categorize"), QStringLiteral("all"));
    emit treeChanged(QStringLiteral("categories"));
}

void Catalog::deleteCategory(const QString &categoryId)
{
    m_db.exec(QStringLiteral("DELETE FROM category WHERE id = ?"), {categoryId});
    m_builtinIds.removeIf([&](const auto &it) { return it.value() == categoryId; });
    emit treeChanged(QStringLiteral("categories"));
    emit photosChanged();
}

void Catalog::moveCategory(const QString &categoryId, const QString &newParentId)
{
    if (!newParentId.isEmpty() && isDescendant(m_db, QStringLiteral("category"), newParentId, categoryId))
        throw ActionError(QStringLiteral("category.cycle"));
    m_db.exec(QStringLiteral("UPDATE category SET parent_id = ? WHERE id = ?"), {parentOrNull(newParentId), categoryId});
    emit treeChanged(QStringLiteral("categories"));
}

void Catalog::setCategoryPrompt(const QString &categoryId, const QString &prompt)
{
    m_db.exec(QStringLiteral("UPDATE category SET prompt = ? WHERE id = ?"), {prompt.trimmed(), categoryId});
    requeue(QStringLiteral("categorize"), QStringLiteral("all"));
}

void Catalog::addPhotosToCategory(const QStringList &photoIds, const QString &categoryId)
{
    PIX_PERF("Catalog::addPhotosToCategory");
    Transaction tx(m_db);
    for (const QString &id : photoIds) {
        m_db.exec(QStringLiteral("INSERT INTO photo_category(photo_id, category_id, score, source) VALUES(?, ?, 1, 'user') "
                                 "ON CONFLICT(photo_id, category_id) DO UPDATE SET source = 'user', score = 1"),
                  {id, categoryId});
        applyRuleCategories(id);
    }
    tx.commit();
    emit photosChanged();
}

void Catalog::removePhotosFromCategory(const QStringList &photoIds, const QString &categoryId)
{
    Transaction tx(m_db);
    for (const QString &id : photoIds) {
        // Posta pela pessoa: sai. Posta pela IA/regra: fica marcada como
        // rejeitada, para a IA não pôr de novo.
        m_db.exec(QStringLiteral("DELETE FROM photo_category WHERE photo_id = ? AND category_id = ? AND source = 'user'"),
                  {id, categoryId});
        m_db.exec(QStringLiteral("UPDATE photo_category SET source = 'rejected' WHERE photo_id = ? AND category_id = ?"),
                  {id, categoryId});
        applyRuleCategories(id);
    }
    tx.commit();
    emit photosChanged();
}

void Catalog::movePhotosToCategory(const QStringList &photoIds, const QString &categoryId, const QString &fromCategoryId)
{
    PIX_PERF("Catalog::movePhotosToCategory");
    // As categorias de onde a foto sai: a de origem e as de dentro dela.
    QStringList from;
    if (!fromCategoryId.isEmpty()) {
        QSqlQuery q = m_db.exec(QStringLiteral("WITH RECURSIVE sub(id) AS (SELECT ? UNION ALL SELECT c.id FROM category c "
                                               "JOIN sub ON c.parent_id = sub.id) SELECT id FROM sub"),
                                {fromCategoryId});
        while (q.next())
            from << q.value(0).toString();
        from.removeAll(categoryId);  // mover para dentro da própria: não tira a de destino
    }
    const QString placeholders = QStringList(from.size(), QStringLiteral("?")).join(u',');
    Transaction tx(m_db);
    for (const QString &id : photoIds) {
        if (fromCategoryId.isEmpty()) {
            m_db.exec(QStringLiteral("UPDATE photo_category SET source = 'rejected' WHERE photo_id = ? AND source = 'ai' "
                                     "AND category_id != ?"),
                      {id, categoryId});
        } else if (!from.isEmpty()) {
            QVariantList params{id};
            for (const QString &c : from)
                params << c;
            m_db.exec(QStringLiteral("DELETE FROM photo_category WHERE photo_id = ? AND source = 'user' AND category_id IN (%1)")
                          .arg(placeholders),
                      params);
            m_db.exec(QStringLiteral("UPDATE photo_category SET source = 'rejected' WHERE photo_id = ? AND source IN ('ai', 'rule') "
                                     "AND category_id IN (%1)")
                          .arg(placeholders),
                      params);
        }
        m_db.exec(QStringLiteral("INSERT INTO photo_category(photo_id, category_id, score, source) VALUES(?, ?, 1, 'user') "
                                 "ON CONFLICT(photo_id, category_id) DO UPDATE SET source = 'user', score = 1"),
                  {id, categoryId});
        applyRuleCategories(id);
    }
    tx.commit();
    emit photosChanged();
    emit treeChanged(QStringLiteral("categories"));
}

QVariantList Catalog::categoryChoices() const
{
    QVariantList out;
    for (const QVariant &v : tree(QStringLiteral("categories"))) {
        const QVariantMap n = v.toMap();
        out << QVariantMap{{QStringLiteral("id"), n.value(QStringLiteral("key")).toString().mid(4)},
                           {QStringLiteral("name"), n.value(QStringLiteral("label"))},
                           {QStringLiteral("depth"), n.value(QStringLiteral("depth"))}};
    }
    return out;
}

// --------------------------------------------------------------------------- //
// Álbuns
// --------------------------------------------------------------------------- //

QString Catalog::createAlbum(const QString &parentId, const QString &name)
{
    const QString clean = name.trimmed();
    if (clean.isEmpty())
        throw ActionError(QStringLiteral("album.emptyName"));
    // Sub-álbum do privado é privado (e só com o cofre aberto).
    const bool isPrivate = !parentId.isEmpty() && isPrivateAlbum(parentId);
    if (isPrivate && !privateUnlocked())
        throw ActionError(QStringLiteral("vault.locked"));
    const QString id = Database::newId();
    const int position = m_db.scalar(QStringLiteral("SELECT coalesce(max(position), 0) + 1 FROM album")).toInt();
    m_db.exec(QStringLiteral("INSERT INTO album(id, parent_id, name, position, created_at, private) VALUES(?, ?, ?, ?, ?, ?)"),
              {id, parentOrNull(parentId), clean, position, Database::timestamp(), isPrivate ? 1 : 0});
    emit treeChanged(QStringLiteral("albums"));
    return id;
}

void Catalog::renameAlbum(const QString &albumId, const QString &name)
{
    guardAlbum(albumId);
    const QString clean = name.trimmed();
    if (clean.isEmpty())
        throw ActionError(QStringLiteral("album.emptyName"));
    m_db.exec(QStringLiteral("UPDATE album SET name = ? WHERE id = ?"), {clean, albumId});
    emit treeChanged(QStringLiteral("albums"));
}

void Catalog::deleteAlbum(const QString &albumId)
{
    guardAlbum(albumId);
    m_db.exec(QStringLiteral("DELETE FROM album WHERE id = ?"), {albumId});
    emit treeChanged(QStringLiteral("albums"));
    emit photosChanged();
}

void Catalog::moveAlbum(const QString &albumId, const QString &newParentId)
{
    guardAlbum(albumId);
    if (!newParentId.isEmpty() && isDescendant(m_db, QStringLiteral("album"), newParentId, albumId))
        throw ActionError(QStringLiteral("album.cycle"));
    // Um álbum não atravessa a fronteira do privado (as fotos dele não mudam
    // de visibilidade por arrastar uma pasta).
    const bool toPrivate = !newParentId.isEmpty() && isPrivateAlbum(newParentId);
    if (toPrivate != isPrivateAlbum(albumId))
        throw ActionError(QStringLiteral("album.privateBoundary"));
    m_db.exec(QStringLiteral("UPDATE album SET parent_id = ? WHERE id = ?"), {parentOrNull(newParentId), albumId});
    emit treeChanged(QStringLiteral("albums"));
}

void Catalog::addPhotosToAlbum(const QStringList &photoIds, const QString &albumId)
{
    PIX_PERF("Catalog::addPhotosToAlbum");
    const bool toPrivate = isPrivateAlbum(albumId);
    // Pôr num álbum privado = tornar privada (cifra o original e a miniatura).
    if (toPrivate)
        makePrivate(photoIds);
    Transaction tx(m_db);
    const QString now = Database::timestamp();
    for (const QString &id : photoIds)
        m_db.exec(QStringLiteral("INSERT OR IGNORE INTO album_photo(album_id, photo_id, added_at) "
                                 "SELECT ?, id, ? FROM photo WHERE id = ? AND private = ?"),
                  {albumId, now, id, toPrivate ? 1 : 0});
    tx.commit();
    emit treeChanged(QStringLiteral("albums"));
    emit photosChanged();
}

void Catalog::removePhotosFromAlbum(const QStringList &photoIds, const QString &albumId)
{
    // Tirar do álbum "Private" (a raiz) = voltar a ser pública.
    if (albumId == privateAlbumId()) {
        makePublic(photoIds);
        return;
    }
    Transaction tx(m_db);
    for (const QString &id : photoIds)
        m_db.exec(QStringLiteral("DELETE FROM album_photo WHERE album_id = ? AND photo_id = ?"), {albumId, id});
    tx.commit();
    emit treeChanged(QStringLiteral("albums"));
    emit photosChanged();
}

QVariantList Catalog::albumChoices() const
{
    QVariantList out;
    for (const QVariant &v : tree(QStringLiteral("albums"))) {
        const QVariantMap n = v.toMap();
        out << QVariantMap{{QStringLiteral("id"), n.value(QStringLiteral("key")).toString().mid(6)},
                           {QStringLiteral("name"), n.value(QStringLiteral("label"))},
                           {QStringLiteral("depth"), n.value(QStringLiteral("depth"))}};
    }
    return out;
}

} // namespace pix
