// SPDX-License-Identifier: BSD-2-Clause
#include "core/Pipeline.h"

#include "core/Catalog.h"
#include "core/Paths.h"
#include "core/Places.h"
#include "imaging/Formats.h"
#include "imaging/Hashes.h"
#include "imaging/ImageIO.h"
#include "imaging/Inspect.h"

#include <QDir>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QRunnable>
#include <QTimeZone>
#include <QUuid>

#include <opencv2/core.hpp>

#include <algorithm>
#include <cmath>

Q_LOGGING_CATEGORY(lcPipeline, "photoidx.pipeline")

namespace pix {

namespace {

/// Lado maior da miniatura gravada em ~/.photo_indexer/thumbnail.
constexpr int kThumbSide = 480;
constexpr int kThumbQuality = 82;
/// Arquivos por lote do Searcher para a thread principal.
constexpr int kScanBatch = 400;
/// A cada quantas fotos com rosto o merge automático de pessoas roda.
constexpr int kMergeEvery = 300;

/// Imagem menor que isso é ícone, recurso de programa ou miniatura de cache,
/// não foto (padrão da preferência scan/minBytes).
constexpr qint64 kMinImageBytes = 16 * 1024;
/// A extensão ".raw" também é de imagens de disco (QEMU, recuperação do
/// macOS) com gigabytes: acima disso, não é foto.
constexpr qint64 kMaxGenericRawBytes = 200LL * 1024 * 1024;

/// Pastas que não são fotos de ninguém: lixeiras, caches de miniatura de NAS
/// e do SO, e as de programas e de desenvolvimento (pacotes de app, recursos
/// de projeto, dependências) — que juntas têm dezenas de milhares de ícones.
bool skipDirectory(const QString &name)
{
    if (name.startsWith(u'.'))
        return true;
    static const QStringList skip{
        QStringLiteral("@eaDir"), QStringLiteral("$RECYCLE.BIN"), QStringLiteral("System Volume Information"),
        QStringLiteral("#recycle"), QStringLiteral("#snapshot"), QStringLiteral("lost+found"),
        QStringLiteral("__MACOSX"), QStringLiteral("node_modules"), QStringLiteral("bower_components"),
        QStringLiteral("site-packages"), QStringLiteral("dist-packages"), QStringLiteral("__pycache__"),
        QStringLiteral("Pods"), QStringLiteral("DerivedData"), QStringLiteral("vendor"),
    };
    if (skip.contains(name, Qt::CaseInsensitive))
        return true;
    // Pacotes do macOS e recursos de projeto: Foo.app, Bar.framework, *.xcassets...
    static const QStringList bundles{QStringLiteral(".app"), QStringLiteral(".framework"), QStringLiteral(".bundle"),
                                     QStringLiteral(".xcassets"), QStringLiteral(".appiconset"), QStringLiteral(".imageset"),
                                     QStringLiteral(".lproj"), QStringLiteral(".plugin"), QStringLiteral(".kext")};
    for (const QString &suffix : bundles)
        if (name.endsWith(suffix, Qt::CaseInsensitive))
            return true;
    return false;
}

/// Mesmo caminho? (sem diferenciar maiúsculas onde o SO não diferencia).
bool isSamePath(const QString &a, const QString &b)
{
    if (a.isEmpty() || b.isEmpty())
        return false;
#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
    return a.compare(b, Qt::CaseInsensitive) == 0;
#else
    return a == b;
#endif
}

template<typename F>
void runInPool(QThreadPool &pool, F &&fn)
{
    pool.start(QRunnable::create(std::forward<F>(fn)));
}

/// Data de criação quando a foto não tem EXIF: a do arquivo (ou a da última
/// gravação, se o SO não conta a criação).
QDateTime fileDate(const IndexJob &job)
{
    QDateTime d = job.fileCreated.isValid() ? job.fileCreated : job.fileModified;
    if (job.fileCreated.isValid() && job.fileModified.isValid() && job.fileModified < job.fileCreated)
        d = job.fileModified;  // cópia: o arquivo "nasceu" depois de ser modificado
    if (!d.isValid())
        return {};
    return QDateTime(d.date(), d.time(), QTimeZone::UTC);
}

IndexResult runIndex(const IndexJob &job)
{
    IndexResult r;
    r.photoId = job.photoId;
    imaging::Inspection in = imaging::inspect(job.path);
    if (!in.ok) {
        r.error = in.error.isEmpty() ? QStringLiteral("unreadable file") : in.error;
        return r;
    }
    r.ok = true;
    r.error = in.error;
    r.isRaw = in.isRaw;
    r.format = in.format;
    r.width = in.width;
    r.height = in.height;
    r.shot = in.shot;
    r.tags = std::move(in.tags);
    r.pictureHash = in.pictureHash;
    r.perceptualHash = in.image.empty() ? QString() : imaging::hashToHex(in.perceptualHash);
    r.sharpness = in.sharpness;
    if (in.shot.takenLocal.isValid()) {
        r.takenAt = in.shot.takenLocal;
        r.takenSource = QStringLiteral("exif");
    } else {
        r.takenAt = fileDate(job);
        r.takenSource = r.takenAt.isValid() ? QStringLiteral("file") : QString();
    }
    if (!in.image.empty()) {
        const cv::Mat thumb = imaging::fitTo(in.image, kThumbSide);
        const QString path = AppPaths::thumbnailPath(job.photoId, r.takenAt.date());
        QDir().mkpath(QFileInfo(path).absolutePath());
        if (imaging::writeAtomic(path, imaging::encodeJpeg(thumb, kThumbQuality))) {
            r.thumbPath = path;
            r.thumbWidth = thumb.cols;
            r.thumbHeight = thumb.rows;
        }
    }
    if (in.shot.hasGps)
        r.place = Places::lookup(in.shot.latitude, in.shot.longitude);
    return r;
}

CategorizeResult runCategorize(const CategorizeJob &job, imaging::ClipModel &clip,
                               const std::shared_ptr<const QList<CategoryPrompt>> &prompts, float minProbability,
                               int maxCategories)
{
    CategorizeResult r;
    r.photoId = job.photoId;
    imaging::Embedding image = job.embedding;
    if (image.size() != size_t(imaging::ClipModel::kDimensions)) {
        QFile f(job.thumbPath);
        const cv::Mat rgb = f.open(QIODevice::ReadOnly) ? imaging::decodeImage(f.readAll()) : cv::Mat();
        if (rgb.empty()) {
            r.error = QStringLiteral("thumbnail unreadable");
            return r;
        }
        image = clip.encodeImage(rgb);
        if (image.empty()) {
            r.error = clip.error().isEmpty() ? QStringLiteral("CLIP failed") : clip.error();
            return r;
        }
    }
    r.ok = true;
    r.embedding = image;
    if (!prompts || prompts->isEmpty())
        return r;
    // Cosseno de cada categoria = o da melhor frase dela; probabilidade =
    // softmax com a escala do CLIP (100), como no zero-shot do artigo.
    QHash<QString, float> best;
    for (const CategoryPrompt &p : *prompts) {
        const float s = imaging::dot(image, p.text);
        auto it = best.find(p.categoryId);
        if (it == best.end() || s > *it)
            best.insert(p.categoryId, s);
    }
    float maxLogit = -1e9f;
    for (float s : best)
        maxLogit = std::max(maxLogit, 100.0f * s);
    double total = 0.0;
    QList<QPair<QString, float>> probs;
    for (auto it = best.constBegin(); it != best.constEnd(); ++it) {
        const double e = std::exp(100.0 * it.value() - maxLogit);
        total += e;
        probs.append({it.key(), float(e)});
    }
    for (auto &p : probs)
        p.second = float(p.second / total);
    std::sort(probs.begin(), probs.end(), [](const auto &a, const auto &b) { return a.second > b.second; });
    for (const auto &p : probs) {
        if (p.second < minProbability || r.categories.size() >= maxCategories)
            break;
        r.categories.append(p);
    }
    // Com ~140 categorias a probabilidade se espalha: sem nenhuma acima do
    // limiar, fica o melhor palpite — se ele tiver ao menos 1/3 do limiar.
    if (r.categories.isEmpty() && !probs.isEmpty() && probs.first().second >= minProbability / 3)
        r.categories.append(probs.first());
    return r;
}

FaceResult runFaces(const FaceJob &job, imaging::FaceEngine &engine)
{
    FaceResult r;
    r.photoId = job.photoId;
    const cv::Mat rgb = imaging::loadDisplayImage(job.path, imaging::kFaceImageSide);
    if (rgb.empty()) {
        r.error = QStringLiteral("image unreadable");
        return r;
    }
    if (!engine.load()) {
        r.error = engine.error();
        return r;
    }
    r.ok = true;
    for (const imaging::DetectedFace &d : engine.detect(rgb)) {
        FoundFace f;
        f.faceId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        f.x = std::clamp(d.box.x / rgb.cols, 0.0f, 1.0f);
        f.y = std::clamp(d.box.y / rgb.rows, 0.0f, 1.0f);
        f.w = std::clamp(d.box.width / rgb.cols, 0.0f, 1.0f - f.x);
        f.h = std::clamp(d.box.height / rgb.rows, 0.0f, 1.0f - f.y);
        f.score = d.score;
        f.embedding = d.embedding;
        if (!d.crop.isEmpty()) {
            const QString path = AppPaths::facePath(f.faceId, job.date);
            QDir().mkpath(QFileInfo(path).absolutePath());
            if (imaging::writeAtomic(path, d.crop))
                f.cropPath = path;
        }
        r.faces.append(f);
    }
    return r;
}

} // namespace

Pipeline::Pipeline(Catalog *catalog, QObject *parent)
    : QObject(parent)
    , m_catalog(catalog)
    , m_clip(std::make_unique<imaging::ClipModel>(AppPaths::modelDir(QString::fromLatin1(imaging::ClipModel::kName))))
    , m_faces(std::make_unique<imaging::FaceEngine>(AppPaths::modelDir(QString::fromLatin1(imaging::FaceEngine::kName))))
{
    m_clock.start();
    // Padrões contidos: a indexação roda enquanto a pessoa usa o computador.
    // O paralelismo é por foto; o OpenCV e o ONNX não abrem threads próprias
    // dentro de cada uma (setNumThreads(1), intra-op 1).
    cv::setNumThreads(1);
    const int cores = std::max(2, QThread::idealThreadCount());
    const QHash<QString, int> defaults{{QStringLiteral("search"), 2},
                                       {QStringLiteral("index"), std::clamp(cores / 2, 2, 6)},
                                       {QStringLiteral("categorize"), std::clamp(cores / 8, 1, 2)},
                                       {QStringLiteral("faces"), std::clamp(cores / 8, 1, 2)}};
    for (const QString &key : stageKeys()) {
        Stage s;
        s.key = key;
        s.pool = std::make_unique<QThreadPool>();
        // Prioridade baixa: o resto do sistema (e a interface) passa na frente.
        s.pool->setThreadPriority(QThread::LowPriority);
        s.threads = std::clamp(catalog->setting(QStringLiteral("threads/") + key, defaults.value(key)).toInt(), 1, 64);
        s.pool->setMaxThreadCount(s.threads);
        m_stages.emplace(key, std::move(s));
    }
    m_interactive.setMaxThreadCount(1);
    // Os pendentes de cada etapa são contagens no banco: no máximo a cada 2 s.
    m_pendingTimer.start();
    m_timer.setInterval(2000);
    connect(&m_timer, &QTimer::timeout, this, &Pipeline::pump);
    m_statusTimer.setSingleShot(true);
    m_statusTimer.setInterval(250);
    connect(&m_statusTimer, &QTimer::timeout, this, &Pipeline::statusChanged);
    connect(catalog, &Catalog::treeChanged, this, [this](const QString &tab) {
        if (tab == QLatin1String("categories"))
            invalidatePrompts();
    });
    qCInfo(lcPipeline) << "CLIP" << (m_clip->available() ? "available" : "missing") << "- faces"
                       << (m_faces->available() ? "available" : "missing");
}

Pipeline::~Pipeline()
{
    m_scanCancel->store(true);
    for (auto &[key, s] : m_stages) {
        s.running = false;
        s.pool->clear();
    }
    for (auto &[key, s] : m_stages)
        s.pool->waitForDone();
    m_interactive.waitForDone();
}

QStringList Pipeline::stageKeys()
{
    return {QStringLiteral("search"), QStringLiteral("index"), QStringLiteral("categorize"), QStringLiteral("faces")};
}

Pipeline::Stage &Pipeline::stage(const QString &key)
{
    return m_stages.at(key);
}

const Pipeline::Stage &Pipeline::stage(const QString &key) const
{
    return m_stages.at(key);
}

void Pipeline::emitStatus()
{
    if (!m_statusTimer.isActive())
        m_statusTimer.start();
}

QVariantList Pipeline::status() const
{
    QVariantList out;
    const qint64 now = m_clock.elapsed();
    for (const QString &key : stageKeys()) {
        const Stage &s = stage(key);
        QString state = s.running ? (s.inFlight > 0 ? QStringLiteral("running") : QStringLiteral("idle")) : QStringLiteral("paused");
        QString detail;
        int pending = 0;
        if (key == QLatin1String("search")) {
            pending = int(m_scanQueue.size() + m_scanning.size());
            detail = QString::number(m_scanFound);
        } else {
            if (!m_pendingTimer.isValid() || m_pendingTimer.elapsed() > 2000 || !m_pendingCache.contains(key)) {
                for (const QString &k : {QStringLiteral("index"), QStringLiteral("categorize"), QStringLiteral("faces")})
                    m_pendingCache.insert(k, m_catalog->pendingCount(k));
                m_pendingTimer.restart();
            }
            pending = m_pendingCache.value(key);
        }
        if (key == QLatin1String("categorize") && !m_clip->available())
            state = QStringLiteral("unavailable");
        if (key == QLatin1String("faces") && !m_faces->available())
            state = QStringLiteral("unavailable");
        // Itens por minuto nos últimos 60 s.
        int recent = 0;
        for (qint64 t : s.finishedAt)
            if (now - t <= 60000)
                ++recent;
        out << QVariantMap{{QStringLiteral("key"), key},          {QStringLiteral("state"), state},
                           {QStringLiteral("done"), s.done},      {QStringLiteral("failed"), s.failed},
                           {QStringLiteral("pending"), pending},  {QStringLiteral("inFlight"), s.inFlight},
                           {QStringLiteral("threads"), s.threads}, {QStringLiteral("current"), s.current},
                           {QStringLiteral("rate"), recent},      {QStringLiteral("detail"), detail}};
    }
    return out;
}

bool Pipeline::busy() const
{
    for (const auto &[key, s] : m_stages)
        if (s.inFlight > 0)
            return true;
    return !m_scanQueue.isEmpty() || !m_scanning.isEmpty();
}

void Pipeline::startAll()
{
    for (const QString &key : stageKeys())
        stage(key).running = true;
    m_timer.start();
    pump();
    emitStatus();
}

void Pipeline::pauseAll()
{
    for (const QString &key : stageKeys())
        stage(key).running = false;
    emitStatus();
}

void Pipeline::start(const QString &key)
{
    if (!m_stages.count(key))
        return;
    stage(key).running = true;
    if (key == QLatin1String("search"))
        m_scanCancel->store(false);
    if (!m_timer.isActive())
        m_timer.start();
    pump();
    emitStatus();
}

void Pipeline::pause(const QString &key)
{
    if (!m_stages.count(key))
        return;
    stage(key).running = false;
    emitStatus();
}

void Pipeline::cancel(const QString &key)
{
    if (!m_stages.count(key))
        return;
    Stage &s = stage(key);
    s.running = false;
    s.generation->fetch_add(1);
    s.pool->clear();
    if (key == QLatin1String("search")) {
        m_scanCancel->store(true);
        m_scanQueue.clear();
        // Um flag novo para as próximas varreduras; as de agora veem o antigo.
        m_scanCancel = std::make_shared<std::atomic<bool>>(false);
    }
    qCInfo(lcPipeline) << "Stage" << key << "cancelled";
    emitStatus();
}

void Pipeline::setThreads(const QString &key, int threads)
{
    if (!m_stages.count(key))
        return;
    Stage &s = stage(key);
    s.threads = std::clamp(threads, 1, 64);
    s.pool->setMaxThreadCount(s.threads);
    m_catalog->setSetting(QStringLiteral("threads/") + key, s.threads);
    pump();
    emitStatus();
}

int Pipeline::threads(const QString &key) const
{
    return m_stages.count(key) ? stage(key).threads : 0;
}

void Pipeline::finished(Stage &s, const QString &id, bool ok, const QString &name)
{
    --s.inFlight;
    s.busy.remove(id);
    ok ? ++s.done : ++s.failed;
    s.current = name;
    s.finishedAt.push_back(m_clock.elapsed());
    while (s.finishedAt.size() > 2000 || (!s.finishedAt.empty() && m_clock.elapsed() - s.finishedAt.front() > 60000))
        s.finishedAt.pop_front();
}

void Pipeline::pump()
{
    // Searcher: uma pasta por thread do pool.
    Stage &search = stage(QStringLiteral("search"));
    while (search.running && !m_scanQueue.isEmpty() && search.inFlight < search.threads) {
        const QString folderId = m_scanQueue.takeFirst();
        const QString root = m_catalog->folderPath(folderId);
        if (root.isEmpty() || !QFileInfo(root).isDir())
            continue;
        m_scanning.insert(folderId);
        ++search.inFlight;
        search.current = QDir::toNativeSeparators(root);
        QPointer<Pipeline> self(this);
        const auto cancelled = m_scanCancel;
        // A pasta de dados do app (miniaturas e recortes de rosto) nunca é
        // indexada, mesmo que a pessoa indexe a raiz do disco (C:\, /).
        const QString appData = QFileInfo(AppPaths::dataDir()).canonicalFilePath();
        const qint64 minBytes = m_catalog->setting(QStringLiteral("scan/minBytes"), kMinImageBytes).toLongLong();
        runInPool(*search.pool, [self, folderId, root, cancelled, appData, minBytes]() {
            QSet<QString> seen;
            QList<FoundFile> batch;
            int found = 0;
            auto flush = [&](bool last) {
                QList<FoundFile> files = std::move(batch);
                batch.clear();
                const QSet<QString> seenCopy = last ? seen : QSet<QString>();
                const bool wasCancelled = cancelled->load();
                QMetaObject::invokeMethod(
                    self.data(),
                    [self, folderId, files, seenCopy, last, wasCancelled, found]() {
                        if (!self)
                            return;
                        Pipeline &p = *self;
                        int queued = p.m_catalog->ingest(folderId, files);
                        p.m_scanFound += int(files.size());
                        if (last) {
                            Stage &st = p.stage(QStringLiteral("search"));
                            // Varredura cancelada no meio não marca nada como sumido.
                            const int missing = wasCancelled ? 0 : p.m_catalog->finishScan(folderId, seenCopy);
                            p.m_scanning.remove(folderId);
                            p.finished(st, folderId, !wasCancelled, p.m_catalog->folderPath(folderId));
                            qCInfo(lcPipeline) << "Scan of" << p.m_catalog->folderPath(folderId) << "done:" << found
                                               << "images," << missing << "missing" << (wasCancelled ? "(cancelled)" : "");
                            emit p.scanFinished(folderId, found, queued, missing);
                        }
                        p.pump();
                        p.emitStatus();
                    },
                    Qt::QueuedConnection);
            };
            QStringList stack{root};
            while (!stack.isEmpty() && !cancelled->load()) {
                const QDir dir(stack.takeLast());
                const QFileInfoList entries = dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks,
                                                                QDir::Name);
                for (const QFileInfo &fi : entries) {
                    if (fi.isDir()) {
                        if (!skipDirectory(fi.fileName()) && !isSamePath(fi.canonicalFilePath(), appData))
                            stack << fi.absoluteFilePath();
                        continue;
                    }
                    const QString name = fi.fileName();
                    if (name.startsWith(QLatin1String("._")) || !imaging::isImageFile(name))
                        continue;
                    if (fi.size() < minBytes
                        || (imaging::extensionOf(name) == QLatin1String("raw") && fi.size() > kMaxGenericRawBytes))
                        continue;
                    // A extensão diz "imagem"; o conteúdo (os primeiros 4 KB)
                    // confirma: imagem de disco, arquivo corrompido ou
                    // renomeado fica de fora.
                    if (imaging::sniffImageFile(fi.absoluteFilePath()).isEmpty())
                        continue;
                    FoundFile f;
                    f.path = QDir::fromNativeSeparators(fi.absoluteFilePath());
                    f.size = fi.size();
                    f.modified = fi.lastModified();
                    f.mtimeMs = f.modified.toMSecsSinceEpoch();
                    f.created = fi.birthTime().isValid() ? fi.birthTime() : fi.metadataChangeTime();
                    seen.insert(f.path);
                    batch.append(f);
                    ++found;
                    if (batch.size() >= kScanBatch)
                        flush(false);
                }
            }
            flush(true);
        });
    }
    pumpIndex();
    pumpCategorize();
    pumpFaces();
    emitStatus();
}

