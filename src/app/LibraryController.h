// SPDX-License-Identifier: BSD-2-Clause
#pragma once

// A biblioteca (App.library no QML): o painel lateral (abas e árvore), a
// busca, o filtro personalizado, a grade de miniaturas e as ações de
// organização sobre a seleção.
//
// A grade se refaz quando o catálogo muda — agrupando as mudanças: durante a
// indexação chegam centenas por minuto, e a grade se refaz no máximo a cada
// ~5 s (a árvore, a cada ~10 s).

#include "PhotoGridModel.h"
#include "TreeModel.h"

#include <QObject>
#include <QThreadPool>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

namespace pix {
class Catalog;
class Pipeline;
}

class LibraryController : public QObject
{
    Q_OBJECT
    /// "folders" | "dates" | "categories" | "albums" | "places" | "people"
    Q_PROPERTY(QString tab READ tab WRITE setTab NOTIFY tabChanged)
    /// Nó escolhido ("all", "dir:...", "cat:<id>"...): ver CatalogQuery.cpp.
    Q_PROPERTY(QString node READ node NOTIFY nodeChanged)
    Q_PROPERTY(QString nodeLabel READ nodeLabel NOTIFY nodeChanged)
    Q_PROPERTY(QString search READ search WRITE setSearch NOTIFY searchChanged)
    /// Busca por IA (CLIP): o texto vira "a foto mais parecida com isto".
    Q_PROPERTY(bool aiSearch READ aiSearch WRITE setAiSearch NOTIFY searchChanged)
    Q_PROPERTY(bool aiAvailable READ aiAvailable CONSTANT)
    Q_PROPERTY(QString sort READ sort WRITE setSort NOTIFY sortChanged)
    Q_PROPERTY(int thumbSize READ thumbSize WRITE setThumbSize NOTIFY thumbSizeChanged)
    /// Regras do filtro: [{field, op, value, value2}]
    Q_PROPERTY(QVariantList rules READ rules NOTIFY rulesChanged)
    Q_PROPERTY(bool matchAll READ matchAll WRITE setMatchAll NOTIFY rulesChanged)
    Q_PROPERTY(QVariantList filterFields READ filterFields CONSTANT)
    Q_PROPERTY(QVariantList savedFilters READ savedFilters NOTIFY savedFiltersChanged)
    Q_PROPERTY(PhotoGridModel *grid READ grid CONSTANT)
    Q_PROPERTY(TreeModel *tree READ tree CONSTANT)
    Q_PROPERTY(bool searching READ searching NOTIFY searchingChanged)
    /// Escolhas para menus e filtros: [{id, name, depth}]
    Q_PROPERTY(QVariantList categoryChoices READ categoryChoices NOTIFY choicesChanged)
    Q_PROPERTY(QVariantList albumChoices READ albumChoices NOTIFY choicesChanged)
    Q_PROPERTY(QVariantList personChoices READ personChoices NOTIFY choicesChanged)
    /// O nó escolhido é o álbum privado (ou um de dentro dele).
    Q_PROPERTY(bool privateNode READ privateNode NOTIFY nodeChanged)

public:
    LibraryController(pix::Catalog *catalog, pix::Pipeline *pipeline, QObject *parent = nullptr);

    QString tab() const { return m_tab; }
    void setTab(const QString &tab);
    QString node() const { return m_node; }
    QString nodeLabel() const { return m_nodeLabel; }
    QString search() const { return m_search; }
    void setSearch(const QString &search);
    bool aiSearch() const { return m_aiSearch; }
    void setAiSearch(bool on);
    bool aiAvailable() const;
    QString sort() const { return m_sort; }
    void setSort(const QString &sort);
    int thumbSize() const { return m_thumbSize; }
    void setThumbSize(int size);
    QVariantList rules() const { return m_rules; }
    bool matchAll() const { return m_matchAll; }
    void setMatchAll(bool all);
    QVariantList filterFields() const;
    QVariantList savedFilters() const;
    PhotoGridModel *grid() { return &m_grid; }
    TreeModel *tree() { return &m_tree; }
    bool searching() const { return m_searching; }
    QVariantList categoryChoices() const;
    QVariantList albumChoices() const;
    QVariantList personChoices() const;
    bool privateNode() const;

