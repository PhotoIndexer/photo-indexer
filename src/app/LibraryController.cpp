// SPDX-License-Identifier: BSD-2-Clause
#include "LibraryController.h"

#include "core/Perf.h"

#include "core/Catalog.h"
#include "core/CatalogReader.h"
#include "core/GridReader.h"
#include "core/Pipeline.h"

#include <QCoreApplication>
#include <QFutureWatcher>
#include <QLoggingCategory>
#include <QSettings>
#include <QtConcurrent/QtConcurrentRun>

#include <utility>

Q_LOGGING_CATEGORY(lcLibrary, "photoidx.library")

using namespace pix;

namespace {

/// Similaridade mínima (cosseno do CLIP) para a busca por IA mostrar a foto.
constexpr float kAiMinScore = 0.20f;
constexpr int kAiLimit = 600;

} // namespace

LibraryController::LibraryController(Catalog *catalog, Pipeline *pipeline, CatalogReader *reader, QObject *parent)
    : QObject(parent)
    , m_catalog(catalog)
    , m_pipeline(pipeline)
{
    QSettings settings;
    m_thumbSize = std::clamp(settings.value(QStringLiteral("library/thumbSize"), 180).toInt(), 80, 480);
    m_tab = settings.value(QStringLiteral("library/tab"), QStringLiteral("folders")).toString();
    m_sort = settings.value(QStringLiteral("library/sort"), QStringLiteral("date-desc")).toString();
    m_nodeLabel = tr("All photos");
    m_copiesTolerance = std::clamp(settings.value(QStringLiteral("library/copiesTolerance"), 1).toInt(), 0, 2);

    // Uma thread só para ler a grade, que vive o app inteiro (a conexão de
    // leitura é dela).
    m_gridPool.setMaxThreadCount(1);
    m_gridPool.setExpiryTimeout(-1);
    m_reader = reader;
    m_refreshTimer.setSingleShot(true);
    connect(&m_refreshTimer, &QTimer::timeout, this, &LibraryController::refresh);
    m_treeTimer.setSingleShot(true);
    connect(&m_treeTimer, &QTimer::timeout, this, [this]() {
        if (m_staleTabs.contains(m_tab) || m_staleTabs.contains(QStringLiteral("*")))
            refreshTree();
        m_staleTabs.clear();
    });
    connect(catalog, &Catalog::photosChanged, this, [this]() {
        scheduleRefresh();
        scheduleTree(QStringLiteral("*"));
    });
    connect(catalog, &Catalog::treeChanged, this, [this](const QString &tab) {
        scheduleTree(tab);
        if (tab == QLatin1String("categories") || tab == QLatin1String("albums"))
            scheduleChoices(tab);
    });
    // Disco que entrou ou saiu: a marca de "offline" troca já (a árvore de
    // fundo, indexando, só se refaz a cada 20 s).
    connect(catalog, &Catalog::disksChanged, this, [this]() {
        if (m_tab == QLatin1String("folders"))
            refreshTree();
        else
            scheduleTree(QStringLiteral("folders"));
    });
    connect(catalog, &Catalog::peopleChanged, this, [this]() {
        scheduleTree(QStringLiteral("people"));
        scheduleChoices(QStringLiteral("people"));
    });
    m_choicesTimer.setSingleShot(true);
    connect(&m_choicesTimer, &QTimer::timeout, this, &LibraryController::readChoices);
    refreshTree();
    refresh();
    m_staleChoices = {QStringLiteral("categories"), QStringLiteral("albums"), QStringLiteral("people")};
    readChoices();
}

bool LibraryController::aiAvailable() const
{
    return m_pipeline->clip().available();
}

void LibraryController::scheduleRefresh()
{
    // Indexando: no máximo um refresh a cada 5 s (refazer a grade de um
    // catálogo grande custa); parado, quase na hora.
    if (!m_refreshTimer.isActive()) {
        m_backgroundRefresh = true;
        m_refreshTimer.start(m_pipeline->busy() ? 5000 : 150);
    }
}

void LibraryController::scheduleTree(const QString &tab)
{
    m_staleTabs.insert(tab);
    if (!m_treeTimer.isActive())
        m_treeTimer.start(m_pipeline->busy() ? 20000 : 150);
}