void Pipeline::scan(const QStringList &folderIds)
{
    const QStringList ids = folderIds.isEmpty() ? m_catalog->folderIds() : folderIds;
    for (const QString &id : ids)
        if (!m_scanQueue.contains(id) && !m_scanning.contains(id))
            m_scanQueue << id;
    m_scanFound = 0;
    Stage &s = stage(QStringLiteral("search"));
    s.running = true;
    m_scanCancel->store(false);
    pump();
}

void Pipeline::pumpIndex()
{
    Stage &s = stage(QStringLiteral("index"));
    if (!s.running)
        return;
    const int room = s.threads * 2 - s.inFlight;
    if (room <= 0)
        return;
    QPointer<Pipeline> self(this);
    const auto gen = s.generation;
    const int myGen = gen->load();
    for (const IndexJob &job : m_catalog->nextIndexJobs(room, s.busy)) {
        s.busy.insert(job.photoId);
        ++s.inFlight;
        runInPool(*s.pool, [self, job, gen, myGen]() {
            if (gen->load() != myGen) {
                QMetaObject::invokeMethod(self.data(), [self, job]() {
                    if (self)
                        self->finished(self->stage(QStringLiteral("index")), job.photoId, false, {});
                }, Qt::QueuedConnection);
                return;
            }
            IndexResult r = runIndex(job);
            QMetaObject::invokeMethod(
                self.data(),
                [self, r = std::move(r), path = job.path]() {
                    if (!self)
                        return;
                    self->m_catalog->applyIndex(r);
                    if (!r.ok)
                        qCWarning(lcPipeline) << "Index failed:" << path << "-" << r.error;
                    self->finished(self->stage(QStringLiteral("index")), r.photoId, r.ok, QFileInfo(path).fileName());
                    self->pump();
                },
                Qt::QueuedConnection);
        });
    }
}

