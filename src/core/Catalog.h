// SPDX-License-Identifier: BSD-2-Clause
#pragma once

// O catálogo visto pelo app: tudo o que lê ou grava no banco passa por aqui,
// SEMPRE na thread principal (regra 9). Os workers das etapas recebem pedidos
// prontos (IndexJob, CategorizeJob, FaceJob) e devolvem resultados que o
// Catalog grava (applyIndex, applyCategorize, applyFaces).
//
// Implementação dividida por assunto:
//   Catalog.cpp          abrir, preferências, discos, pastas, Searcher, Indexer
//   CatalogAi.cpp        Categorizer: categorias por IA e por regra, busca por IA
//   CatalogPeople.cpp    faces, agrupamento em pessoas, merge, busca por rosto
//   CatalogQuery.cpp     árvores do painel lateral, grade de fotos, detalhe
//   CatalogOrganize.cpp  categorias e álbuns da pessoa
//   CatalogPrivate.cpp   fotos privadas: chaveiro, álbum "Private", etapa "privacy"

#include "core/Crypto.h"
#include "core/Database.h"
#include "core/Disks.h"
#include "core/FaceIndex.h"
#include "core/HashTree.h"
#include "core/Models.h"
#include "core/Vault.h"

#include <QHash>
#include <QObject>
#include <QSqlQuery>
#include <QSet>
#include <QVariantList>
#include <QVariantMap>

#include <memory>
#include <optional>

namespace pix {

/// Erro de uma ação, com chave de i18n (o QML mostra o texto, Messages.qml).
class ActionError : public std::runtime_error
{
public:
    ActionError(const QString &key, const QVariantMap &params = {})
        : std::runtime_error(key.toStdString())
        , m_key(key)
        , m_params(params)
    {
    }
    QString key() const { return m_key; }
    QVariantMap params() const { return m_params; }

private:
    QString m_key;
    QVariantMap m_params;
};

/// Uma linha da grade de miniaturas.
struct GridRow
{
    QString id;
    QString name;
    QString thumbPath;
    int width = 0;
    int height = 0;
    bool isRaw = false;
    bool isPrivate = false;     ///< miniatura cifrada (.pidx): image://private
    QString takenAt;
    QString state;
    double score = 0.0;         ///< busca por IA / por rosto: a similaridade
};

/// Uma regra do filtro personalizado: ``field`` e ``op`` são chaves da lista
/// fechada de Catalog::filterFields() (nunca SQL vindo da interface).
struct FilterRule
{
    QString field;              ///< "width", "camera", "taken", "country", "exif"...
    QString op;                 ///< "eq", "contains", "gt", "between", "empty"...
    QVariant value;
    QVariant value2;            ///< segundo valor ("between"; na tag EXIF, o valor)
};

/// O que a grade mostra: um nó da árvore + texto de busca + filtro + ordem.
struct GridQuery
{
    QString node;               ///< "all", "dir:<path>", "year:2024", "cat:<id>"... (ver CatalogQuery.cpp)
    QString search;             ///< palavras (todas precisam casar)
    QList<FilterRule> rules;    ///< filtro personalizado
    bool matchAll = true;       ///< regras com E (todas) ou OU (qualquer)
    QString sort = QStringLiteral("date-desc");  ///< date-desc | date-asc | name | added | size | ...
};

class Catalog : public QObject
{
    Q_OBJECT

public:
    explicit Catalog(QObject *parent = nullptr);
    ~Catalog() override;

    /// Abre (cria/migra) o banco, semeia as categorias e monta os índices em memória.
    void open(const QString &path);
    /// Um catálogo SÓ DE LEITURA sobre o mesmo arquivo de ``main`` (o mesmo
    /// chaveiro: as privadas continuam escondidas com o cofre trancado). É o
    /// do CatalogReader: árvores, listas e contagens lidas em outra thread.
    /// Sem os índices em memória (árvore de hashes, faces): só consultas SQL.
    void openReaderOf(const Catalog &main);
    bool isReader() const { return m_reader; }
    Database &db() { return m_db; }
    const Database &db() const { return m_db; }