void LibraryController::setTab(const QString &tab)
{
    if (tab == m_tab)
        return;
    m_tab = tab;
    QSettings().setValue(QStringLiteral("library/tab"), tab);
    refreshTree();
    emit tabChanged();
}

void LibraryController::refreshTree()
{
    // A consulta roda no leitor; a árvore troca quando chega (e só se a aba
    // ainda é a mesma e nenhum pedido mais novo saiu).
    const QString tab = m_tab;
    const int serial = ++m_treeSerial;
    m_reader->read<QVariantList>(
        this, [tab](const Catalog &c) { return c.tree(tab); },
        [this, tab, serial](QVariantList nodes) {
            if (serial != m_treeSerial || tab != m_tab)
                return;
            PIX_PERF("LibraryController::setTree");
            m_tree.setNodes(nodes);
            // O nome do nó escolhido pode ter mudado (renomeado, contagem).
            const QVariantMap n = m_tree.node(m_node);
            if (!n.isEmpty() && n.value(QStringLiteral("label")).toString() != m_nodeLabel) {
                m_nodeLabel = n.value(QStringLiteral("label")).toString();
                emit nodeChanged();
            }
        });
}

void LibraryController::selectNode(const QString &key)
{
    if (!m_copiesOf.isEmpty()) {
        m_copiesOf.clear();
        emit copiesChanged();
    }
    const QString k = key.isEmpty() ? QStringLiteral("all") : key;
    if (k == QLatin1String("all")) {
        m_nodeLabel = tr("All photos");
    } else if (k == QLatin1String("errors")) {
        m_nodeLabel = tr("Photos with errors");
    } else if (k == QLatin1String("missing")) {
        m_nodeLabel = tr("Missing files");
    } else {
        const QVariantMap n = m_tree.node(k);
        if (!n.isEmpty())
            m_nodeLabel = n.value(QStringLiteral("label")).toString();
        m_tree.reveal(k);
    }
    m_node = k;
    m_grid.clearSelection();
    emit nodeChanged();
    refresh();
}

void LibraryController::findCopies(const QString &photoId)
{
    if (photoId.isEmpty())
        return;
    m_copiesOf = photoId;
    m_copiesName = m_catalog->db().scalar(QStringLiteral("SELECT name FROM photo WHERE id = ?"), {photoId}).toString();
    m_grid.clearSelection();
    emit copiesChanged();
    m_backgroundRefresh = false;
    refresh();
}

QVariantMap LibraryController::photoPeople(const QString &photoId) const
{
    return photoId.isEmpty() ? QVariantMap() : m_catalog->photoPeople(photoId);
}

void LibraryController::findFacesNow(const QStringList &photoIds)
{
    m_pipeline->findFacesNow(orSelection(photoIds));
}

void LibraryController::clearCopies()
{
    if (m_copiesOf.isEmpty())
        return;
    m_copiesOf.clear();
    emit copiesChanged();
    m_backgroundRefresh = false;
    refresh();
}

void LibraryController::setCopiesTolerance(int tolerance)
{
    const int t = std::clamp(tolerance, 0, 2);
    if (t == m_copiesTolerance)
        return;
    m_copiesTolerance = t;
    QSettings().setValue(QStringLiteral("library/copiesTolerance"), t);
    emit copiesChanged();
    if (!m_copiesOf.isEmpty()) {
        m_backgroundRefresh = false;
        refresh();
    }
}

void LibraryController::setSearch(const QString &search)
{
    if (search == m_search)
        return;
    m_search = search;
    emit searchChanged();
    if (!m_copiesOf.isEmpty()) {
        m_copiesOf.clear();
        emit copiesChanged();
    }
    m_backgroundRefresh = false;
    m_refreshTimer.start(m_aiSearch ? 450 : 200);
}

void LibraryController::setAiSearch(bool on)
{
    if (on == m_aiSearch)
        return;
    m_aiSearch = on;
    emit searchChanged();
    refresh();
}

void LibraryController::setSort(const QString &sort)
{
    if (sort == m_sort)
        return;
    m_sort = sort;
    QSettings().setValue(QStringLiteral("library/sort"), sort);
    emit sortChanged();
    refresh();
}

void LibraryController::setThumbSize(int size)
{
    const int s = std::clamp(size, 80, 480);
    if (s == m_thumbSize)
        return;
    m_thumbSize = s;
    QSettings().setValue(QStringLiteral("library/thumbSize"), s);
    emit thumbSizeChanged();
}