void Pipeline::invalidatePrompts()
{
    m_promptsDirty = true;
}

void Pipeline::preparePrompts()
{
    if (m_preparingPrompts)
        return;
    m_preparingPrompts = true;
    m_promptsDirty = false;
    // Frases por categoria, montadas aqui (banco); o texto vira embedding no
    // pool (o encoder de texto leva ~10 ms por frase).
    const auto categories = m_catalog->classifiableCategories();
    const QHash<QString, imaging::Embedding> cache = m_promptCache;
    QPointer<Pipeline> self(this);
    imaging::ClipModel *clip = m_clip.get();
    runInPool(m_interactive, [self, categories, cache, clip]() {
        auto prompts = std::make_shared<QList<CategoryPrompt>>();
        QHash<QString, imaging::Embedding> computed = cache;
        for (const auto &[categoryId, texts] : categories)
            for (const QString &text : texts) {
                auto it = computed.find(text);
                if (it == computed.end())
                    it = computed.insert(text, clip->encodeText(text));
                if (!it->empty())
                    prompts->append({categoryId, *it});
            }
        QMetaObject::invokeMethod(
            self.data(),
            [self, prompts, computed]() {
                if (!self)
                    return;
                self->m_prompts = prompts;
                self->m_promptCache = computed;
                self->m_preparingPrompts = false;
                qCInfo(lcPipeline) << "Category prompts ready:" << prompts->size();
                self->pump();
            },
            Qt::QueuedConnection);
    });
}

