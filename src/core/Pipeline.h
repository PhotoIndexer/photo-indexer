// SPDX-License-Identifier: BSD-2-Clause
#pragma once

// As etapas da indexação, cada uma com o seu pool de threads:
//
//   search      Searcher     desce pelas pastas indexadas e acha as imagens
//   index       Indexer      abre o arquivo: EXIF completo, miniatura, hashes,
//                            lugar (GPS -> cidade/estado/país)
//   categorize  Categorizer  CLIP: categorias e o embedding da busca por IA
//   faces       Faces        insightface: rostos, biometria e pessoas
//   privacy     Privacy      fotos privadas: cifra/decifra o original e a
//                            miniatura (só com o cofre aberto)
//
// O banco é só da thread principal (regra 9): o Pipeline pede ao Catalog o
// próximo lote (pedidos prontos), despacha para o pool e grava o resultado
// quando ele volta (por invokeMethod em fila). A fila de cada etapa É o banco
// (o estado de cada foto): fechar o app no meio não perde nada.
//
// Cada etapa: running | paused | idle | unavailable (modelo de IA ausente) |
// locked (privacy com o cofre trancado).
// Pausar para de despachar (o que já está rodando termina); cancelar também
// descarta o que estava na fila do pool.

#include "core/Models.h"
#include "imaging/Clip.h"
#include "imaging/Faces.h"

#include <QElapsedTimer>
#include <QHash>
#include <QObject>
#include <QPointer>
#include <QSet>
#include <QThreadPool>
#include <QTimer>
#include <QVariantList>

#include <atomic>
#include <deque>
#include <functional>
#include <map>
#include <memory>

namespace pix {

class Catalog;
class CatalogReader;

class Pipeline : public QObject
{
    Q_OBJECT

public:
    explicit Pipeline(Catalog *catalog, QObject *parent = nullptr);
    ~Pipeline() override;

    /// Etapas: "search", "index", "categorize", "faces", "privacy".
    static QStringList stageKeys();

    /// [{key, state, done, failed, pending, inFlight, threads, current, rate, detail, total}]
    /// ``total`` = universo da etapa no catálogo (-1 no Searcher).
    QVariantList status() const;
    bool busy() const;

    /// Liga as etapas (e o despacho automático). Chamado ao abrir o app.
    void startAll();
    void pauseAll();

    /// Varre as pastas (todas, se vazio): arquivos novos, alterados e sumidos.
    void scan(const QStringList &folderIds = {});
    void start(const QString &stage);
    void pause(const QString &stage);
    /// Para a etapa e descarta o que estava na fila do pool.
    void cancel(const QString &stage);
    void setThreads(const QString &stage, int threads);
    /// Há trabalho novo na fila (requeue, fotos novas): olha já.
    void wake();
    /// "Procurar rostos agora": estas fotos furam a fila da etapa de faces
    /// (mesmo pausada), dentro do teto de tarefas.
    void findFacesNow(const QStringList &photoIds);
    /// Refaz as pessoas automáticas (core/FaceClusterer.h) fora da thread
    /// principal; ``done`` recebe quantas pessoas ficaram.
    void regroupPeople(std::function<void(int people)> done = {});
    bool regrouping() const { return m_regrouping; }
    int threads(const QString &stage) const;
    /// Teto de tarefas ao mesmo tempo, somando todas as etapas (o padrão
    /// é metade dos núcleos, entre 2 e 8).
    int maxTasks() const { return m_maxTasks; }
    void setMaxTasks(int tasks);
    static int defaultMaxTasks();

    /// As frases das categorias mudaram: recalcula os embeddings de texto.
    void invalidatePrompts();

    /// Contagens de pendentes e totais lidas no CatalogReader (sem ele, na
    /// thread principal — testes).
    void setReader(CatalogReader *reader) { m_reader = reader; }

    imaging::ClipModel &clip() { return *m_clip; }
    imaging::FaceEngine &faceEngine() { return *m_faces; }

