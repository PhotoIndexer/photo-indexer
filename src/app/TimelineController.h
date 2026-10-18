// SPDX-License-Identifier: BSD-2-Clause
#pragma once

// "Ao longo dos anos" (App.timeline no QML): uma foto da pessoa por período
// (mês, trimestre, semestre ou ano), montadas numa imagem só para exportar.
//
// A regra (agrupar, ordenar, resolver as escolhas) é a de core/Timeline. As
// escolhas ficam no catálogo (person_timeline): a primeira é automática e a
// pessoa troca a foto de um período ou o tira da montagem; voltar à tela
// mostra a mesma montagem. As faces vêm pelo CatalogReader; os recortes de
// retrato, de um provider assíncrono; a exportação, de uma thread própria.

#include "core/Timeline.h"

#include <QObject>
#include <QThreadPool>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

namespace pix {
class Catalog;
class CatalogReader;
}

class TimelineController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString personId READ personId NOTIFY changed)
    Q_PROPERTY(QString personLabel READ personLabel NOTIFY changed)
    /// "month" | "quarter" | "semester" | "year" (lembrada nas preferências).
    Q_PROPERTY(QString frequency READ frequency WRITE setFrequency NOTIFY changed)
    /// Colunas da montagem (0 = automático) e a faixa com o período.
    Q_PROPERTY(int columns READ columns WRITE setColumns NOTIFY layoutChanged)
    Q_PROPERTY(int effectiveColumns READ effectiveColumns NOTIFY layoutChanged)
    Q_PROPERTY(bool labels READ labels WRITE setLabels NOTIFY layoutChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY changed)
    /// Os períodos em ordem: [{period, label, faceId, photoId, photoName,
    /// imageUrl, user, hidden, candidates}]
    Q_PROPERTY(QVariantList entries READ entries NOTIFY changed)
    /// Quantos entram na montagem (os não tirados).
    Q_PROPERTY(int shownCount READ shownCount NOTIFY changed)
    /// O período aberto no painel de troca ("" = nenhum).
    Q_PROPERTY(QString selected READ selected WRITE setSelected NOTIFY selectionChanged)
    Q_PROPERTY(QVariantMap selectedEntry READ selectedEntry NOTIFY selectionChanged)
    /// As fotos do período aberto, da melhor para a pior: [{faceId, photoId,
    /// cropUrl, photoName, current}]
    Q_PROPERTY(QVariantList candidates READ candidates NOTIFY selectionChanged)
    Q_PROPERTY(bool exporting READ exporting NOTIFY exportingChanged)

public:
    TimelineController(pix::Catalog *catalog, pix::CatalogReader *reader, QObject *parent = nullptr);

    QString personId() const { return m_personId; }
    QString personLabel() const { return m_personLabel; }
    QString frequency() const { return m_frequency; }
    void setFrequency(const QString &frequency);
    int columns() const { return m_columns; }
    void setColumns(int columns);
    int effectiveColumns() const;
    bool labels() const { return m_labels; }
    void setLabels(bool on);
    bool loading() const { return m_loading; }
    QVariantList entries() const { return m_entryList; }
    int shownCount() const;
    QString selected() const { return m_selected; }
    void setSelected(const QString &period);
    QVariantMap selectedEntry() const;
    QVariantList candidates() const;
    bool exporting() const { return m_exporting; }

    /// Abre a pessoa (relê as faces e as escolhas).
    Q_INVOKABLE void open(const QString &personId, const QString &label);
    Q_INVOKABLE void reload();
    /// A foto do período passa a ser ``faceId`` (escolha da pessoa).
    Q_INVOKABLE void choose(const QString &period, const QString &faceId);
    /// Tira o período da montagem (ou devolve).
    Q_INVOKABLE void setHidden(const QString &period, bool hidden);
    /// Esquece as escolhas desta frequência: tudo automático de novo.
    Q_INVOKABLE void reset();
    /// Grava a montagem (JPEG ou PNG, pela extensão) com EXIF do app.
    Q_INVOKABLE void exportImage(const QUrl &file);

    /// "2016", "Mar 2016", "Q1 2016", "H2 2016" (no idioma da interface).
    static QString periodLabel(const QString &period);

signals:
    void changed();
    void layoutChanged();
    void selectionChanged();
    void exportingChanged();
    void failed(const QString &key, const QVariantMap &params);
    void notice(const QString &key, const QVariantMap &params);

private:
    void rebuild();
    const pix::timeline::Face *faceById(const QString &faceId) const;
    int entryIndex(const QString &period) const;

    pix::Catalog *m_catalog;
    pix::CatalogReader *m_reader;
    QThreadPool m_pool;

    QString m_personId;
    QString m_personLabel;
    QString m_frequency;
    int m_columns = 0;
    bool m_labels = true;
    bool m_loading = false;
    bool m_exporting = false;
    int m_serial = 0;

    QList<pix::timeline::Face> m_faces;
    QHash<QString, int> m_faceIndex;   ///< faceId -> índice em m_faces
    QList<QPair<QString, QList<int>>> m_groups;
    QList<pix::timeline::Entry> m_entries;
    QVariantList m_entryList;
    QString m_selected;
};