void Pipeline::pumpCategorize()
{
    Stage &s = stage(QStringLiteral("categorize"));
    if (!s.running || !m_clip->available())
        return;
    if (m_promptsDirty || !m_prompts) {
        // Lote em andamento termina com as frases velhas; os próximos esperam as novas.
        if (s.inFlight == 0)
            preparePrompts();
        return;
    }
    const int room = s.threads * 2 - s.inFlight;
    if (room <= 0)
        return;
    QPointer<Pipeline> self(this);
    const auto prompts = m_prompts;
    imaging::ClipModel *clip = m_clip.get();
    const float minProbability = m_catalog->setting(QStringLiteral("ai/minProbability"), 0.15).toFloat();
    const int maxCategories = m_catalog->setting(QStringLiteral("ai/maxCategories"), 3).toInt();
    const auto gen = s.generation;
    const int myGen = gen->load();
    for (const CategorizeJob &job : m_catalog->nextCategorizeJobs(room, s.busy)) {
        s.busy.insert(job.photoId);
        ++s.inFlight;
        runInPool(*s.pool, [self, job, prompts, clip, minProbability, maxCategories, gen, myGen]() {
            CategorizeResult r;
            const bool skip = gen->load() != myGen;
            if (!skip)
                r = runCategorize(job, *clip, prompts, minProbability, maxCategories);
            QMetaObject::invokeMethod(
                self.data(),
                [self, r = std::move(r), skip, id = job.photoId]() {
                    if (!self)
                        return;
                    if (!skip)
                        self->m_catalog->applyCategorize(r);
                    self->finished(self->stage(QStringLiteral("categorize")), id, !skip && r.ok, {});
                    self->pump();
                },
                Qt::QueuedConnection);
        });
    }
}