    Q_INVOKABLE void selectNode(const QString &key);
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void refreshTree();

    // Filtro personalizado
    Q_INVOKABLE void addRule(const QString &field);
    Q_INVOKABLE void updateRule(int index, const QVariantMap &rule);
    Q_INVOKABLE void removeRule(int index);
    Q_INVOKABLE void clearRules();
    Q_INVOKABLE void saveFilter(const QString &name);
    Q_INVOKABLE void applySavedFilter(const QString &filterId);
    Q_INVOKABLE void deleteSavedFilter(const QString &filterId);

    // Organização (``photoIds`` vazio = a seleção da grade)
    Q_INVOKABLE void addToAlbum(const QStringList &photoIds, const QString &albumId);
    /// Fotos privadas: some de tudo e o original é cifrado (.photoidxe).
    Q_INVOKABLE void makePrivate(const QStringList &photoIds);
    Q_INVOKABLE void makePublic(const QStringList &photoIds);
    Q_INVOKABLE void removeFromAlbum(const QStringList &photoIds, const QString &albumId);
    Q_INVOKABLE void addToCategory(const QStringList &photoIds, const QString &categoryId);
    /// Corrigir a IA: a foto vai para a categoria e sai da do nó atual (ou,
    /// fora de uma categoria, das que a IA pôs).
    Q_INVOKABLE void moveToCategory(const QStringList &photoIds, const QString &categoryId);
    /// Para o álbum; vindo de outro álbum (o nó atual), sai dele.
    Q_INVOKABLE void moveToAlbum(const QStringList &photoIds, const QString &albumId);
    Q_INVOKABLE void removeFromCategory(const QStringList &photoIds, const QString &categoryId);
    Q_INVOKABLE QString createAlbum(const QString &parentId, const QString &name);
    Q_INVOKABLE void renameAlbum(const QString &albumId, const QString &name);
    Q_INVOKABLE void deleteAlbum(const QString &albumId);
    Q_INVOKABLE void moveAlbum(const QString &albumId, const QString &parentId);
    Q_INVOKABLE QString createCategory(const QString &parentId, const QString &name);
    Q_INVOKABLE void renameCategory(const QString &categoryId, const QString &name);
    Q_INVOKABLE void deleteCategory(const QString &categoryId);
    Q_INVOKABLE void moveCategory(const QString &categoryId, const QString &parentId);
    Q_INVOKABLE QString categoryPrompt(const QString &categoryId) const;
    Q_INVOKABLE void setCategoryPrompt(const QString &categoryId, const QString &prompt);

signals:
    void tabChanged();
    void nodeChanged();
    void searchChanged();
    void sortChanged();
    void thumbSizeChanged();
    void rulesChanged();
    void savedFiltersChanged();
    void searchingChanged();
    void choicesChanged();
    void failed(const QString &key, const QVariantMap &params);

private:
    QStringList orSelection(const QStringList &ids) const;
    template<typename F>
    void act(F &&fn);
    void scheduleRefresh();
    void scheduleTree(const QString &tab);
    pix::GridQuery query() const;

    pix::Catalog *m_catalog;
    pix::Pipeline *m_pipeline;
    PhotoGridModel m_grid;
    TreeModel m_tree;
    QString m_tab = QStringLiteral("folders");
    QString m_node = QStringLiteral("all");
    QString m_nodeLabel;
    QString m_search;
    bool m_aiSearch = false;
    QString m_sort = QStringLiteral("date-desc");
    int m_thumbSize = 180;
    QVariantList m_rules;
    bool m_matchAll = true;
    bool m_searching = false;
    int m_searchSerial = 0;
    QTimer m_refreshTimer;
    bool m_backgroundRefresh = false;   ///< o refresh pendente é de fundo (mantém a rolagem)
    QThreadPool m_reader;               ///< lê a grade fora da thread principal (GridReader)
    QTimer m_treeTimer;
    QSet<QString> m_staleTabs;
};
