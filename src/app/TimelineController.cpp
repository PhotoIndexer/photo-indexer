// SPDX-License-Identifier: BSD-2-Clause
#include "TimelineController.h"

#include "ImageProviders.h"
#include "core/Catalog.h"
#include "core/CatalogReader.h"
#include "core/Perf.h"
#include "imaging/Collage.h"
#include "imaging/ImageIO.h"
#include "imaging/Inspect.h"

#include <QDir>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QLocale>
#include <QLoggingCategory>
#include <QSettings>
#include <QtConcurrent>

#include <memory>

Q_LOGGING_CATEGORY(lcTimeline, "photoidx.timeline")

using namespace pix;

namespace {

/// Recorte da prévia e o da exportação (retrato 5:6, como as montagens).
const cv::Size kPreviewTile(400, 480);
const cv::Size kExportTile(600, 720);
/// Lado maior do original decodificado para cada um.
constexpr int kPreviewSource = 1600;
constexpr int kExportSource = 2400;
/// Fotos mostradas no painel de troca de um período.
constexpr int kMaxCandidates = 300;

/// O recorte de retrato de uma face: do original ou, com o disco
/// desconectado, da miniatura; em último caso, o recorte pequeno do rosto.
QImage portraitOf(const timeline::Face &f, int sourceSide, cv::Size tile)
{
    cv::Mat src;
    if (QFileInfo::exists(f.path))
        src = imaging::loadDisplayImage(f.path, sourceSide);
    if (src.empty() && !f.thumbPath.isEmpty() && QFileInfo::exists(f.thumbPath))
        src = imaging::fromQImage(QImage(f.thumbPath));
    if (!src.empty()) {
        const cv::Mat crop = imaging::cropPortrait(src, QRectF(f.x, f.y, f.w, f.h), tile);
        if (!crop.empty())
            return imaging::toQImage(crop);
    }
    const QImage small(f.cropPath);
    if (small.isNull())
        return {};
    const QImage scaled = small.scaled(tile.width, tile.height, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    return scaled.copy((scaled.width() - tile.width) / 2, (scaled.height() - tile.height) / 2, tile.width, tile.height);
}

} // namespace

TimelineController::TimelineController(Catalog *catalog, CatalogReader *reader, QObject *parent)
    : QObject(parent)
    , m_catalog(catalog)
    , m_reader(reader)
{
    QSettings settings;
    m_frequency = settings.value(QStringLiteral("timeline/frequency"), QStringLiteral("year")).toString();
    if (!timeline::validFrequency(m_frequency))
        m_frequency = QStringLiteral("year");
    m_columns = std::clamp(settings.value(QStringLiteral("timeline/columns"), 0).toInt(), 0, 12);
    m_labels = settings.value(QStringLiteral("timeline/labels"), true).toBool();
    m_pool.setMaxThreadCount(1);
}

QString TimelineController::periodLabel(const QString &period)
{
    const QString year = period.left(4);
    if (period.size() == 4)
        return year;
    const QString rest = period.mid(5);
    if (rest.startsWith(u'Q'))
        return tr("Q%1 %2").arg(rest.mid(1), year);
    if (rest == QLatin1String("S1"))
        return tr("H1 %1").arg(year);
    if (rest == QLatin1String("S2"))
        return tr("H2 %1").arg(year);
    QString month = QLocale().monthName(rest.toInt(), QLocale::ShortFormat);
    if (month.endsWith(u'.'))
        month.chop(1);
    if (!month.isEmpty())
        month[0] = month[0].toUpper();
    return month + u' ' + year;
}

// --------------------------------------------------------------------------- //
// Preferências
// --------------------------------------------------------------------------- //

void TimelineController::setFrequency(const QString &frequency)
{
    if (frequency == m_frequency || !timeline::validFrequency(frequency))
        return;
    m_frequency = frequency;
    QSettings().setValue(QStringLiteral("timeline/frequency"), frequency);
    m_selected.clear();
    selectionUpdated();
    reload();
}

void TimelineController::setColumns(int columns)
{
    columns = std::clamp(columns, 0, 12);
    if (columns == m_columns)
        return;
    m_columns = columns;
    QSettings().setValue(QStringLiteral("timeline/columns"), columns);
    emit layoutChanged();
}

int TimelineController::effectiveColumns() const
{
    return m_columns > 0 ? m_columns : timeline::autoColumns(std::max(1, shownCount()));
}

void TimelineController::setLabels(bool on)
{
    if (on == m_labels)
        return;
    m_labels = on;
    QSettings().setValue(QStringLiteral("timeline/labels"), on);
    emit layoutChanged();
}

int TimelineController::shownCount() const
{
    return int(std::count_if(m_entries.begin(), m_entries.end(), [](const timeline::Entry &e) { return !e.hidden; }));
}

// --------------------------------------------------------------------------- //
// Leitura
// --------------------------------------------------------------------------- //

void TimelineController::open(const QString &personId, const QString &label)
{
    if (personId != m_personId) {
        m_faces.clear();
        m_faceIndex.clear();
        m_groups.clear();
        m_entries.clear();
        m_entryList.clear();
        m_entryModel.setItems({});
        m_selected.clear();
        selectionUpdated();
    }
    m_personId = personId;
    m_personLabel = label;
    reload();
}

void TimelineController::reload()
{
    if (m_personId.isEmpty())
        return;
    m_loading = true;
    emit changed();
    struct Data
    {
        QList<timeline::Face> faces;
        QHash<QString, timeline::Pick> picks;
    };
    const int serial = ++m_serial;
    const QString personId = m_personId, frequency = m_frequency;
    m_reader->read<Data>(
        this,
        [personId, frequency](const Catalog &c) { return Data{c.timelineFaces(personId), c.timelinePicks(personId, frequency)}; },
        [this, serial, personId, frequency](Data d) {
            if (serial != m_serial)
                return;
            PIX_PERF("TimelineController::apply");
            m_faces = std::move(d.faces);
            m_faceIndex.clear();
            for (int i = 0; i < m_faces.size(); ++i)
                m_faceIndex.insert(m_faces[i].faceId, i);
            m_groups = timeline::group(m_faces, frequency);
            QList<timeline::Pick> toSave;
            m_entries = timeline::resolve(m_faces, m_groups, d.picks, &toSave);
            // As escolhas automáticas ficam guardadas: voltar mostra a mesma
            // montagem, mesmo que fotos novas entrem no catálogo.
            try {
                m_catalog->saveTimelinePicks(personId, frequency, toSave);
            } catch (const std::exception &e) {
                qCWarning(lcTimeline) << "Could not save the timeline picks:" << e.what();
            }
            m_loading = false;
            rebuild();
        });
}

const timeline::Face *TimelineController::faceById(const QString &faceId) const
{
    const auto it = m_faceIndex.constFind(faceId);
    return it == m_faceIndex.constEnd() ? nullptr : &m_faces[*it];
}

int TimelineController::entryIndex(const QString &period) const
{
    for (int i = 0; i < m_entries.size(); ++i)
        if (m_entries[i].period == period)
            return i;
    return -1;
}

void TimelineController::rebuild()
{
    m_entryList.clear();
    for (const timeline::Entry &e : m_entries) {
        const timeline::Face *f = faceById(e.faceId);
        QString imageUrl;
        if (f) {
            // A chave inclui a face: trocar a foto do período pede outra imagem.
            const QString key = m_personId + u'-' + e.period + u'-' + f->faceId;
            ImageRegistry::instance().put(QStringLiteral("timeline"), key, [face = *f]() {
                return portraitOf(face, kPreviewSource, kPreviewTile);
            });
            imageUrl = QStringLiteral("image://photo/timeline/") + key;
        }
        m_entryList << QVariantMap{{QStringLiteral("period"), e.period},
                                   {QStringLiteral("label"), periodLabel(e.period)},
                                   {QStringLiteral("faceId"), e.faceId},
                                   {QStringLiteral("photoId"), f ? f->photoId : QString()},
                                   {QStringLiteral("photoName"), f ? f->photoName : QString()},
                                   {QStringLiteral("imageUrl"), imageUrl},
                                   {QStringLiteral("user"), e.user},
                                   {QStringLiteral("hidden"), e.hidden},
                                   {QStringLiteral("candidates"), e.candidates}};
    }
    if (!m_selected.isEmpty() && entryIndex(m_selected) < 0)
        m_selected.clear();
    m_entryModel.setItems(m_entryList);
    emit changed();
    emit layoutChanged();
    selectionUpdated();
}

void TimelineController::selectionUpdated()
{
    m_candidateModel.setItems(candidates());
    emit selectionChanged();
}

// --------------------------------------------------------------------------- //
// Período aberto
// --------------------------------------------------------------------------- //

void TimelineController::setSelected(const QString &period)
{
    if (period == m_selected)
        return;
    m_selected = period;
    selectionUpdated();
}

QVariantMap TimelineController::selectedEntry() const
{
    const int i = entryIndex(m_selected);
    return i < 0 ? QVariantMap() : m_entryList.value(i).toMap();
}

QVariantList TimelineController::candidates() const
{
    QVariantList out;
    const int i = entryIndex(m_selected);
    if (i < 0)
        return out;
    const QString current = m_entries[i].faceId;
    for (const auto &[period, list] : m_groups) {
        if (period != m_selected)
            continue;
        for (int k = 0; k < list.size() && k < kMaxCandidates; ++k) {
            const timeline::Face &f = m_faces[list[k]];
            out << QVariantMap{{QStringLiteral("faceId"), f.faceId},
                               {QStringLiteral("photoId"), f.photoId},
                               {QStringLiteral("cropUrl"), thumbnailUrl(f.cropPath)},
                               {QStringLiteral("photoName"), f.photoName},
                               {QStringLiteral("current"), f.faceId == current}};
        }
        break;
    }
    return out;
}

// --------------------------------------------------------------------------- //
// Escolhas
// --------------------------------------------------------------------------- //

void TimelineController::choose(const QString &period, const QString &faceId)
{
    const int i = entryIndex(period);
    if (i < 0 || !faceById(faceId))
        return;
    timeline::Entry &e = m_entries[i];
    e.faceId = faceId;
    e.user = true;
    m_catalog->saveTimelinePicks(m_personId, m_frequency, {{period, faceId, true, e.hidden}});
    rebuild();
}

void TimelineController::setHidden(const QString &period, bool hidden)
{
    const int i = entryIndex(period);
    if (i < 0 || m_entries[i].hidden == hidden)
        return;
    timeline::Entry &e = m_entries[i];
    e.hidden = hidden;
    m_catalog->saveTimelinePicks(m_personId, m_frequency, {{period, e.faceId, e.user, hidden}});
    rebuild();
}

void TimelineController::reset()
{
    if (m_personId.isEmpty())
        return;
    m_catalog->resetTimeline(m_personId, m_frequency);
    reload();
}

// --------------------------------------------------------------------------- //
// Exportar
// --------------------------------------------------------------------------- //

void TimelineController::exportImage(const QUrl &file)
{
    if (m_exporting)
        return;
    QString path = file.isLocalFile() ? file.toLocalFile() : file.toString();
    if (path.isEmpty())
        return;
    if (QFileInfo(path).suffix().isEmpty())
        path += QStringLiteral(".jpg");

    QList<timeline::Face> faces;
    QStringList labels;
    for (const timeline::Entry &e : std::as_const(m_entries)) {
        const timeline::Face *f = faceById(e.faceId);
        if (e.hidden || !f)
            continue;
        faces << *f;
        labels << periodLabel(e.period);
    }
    if (faces.isEmpty())
        return;
    imaging::CollageStyle style;
    style.columns = effectiveColumns();
    style.labels = m_labels;
    imaging::ExportInfo info;
    info.title = tr("%1 over the years").arg(m_personLabel);
    info.software = QStringLiteral(PHOTOIDX_BRAND_NAME " " PHOTOIDX_VERSION);
    info.url = QStringLiteral(PHOTOIDX_BRAND_URL);

    m_exporting = true;
    emit exportingChanged();
    qCInfo(lcTimeline) << "Exporting" << faces.size() << "photo(s) to" << path;
    auto *watcher = new QFutureWatcher<QString>(this);
    connect(watcher, &QFutureWatcher<QString>::finished, this, [this, watcher, path]() {
        watcher->deleteLater();
        const QString error = watcher->result();
        m_exporting = false;
        emit exportingChanged();
        if (error.isEmpty()) {
            emit notice(QStringLiteral("timeline.exported"), {{QStringLiteral("path"), QDir::toNativeSeparators(path)}});
        } else {
            qCWarning(lcTimeline) << "Export failed:" << error;
            emit failed(QStringLiteral("timeline.exportFailed"), {{QStringLiteral("detail"), error}});
        }
    });
    watcher->setFuture(QtConcurrent::run(&m_pool, [faces, labels, style, info, path]() -> QString {
        // Exceção nunca escapa de um worker (CLAUDE.md, regra 4).
        try {
            PIX_PERF("TimelineController::export");
            QList<QImage> tiles;
            tiles.reserve(faces.size());
            for (const timeline::Face &f : faces) {
                QImage tile = portraitOf(f, kExportSource, kExportTile);
                if (tile.isNull()) {
                    tile = QImage(kExportTile.width, kExportTile.height, QImage::Format_RGB32);
                    tile.fill(Qt::lightGray);
                }
                tiles << tile;
            }
            return imaging::saveExport(imaging::composeCollage(tiles, labels, style), path, info);
        } catch (const std::bad_alloc &) {
            return QStringLiteral("out of memory");
        } catch (const std::exception &e) {
            return QString::fromUtf8(e.what());
        }
    }));
}