GridQuery LibraryController::query() const
{
    GridQuery q;
    q.node = m_node;
    q.search = m_aiSearch ? QString() : m_search;
    q.rules = Catalog::rulesFromVariant(m_rules);
    q.matchAll = m_matchAll;
    q.sort = m_sort;
    return q;
}

void LibraryController::refresh()
{
    PIX_PERF("LibraryController::refresh");
    m_refreshTimer.stop();
    const bool background = std::exchange(m_backgroundRefresh, false);
    const int serial = ++m_searchSerial;
    if (!m_copiesOf.isEmpty()) {
        // Poucas fotos (as cópias de uma): a árvore de hashes e o CLIP já
        // estão na memória, a consulta é de milissegundos.
        m_grid.setRows(m_catalog->photosByIds(m_catalog->findCopies(m_copiesOf, m_copiesTolerance)), background);
        return;
    }
    if (!m_aiSearch || m_search.trimmed().isEmpty() || !aiAvailable()) {
        const QString path = m_catalog->db().path();
        if (path.isEmpty()) {
            // Banco em memória (testes): não há segunda conexão; lê aqui.
            m_grid.setRows(m_catalog->photos(query()), background);
            return;
        }
        // O SQL sai daqui (lista fechada); a leitura roda na thread do leitor
        // com a conexão dela — a interface não espera a consulta.
        QVariantList params;
        const QString sql = m_catalog->photosSql(query(), params);
        auto *watcher = new QFutureWatcher<QList<GridRow>>(this);
        connect(watcher, &QFutureWatcher<QList<GridRow>>::finished, this, [this, watcher, serial, background]() {
            watcher->deleteLater();
            QList<GridRow> rows;
            try {
                rows = watcher->result();
            } catch (const std::exception &e) {
                qCWarning(lcLibrary) << "Grid query failed:" << e.what();
                return;
            }
            if (serial != m_searchSerial)
                return;  // outro pedido veio depois deste
            m_grid.setRows(rows, background);
            if (m_searching) {
                m_searching = false;
                emit searchingChanged();
            }
        });
        watcher->setFuture(QtConcurrent::run(&m_gridPool, [path, sql, params]() { return readGridRows(path, sql, params); }));
        return;
    }
    // Busca por IA: o texto vira embedding fora da thread principal; as fotos
    // mais parecidas que também passam pelo nó e pelo filtro, na ordem da
    // similaridade.
    m_searching = true;
    emit searchingChanged();
    m_pipeline->encodeText(m_search, [this, serial](imaging::Embedding text) {
        if (serial != m_searchSerial)
            return;  // outra busca começou
        const auto ranked = m_catalog->rankByEmbedding(text, kAiMinScore, kAiLimit);
        QSet<QString> allowed;
        for (const GridRow &r : m_catalog->photos(query()))
            allowed.insert(r.id);
        QList<QPair<QString, float>> kept;
        for (const auto &p : ranked)
            if (allowed.contains(p.first))
                kept.append(p);
        m_grid.setRows(m_catalog->photosByIds(kept));
        m_searching = false;
        emit searchingChanged();
    });
}

// --------------------------------------------------------------------------- //
// Filtro personalizado
// --------------------------------------------------------------------------- //

QVariantList LibraryController::filterFields() const
{
    return Catalog::filterFields();
}

QVariantList LibraryController::savedFilters() const
{
    return m_catalog->savedFilters();
}

void LibraryController::setMatchAll(bool all)
{
    if (all == m_matchAll)
        return;
    m_matchAll = all;
    emit rulesChanged();
    refresh();
}

void LibraryController::addRule(const QString &field)
{
    QString op;
    for (const QVariant &f : Catalog::filterFields())
        if (f.toMap().value(QStringLiteral("key")).toString() == field)
            op = f.toMap().value(QStringLiteral("ops")).toStringList().value(0);
    m_rules << QVariantMap{{QStringLiteral("field"), field}, {QStringLiteral("op"), op},
                           {QStringLiteral("value"), QString()}, {QStringLiteral("value2"), QString()}};
    emit rulesChanged();
    refresh();
}