    QVariant setting(const QString &key, const QVariant &fallback = {}) const;
    void setSetting(const QString &key, const QVariant &value);

    // ------------------------------------------------------------------ //
    // Discos e pastas (Catalog.cpp)
    // ------------------------------------------------------------------ //

    /// Cadastra uma pasta para indexar (e o disco dela). Lança ActionError
    /// (folder.notFound, folder.alreadyAdded, folder.insideOther).
    QString addFolder(const QString &path);
    /// Tira a pasta do catálogo: fotos, miniaturas e recortes de rosto
    /// (as fotos no disco não são tocadas).
    void removeFolder(const QString &folderId);
    QString folderPath(const QString &folderId) const;
    QStringList folderIds() const;
    /// [{id, path, diskId, photoCount, indexed, errors, scannedAt, online}]
    QVariantList folders() const;
    /// [{id, key, label, kind, protocol, server, share, fsType, device, serial,
    ///   model, mountPoint, totalBytes, online, folderCount, photoCount}]
    QVariantList disks() const;
    /// Reconhece os discos montados agora: online/offline e, se um voltou em
    /// outro ponto de montagem, refaz os caminhos. Devolve quantos mudaram;
    /// em ``cameOnline``/``wentOffline``, os ids dos que voltaram/saíram.
    int refreshDisks(QStringList *cameOnline = nullptr, QStringList *wentOffline = nullptr);

    /// O que o SO diz dos discos (volumes montados e quais pastas indexadas
    /// existem). Sem banco: roda em qualquer thread — e deve rodar fora da
    /// principal, porque um compartilhamento de rede caído pode demorar
    /// segundos para responder.
    struct DiskProbe
    {
        QList<DiskInfo> mounted;
        QSet<QString> reachableFolders;
    };
    static DiskProbe probeDisks(const QStringList &folderPaths);
    QStringList allFolderPaths() const;
    /// A parte do refreshDisks que é banco (rápida): aplica uma sondagem.
    int applyDiskProbe(const DiskProbe &probe, QStringList *cameOnline = nullptr, QStringList *wentOffline = nullptr);
    /// As pastas indexadas de um disco.
    QStringList folderIdsOnDisk(const QString &diskId) const;

    // ------------------------------------------------------------------ //
    // Searcher -> catálogo
    // ------------------------------------------------------------------ //

    /// Grava o que o Searcher achou: arquivo novo ou alterado (tamanho/data)
    /// vira foto pendente. Devolve quantos entraram na fila do Indexer.
    int ingest(const QString &folderId, const QList<FoundFile> &files);
    /// Fim da varredura: o que não foi visto vira ``missing``. Devolve quantos.
    int finishScan(const QString &folderId, const QSet<QString> &seenPaths);

    // ------------------------------------------------------------------ //
    // Filas das etapas
    // ------------------------------------------------------------------ //

    QList<IndexJob> nextIndexJobs(int limit, const QSet<QString> &busy) const;
    QList<CategorizeJob> nextCategorizeJobs(int limit, const QSet<QString> &busy) const;
    QList<FaceJob> nextFaceJobs(int limit, const QSet<QString> &busy) const;
    /// Pendentes de uma etapa ("index" | "categorize" | "faces").
    int pendingCount(const QString &stage) const;
    /// O universo de cada etapa ("index", "categorize", "faces", "privacy"),
    /// para a fração feita: uma passada na tabela.
    QHash<QString, int> stageTotals() const;
    /// Volta à fila: ``scope`` "errors" (só as que falharam) ou "all".
    int requeue(const QString &stage, const QString &scope);

    void applyIndex(const IndexResult &result);
    /// Vários resultados numa transação (um commit só).
    void applyIndexBatch(const QList<IndexResult> &results);