void Pipeline::pumpFaces()
{
    Stage &s = stage(QStringLiteral("faces"));
    if (!s.running || !m_faces->available())
        return;
    const int room = s.threads * 2 - s.inFlight;
    if (room <= 0)
        return;
    const QList<FaceJob> jobs = m_catalog->nextFaceJobs(room, s.busy);
    if (jobs.isEmpty() && s.inFlight == 0 && m_facesSinceMerge > 0) {
        // Fila vazia: junta as pessoas parecidas do que entrou.
        m_facesSinceMerge = 0;
        m_catalog->autoMergePeople();
    }
    QPointer<Pipeline> self(this);
    imaging::FaceEngine *engine = m_faces.get();
    const auto gen = s.generation;
    const int myGen = gen->load();
    for (const FaceJob &job : jobs) {
        s.busy.insert(job.photoId);
        ++s.inFlight;
        runInPool(*s.pool, [self, job, engine, gen, myGen]() {
            FaceResult r;
            const bool skip = gen->load() != myGen;
            if (!skip)
                r = runFaces(job, *engine);
            QMetaObject::invokeMethod(
                self.data(),
                [self, r = std::move(r), skip, id = job.photoId, path = job.path]() {
                    if (!self)
                        return;
                    if (!skip) {
                        self->m_catalog->applyFaces(r);
                        if (!r.faces.isEmpty() && ++self->m_facesSinceMerge >= kMergeEvery) {
                            self->m_facesSinceMerge = 0;
                            self->m_catalog->autoMergePeople();
                        }
                    }
                    self->finished(self->stage(QStringLiteral("faces")), id, !skip && r.ok, QFileInfo(path).fileName());
                    self->pump();
                },
                Qt::QueuedConnection);
        });
    }
}