void LibraryController::updateRule(int index, const QVariantMap &rule)
{
    if (index < 0 || index >= m_rules.size())
        return;
    QVariantMap merged = m_rules[index].toMap();
    for (auto it = rule.begin(); it != rule.end(); ++it)
        merged.insert(it.key(), it.value());
    m_rules[index] = merged;
    // Mudou o campo ou o operador: o QML refaz a linha. Só o valor (digitando):
    // nada de sinal — refazer a linha tiraria o foco do campo a cada tecla.
    if (rule.contains(QStringLiteral("field")) || rule.contains(QStringLiteral("op")))
        emit rulesChanged();
    m_backgroundRefresh = false;
    m_refreshTimer.start(250);  // digitando um valor: espera parar
}

void LibraryController::removeRule(int index)
{
    if (index < 0 || index >= m_rules.size())
        return;
    m_rules.removeAt(index);
    emit rulesChanged();
    refresh();
}

void LibraryController::clearRules()
{
    if (m_rules.isEmpty())
        return;
    m_rules.clear();
    emit rulesChanged();
    refresh();
}

void LibraryController::saveFilter(const QString &name)
{
    act([&]() { m_catalog->saveFilter(name, m_matchAll, m_rules); });
    emit savedFiltersChanged();
}

void LibraryController::applySavedFilter(const QString &filterId)
{
    for (const QVariant &v : m_catalog->savedFilters()) {
        const QVariantMap f = v.toMap();
        if (f.value(QStringLiteral("id")).toString() != filterId)
            continue;
        m_rules = f.value(QStringLiteral("rules")).toList();
        m_matchAll = f.value(QStringLiteral("matchAll")).toBool();
        emit rulesChanged();
        refresh();
        return;
    }
}

void LibraryController::deleteSavedFilter(const QString &filterId)
{
    m_catalog->deleteFilter(filterId);
    emit savedFiltersChanged();
}

// --------------------------------------------------------------------------- //
// Organização
// --------------------------------------------------------------------------- //

template<typename F>
void LibraryController::act(F &&fn)
{
    try {
        fn();
    } catch (const ActionError &e) {
        emit failed(e.key(), e.params());
    } catch (const std::exception &e) {
        qCWarning(lcLibrary) << "Action failed:" << e.what();
        emit failed(QStringLiteral("error.generic"), {{QStringLiteral("detail"), QString::fromUtf8(e.what())}});
    }
}

QStringList LibraryController::orSelection(const QStringList &ids) const
{
    return ids.isEmpty() ? m_grid.selectedIds() : ids;
}

void LibraryController::scheduleChoices(const QString &which)
{
    m_staleChoices.insert(which);
    // Indexando, as pessoas mudam a cada foto com rosto: no máximo a cada 5 s.
    if (!m_choicesTimer.isActive())
        m_choicesTimer.start(m_pipeline->busy() ? 5000 : 300);
}

void LibraryController::readChoices()
{
    const QSet<QString> stale = std::exchange(m_staleChoices, {});
    if (stale.contains(QStringLiteral("categories")))
        m_reader->read<QVariantList>(
            this, [](const Catalog &c) { return c.categoryChoices(); },
            [this](QVariantList list) {
                if (list != m_categoryChoices) {
                    m_categoryChoices = list;
                    emit categoryChoicesChanged();
                }
            });
    if (stale.contains(QStringLiteral("albums")))
        m_reader->read<QVariantList>(
            this, [](const Catalog &c) { return c.albumChoices(); },
            [this](QVariantList list) {
                if (list != m_albumChoices) {
                    m_albumChoices = list;
                    emit albumChoicesChanged();
                }
            });
    if (stale.contains(QStringLiteral("people")))
        m_reader->read<QVariantList>(
            this, [](const Catalog &c) { return c.people(); },
            [this](QVariantList people) {
                QVariantList out;
                int unnamed = 0;
                for (const QVariant &v : people) {
                    const QVariantMap p = v.toMap();
                    QString name = p.value(QStringLiteral("name")).toString();
                    if (name.isEmpty())
                        name = tr("Person %1").arg(++unnamed);
                    out << QVariantMap{{QStringLiteral("id"), p.value(QStringLiteral("id"))},
                                       {QStringLiteral("name"), name},
                                       {QStringLiteral("depth"), 0}};
                }
                if (out != m_personChoices) {
                    m_personChoices = out;
                    emit personChoicesChanged();
                }
            });
}