    // ------------------------------------------------------------------ //
    // Categorizer (CatalogAi.cpp)
    // ------------------------------------------------------------------ //

    /// Categorias que a IA pode atribuir: {id, pai, frases do CLIP}.
    struct Classifiable
    {
        QString id;
        QString parentId;
        QStringList prompts;
    };
    QList<Classifiable> classifiableCategories() const;
    void applyCategorize(const CategorizeResult &result);
    /// Fotos ordenadas pela similaridade com um embedding de texto do CLIP.
    QList<QPair<QString, float>> rankByEmbedding(const imaging::Embedding &text, float minScore, int limit);
    void invalidateEmbeddings() { m_embeddingsLoaded = false; }

    // ------------------------------------------------------------------ //
    // Faces e pessoas (CatalogPeople.cpp)
    // ------------------------------------------------------------------ //

    void applyFaces(const FaceResult &result);
    /// Junta pessoas parecidas (limiares nas preferências). Devolve quantas junções.
    int autoMergePeople();
    /// Refaz o agrupamento automático das pessoas sem nome e das faces soltas
    /// (mantém o que a pessoa confirmou e as pessoas com nome).
    void reclusterPeople();
    const FaceIndex &faceIndex() const { return m_faces; }

    /// [{id, name, coverPath, faceCount, photoCount}] — nomeadas primeiro; as
    /// sem nome só com pelo menos minPersonPhotos() fotos (as outras aparecem
    /// na busca por rosto).
    QVariantList people() const;
    /// A partir de quantas fotos uma pessoa sem nome aparece (people/minPhotos).
    int minPersonPhotos() const;
    QVariantMap person(const QString &personId) const;
    /// [{id, photoId, cropPath, score, similarity, assignment, x, y, w, h}]
    QVariantList personFaces(const QString &personId) const;
    /// Faces que a pessoa tirou de alguém e ainda não pôs em ninguém.
    QVariantList unassignedFaces() const;
    QString createPerson(const QString &name, const QStringList &faceIds);
    void renamePerson(const QString &personId, const QString &name);
    void deletePerson(const QString &personId);
    /// Junta ``sourceId`` em ``targetId`` (``sourceId`` deixa de existir).
    void mergePeople(const QString &targetId, const QString &sourceId);
    /// "Esta face é desta pessoa" (corrige falso negativo).
    void assignFaces(const QStringList &faceIds, const QString &personId);
    /// "Esta face NÃO é desta pessoa" (corrige falso positivo): fica sem pessoa
    /// e o agrupamento nunca a devolve para lá.
    void rejectFaces(const QStringList &faceIds);
    /// Busca por rosto: [{photoId, faceId, personId, similarity}] e, por pessoa,
    /// a melhor similaridade.
    QList<FaceIndex::Match> searchFace(const imaging::Embedding &embedding, float threshold, int limit = 0) const;

    // ------------------------------------------------------------------ //
    // Painel lateral, grade e detalhe (CatalogQuery.cpp)
    // ------------------------------------------------------------------ //