    /// Trabalho avulso da interface, fora da thread principal (pool próprio de
    /// 1 thread, que não espera a fila das etapas). O callback roda na thread
    /// principal.
    void encodeText(const QString &text, std::function<void(imaging::Embedding)> done);
    void detectFaces(const QString &imagePath, std::function<void(QList<imaging::DetectedFace>, QString error)> done);

signals:
    void statusChanged();
    /// Fim da varredura de uma pasta.
    void scanFinished(const QString &folderId, int found, int queued, int missing);
    /// Um arquivo da fila não existe mais: vale conferir os discos já.
    void filesVanished();

private:
    struct Stage
    {
        QString key;
        std::unique_ptr<QThreadPool> pool;
        bool running = false;
        int threads = 1;
        int inFlight = 0;
        int done = 0;
        int failed = 0;
        QString current;
        QSet<QString> busy;             ///< ids em andamento
        /// Fotos cujo arquivo sumiu no meio do trabalho (disco desconectado
        /// antes de o app perceber): ficam fora da fila até os discos mudarem,
        /// sem virar erro e sem tentar de novo em laço.
        QSet<QString> parked;
        std::deque<qint64> finishedAt;  ///< ms dos últimos concluídos (taxa)
        std::shared_ptr<std::atomic<int>> generation = std::make_shared<std::atomic<int>>(0);
    };

    Stage &stage(const QString &key);
    const Stage &stage(const QString &key) const;
    void pump();
    /// pump() logo depois (junta os pedidos de vários resultados).
    void schedulePump();
    void flushIndexResults();
    /// Reparte as vagas do teto entre as etapas (uma de cada por vez).
    void dispatchWork();
    void dispatchIndex(const IndexJob &job);
    void dispatchCategorize(const CategorizeJob &job);
    void dispatchFaces(const FaceJob &job);
    void dispatchPrivacy(const PrivacyJob &job);
    int totalInFlight() const;
    void finished(Stage &s, const QString &id, bool ok, const QString &name);
    /// O arquivo sumiu: tira da fila até os discos mudarem (ver Stage::parked).
    void park(Stage &s, const QString &id);
    void preparePrompts();
    void emitStatus();

    Catalog *m_catalog;
    std::unique_ptr<imaging::ClipModel> m_clip;
    std::unique_ptr<imaging::FaceEngine> m_faces;
    std::map<QString, Stage> m_stages;
    QStringList m_facesFirst;     ///< findFacesNow: na frente da fila de faces
    QThreadPool m_interactive;
    QTimer m_timer;
    QTimer m_statusTimer;
    QTimer m_pumpTimer;
    QTimer m_flushTimer;
    QList<QPair<IndexResult, QString>> m_indexResults;  ///< à espera do lote
    QElapsedTimer m_clock;

    // Searcher: pastas na fila e em varredura.
    QStringList m_scanQueue;
    QSet<QString> m_scanning;
    std::shared_ptr<std::atomic<bool>> m_scanCancel = std::make_shared<std::atomic<bool>>(false);
    int m_scanFound = 0;

    // Categorizer: embeddings das frases (compartilhados com os workers).
    std::shared_ptr<const QList<CategoryPrompt>> m_prompts;
    QHash<QString, imaging::Embedding> m_promptCache;
    bool m_preparingPrompts = false;
    bool m_promptsDirty = true;

    int m_facesSinceMerge = 0;
    bool m_merging = false;
    bool m_regrouping = false;
    void mergePeopleAsync();
    int m_maxTasks = 4;
    QElapsedTimer m_indexIdle;      ///< a última busca do Indexer veio vazia (desde quando)
    int m_roundRobin = 0;

    // Pendentes por etapa (contagens no banco), guardados por 2 s.
    mutable QHash<QString, int> m_pendingCache;
    mutable QHash<QString, int> m_totalCache;     ///< universo de cada etapa (mesmo instante)
    mutable bool m_countsInFlight = false;
    CatalogReader *m_reader = nullptr;
    void readCounts() const;
    mutable QElapsedTimer m_pendingTimer;
};

} // namespace pix