void LibraryController::addToAlbum(const QStringList &photoIds, const QString &albumId)
{
    act([&]() { m_catalog->addPhotosToAlbum(orSelection(photoIds), albumId); });
}

void LibraryController::makePrivate(const QStringList &photoIds)
{
    act([&]() {
        const int n = m_catalog->makePrivate(orSelection(photoIds));
        if (n > 0)
            m_grid.clearSelection();
    });
}

void LibraryController::makePublic(const QStringList &photoIds)
{
    act([&]() { m_catalog->makePublic(orSelection(photoIds)); });
}

bool LibraryController::privateNode() const
{
    return m_node.startsWith(QLatin1String("album:")) && m_catalog->isPrivateAlbum(m_node.mid(6));
}

void LibraryController::removeFromAlbum(const QStringList &photoIds, const QString &albumId)
{
    act([&]() { m_catalog->removePhotosFromAlbum(orSelection(photoIds), albumId); });
}

void LibraryController::addToCategory(const QStringList &photoIds, const QString &categoryId)
{
    act([&]() { m_catalog->addPhotosToCategory(orSelection(photoIds), categoryId); });
}

void LibraryController::moveToCategory(const QStringList &photoIds, const QString &categoryId)
{
    const QString from = m_node.startsWith(QLatin1String("cat:")) ? m_node.mid(4) : QString();
    if (from == categoryId)
        return;
    act([&]() {
        m_catalog->movePhotosToCategory(orSelection(photoIds), categoryId, from);
        m_grid.clearSelection();
    });
}

void LibraryController::moveToAlbum(const QStringList &photoIds, const QString &albumId)
{
    const QString from = m_node.startsWith(QLatin1String("album:")) ? m_node.mid(6) : QString();
    if (from == albumId)
        return;
    act([&]() {
        const QStringList ids = orSelection(photoIds);
        m_catalog->addPhotosToAlbum(ids, albumId);
        // Sair do álbum privado é "tornar pública": só com pedido explícito.
        if (!from.isEmpty() && !m_catalog->isPrivateAlbum(from))
            m_catalog->removePhotosFromAlbum(ids, from);
        m_grid.clearSelection();
    });
}

void LibraryController::removeFromCategory(const QStringList &photoIds, const QString &categoryId)
{
    act([&]() { m_catalog->removePhotosFromCategory(orSelection(photoIds), categoryId); });
}

QString LibraryController::createAlbum(const QString &parentId, const QString &name)
{
    QString id;
    act([&]() { id = m_catalog->createAlbum(parentId, name); });
    return id;
}

void LibraryController::renameAlbum(const QString &albumId, const QString &name)
{
    act([&]() { m_catalog->renameAlbum(albumId, name); });
}

void LibraryController::deleteAlbum(const QString &albumId)
{
    act([&]() { m_catalog->deleteAlbum(albumId); });
    if (m_node == QStringLiteral("album:") + albumId)
        selectNode(QStringLiteral("all"));
}

void LibraryController::moveAlbum(const QString &albumId, const QString &parentId)
{
    act([&]() { m_catalog->moveAlbum(albumId, parentId); });
}

QString LibraryController::createCategory(const QString &parentId, const QString &name)
{
    QString id;
    act([&]() { id = m_catalog->createCategory(parentId, name); });
    return id;
}

void LibraryController::renameCategory(const QString &categoryId, const QString &name)
{
    act([&]() { m_catalog->renameCategory(categoryId, name); });
}

void LibraryController::deleteCategory(const QString &categoryId)
{
    act([&]() { m_catalog->deleteCategory(categoryId); });
    if (m_node == QStringLiteral("cat:") + categoryId)
        selectNode(QStringLiteral("all"));
}

void LibraryController::moveCategory(const QString &categoryId, const QString &parentId)
{
    act([&]() { m_catalog->moveCategory(categoryId, parentId); });
}

QString LibraryController::categoryPrompt(const QString &categoryId) const
{
    return m_catalog->db().scalar(QStringLiteral("SELECT prompt FROM category WHERE id = ?"), {categoryId}).toString();
}

void LibraryController::setCategoryPrompt(const QString &categoryId, const QString &prompt)
{
    act([&]() { m_catalog->setCategoryPrompt(categoryId, prompt); });
    m_pipeline->invalidatePrompts();
}