    /// Nós de uma aba ("folders" | "dates" | "categories" | "albums" |
    /// "places" | "people"), em pré-ordem: [{key, parent, label, count, depth,
    /// hasChildren, icon, detail, kind}].
    QVariantList tree(const QString &tab) const;
    QList<GridRow> photos(const GridQuery &query) const;
    /// O SQL da grade (montado aqui, da lista fechada; os valores vão em
    /// ``params``): o app o roda numa conexão de leitura fora da thread
    /// principal (GridReader).
    QString photosSql(const GridQuery &query, QVariantList &params) const;
    /// Uma linha da grade a partir de uma consulta com as colunas de photosSql.
    static GridRow gridRow(const QSqlQuery &query);
    QList<GridRow> photosByIds(const QList<QPair<QString, float>> &ranked) const;
    /// Tudo da foto: arquivo, disco, datas, câmera, lugar, categorias,
    /// álbuns, faces e as tags EXIF.
    QVariantMap photoDetail(const QString &photoId) const;
    QString photoPath(const QString &photoId) const;
    /// A foto deste arquivo (caminho absoluto), se o catálogo a conhece.
    QString photoIdByPath(const QString &path) const;
    /// Fotos com os mesmos pixels (picture_hash) e parecidas (árvore de hashes).
    QVariantList similarPhotos(const QString &photoId, int maxDistance) const;
    /// {photos, indexed, raw, errors, missing, categorized, faces, people, folders, disks}
    QVariantMap stats() const;
    /// Fotos com erro em alguma etapa: [{id, name, path, stage, error}].
    QVariantList errors(int limit) const;

    // ------------------------------------------------------------------ //
    // Filtros personalizados (CatalogFilter.cpp)
    // ------------------------------------------------------------------ //

    /// Os campos filtráveis: [{key, type, ops, group, choices?}]. ``type``:
    /// text | number | date | bool | enum | category | album | person | exif.
    static QVariantList filterFields();
    static QList<FilterRule> rulesFromVariant(const QVariantList &rules);
    /// [{id, name, match, rules}]
    QVariantList savedFilters() const;
    QString saveFilter(const QString &name, bool matchAll, const QVariantList &rules);
    void deleteFilter(const QString &filterId);

    // ------------------------------------------------------------------ //
    // Organização (CatalogOrganize.cpp)
    // ------------------------------------------------------------------ //

    QString createCategory(const QString &parentId, const QString &name);
    void renameCategory(const QString &categoryId, const QString &name);
    void deleteCategory(const QString &categoryId);
    void moveCategory(const QString &categoryId, const QString &newParentId);
    /// Frases do CLIP de uma categoria (inglês, '|'); a foto volta à fila da IA.
    void setCategoryPrompt(const QString &categoryId, const QString &prompt);
    void addPhotosToCategory(const QStringList &photoIds, const QString &categoryId);
    void removePhotosFromCategory(const QStringList &photoIds, const QString &categoryId);
    /// Corrigir a IA: a foto vai para ``categoryId`` (posta pela pessoa) e sai
    /// de ``fromCategoryId`` e das de dentro dela; sem ``fromCategoryId``, sai
    /// de todas as que a IA pôs. As que a pessoa pôs em outras ficam. O que
    /// sai como da IA vira "rejeitada": a IA não põe de volta.
    void movePhotosToCategory(const QStringList &photoIds, const QString &categoryId, const QString &fromCategoryId = {});
    /// [{id, parentId, name, depth}] para escolher categoria (menus).
    QVariantList categoryChoices() const;
    QString categoryName(const QString &categoryId) const;

    QString createAlbum(const QString &parentId, const QString &name);
    void renameAlbum(const QString &albumId, const QString &name);
    void deleteAlbum(const QString &albumId);
    void moveAlbum(const QString &albumId, const QString &newParentId);
    void addPhotosToAlbum(const QStringList &photoIds, const QString &albumId);
    void removePhotosFromAlbum(const QStringList &photoIds, const QString &albumId);
    QVariantList albumChoices() const;

    // ------------------------------------------------------------------ //
    // Fotos privadas (CatalogPrivate.cpp)
    // ------------------------------------------------------------------ //