void Pipeline::encodeText(const QString &text, std::function<void(imaging::Embedding)> done)
{
    QPointer<Pipeline> self(this);
    imaging::ClipModel *clip = m_clip.get();
    runInPool(m_interactive, [self, text, clip, done]() {
        imaging::Embedding e = clip->encodeText(text);
        QMetaObject::invokeMethod(
            self.data(), [self, e = std::move(e), done]() {
                if (self)
                    done(e);
            },
            Qt::QueuedConnection);
    });
}

void Pipeline::detectFaces(const QString &imagePath, std::function<void(QList<imaging::DetectedFace>, QString)> done)
{
    QPointer<Pipeline> self(this);
    imaging::FaceEngine *engine = m_faces.get();
    runInPool(m_interactive, [self, imagePath, engine, done]() {
        QList<imaging::DetectedFace> faces;
        QString error;
        const cv::Mat rgb = imaging::loadDisplayImage(imagePath, imaging::kFaceImageSide);
        if (rgb.empty())
            error = QStringLiteral("image.unreadable");
        else if (!engine->load())
            error = QStringLiteral("faces.unavailable");
        else
            faces = engine->detect(rgb, 0, true);  // a foto enviada: sem tamanho mínimo
        QMetaObject::invokeMethod(
            self.data(), [self, faces, error, done]() {
                if (self)
                    done(faces, error);
            },
            Qt::QueuedConnection);
    });
}

} // namespace pix