    /// O chaveiro da sessão (vazio = trancado). O provider de imagens e os
    /// workers usam (thread-safe).
    std::shared_ptr<const crypto::KeyRing> keyRing() const { return m_keys; }
    KeySnapshot keySnapshot() const;
    bool privateUnlocked() const;
    bool hasVaultPassword() const;
    vault::Record vaultRecord() const;
    /// Grava o chaveiro cifrado com a senha (e apaga o em claro).
    void storeVault(const vault::Record &record);
    /// Abre a sessão com o chaveiro já decifrado pelo controller (o PBKDF2
    /// roda fora da thread principal); ``passwordKey`` recifra o chaveiro
    /// numa troca de chave.
    void openVault(const vault::Ring &ring, const QByteArray &passwordKey);
    void lockVault();
    /// O chaveiro legível agora: o em claro (antes da senha) ou o aberto.
    std::optional<vault::Ring> currentRing() const;
    /// Chave nova (a atual vai junto até as fotos serem recifradas).
    /// Devolve a chave nova. Lança vault.locked.
    QByteArray rotateKey();
    QString privateAlbumId() const;
    bool isPrivateAlbum(const QString &albumId) const;
    /// Esconde já e põe na fila da etapa "privacy" (cifrar o original e a
    /// miniatura). Lança vault.noPassword / vault.locked.
    int makePrivate(const QStringList &photoIds);
    /// Põe na fila para decifrar (a foto volta a aparecer quando terminar).
    int makePublic(const QStringList &photoIds);
    QList<PrivacyJob> nextPrivacyJobs(int limit, const QSet<QString> &busy) const;
    void applyPrivacy(const PrivacyResult &result);
    int pendingPrivacy() const;
    /// {hasPassword, unlocked, photos, encrypted, pending, errors, keyId}
    QVariantMap privacyStatus() const;

signals:
    /// Fotos entraram, saíram ou mudaram (a grade e as árvores se refazem).
    void photosChanged();
    /// Mudou a estrutura de uma aba ("categories", "albums", "people", "folders"...).
    void treeChanged(const QString &tab);
    void peopleChanged();
    /// Um disco entrou ou saiu (online/offline, outro ponto de montagem).
    void disksChanged();
    /// O cofre abriu/fechou ou a fila de privadas mudou.
    void privacyChanged();

private:
    // Catalog.cpp
    void seedCategories();
    QString ensureDisk(const DiskInfo &disk);
    QString ensurePlace(const PlaceMatch &place);
    void loadIndexes();
    void markDuplicates(const QString &photoId, const QString &pictureHash, quint64 phash);
    void removePhotoFiles(const QString &photoId);

    // CatalogPrivate.cpp
    void ensureVault();
    void ensurePrivateAlbum();
    /// O álbum "Private" não se renomeia/apaga/move; os de dentro, só abertos.
    void guardAlbum(const QString &albumId) const;
    void pruneKeys();

    // CatalogAi.cpp
    void applyRuleCategories(const QString &photoId);
    QString builtinId(const char *key) const;

    // CatalogPeople.cpp
    struct PersonState
    {
        imaging::Embedding sum;     ///< soma dos embeddings das faces
        int faces = 0;
        QString name;
    };
    void loadPeople();
    QString clusterFace(const QString &faceId, const imaging::Embedding &embedding, float *similarity);
    void refreshPerson(const QString &personId);
    imaging::Embedding centroidOf(const QString &personId) const;

    // CatalogQuery.cpp
    QString nodeFilter(const QString &node, QVariantList &params) const;
    QString searchFilter(const QString &search, QVariantList &params) const;
    // CatalogFilter.cpp
    QString rulesFilter(const QList<FilterRule> &rules, bool matchAll, QVariantList &params) const;

    Database m_db;
    HashTree m_hashes;
    FaceIndex m_faces;
    QHash<QString, PersonState> m_people;
    QHash<QString, QString> m_builtinIds;      ///< builtin_key -> id

    std::shared_ptr<crypto::KeyRing> m_keys = std::make_shared<crypto::KeyRing>();
    QByteArray m_passwordKey;                  ///< só com o cofre aberto

    bool m_reader = false;
    bool m_embeddingsLoaded = false;
    QStringList m_embeddingIds;
    std::vector<float> m_embeddings;           ///< CLIP de todas as fotos (busca por IA)
};

} // namespace pix
