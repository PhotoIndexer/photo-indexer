// SPDX-License-Identifier: BSD-2-Clause
#pragma once

// O modo Pastas como um gerenciador de arquivos (App.files no QML): mover
// fotos para uma pasta, mover/renomear/criar pastas, renomear foto e mandar
// para a Lixeira (CLAUDE.md, regra 10.1).
//
// A conferência (destino dentro de uma pasta indexada, pasta indexada não se
// move, nome livre) é na thread principal; os arquivos mudam numa thread só
// deste controller, um pedido por vez (core/FileOps); o catálogo é atualizado
// quando ela termina. Com uma varredura rodando, nada começa.

#include "core/FileOps.h"

#include <QObject>
#include <QThreadPool>
#include <QVariantMap>

#include <functional>

namespace pix {
class Catalog;
class Pipeline;
}
class LibraryController;

class FilesController : public QObject
{
    Q_OBJECT
    /// Um pedido em andamento (a interface mostra e não manda outro).
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)

public:
    FilesController(pix::Catalog *catalog, pix::Pipeline *pipeline, LibraryController *library, QObject *parent = nullptr);

    bool busy() const { return m_busy; }

    /// Fotos para ``targetDir`` (com o nome livre: "x (2).jpg" se já há um x.jpg).
    Q_INVOKABLE void movePhotos(const QStringList &photoIds, const QString &targetDir);
    /// Novo nome (a extensão continua a do arquivo).
    Q_INVOKABLE void renamePhoto(const QString &photoId, const QString &name);
    Q_INVOKABLE void trashPhotos(const QStringList &photoIds);
    /// A pasta (com tudo dentro) para dentro de ``targetParent``.
    Q_INVOKABLE void moveFolder(const QString &dir, const QString &targetParent);
    Q_INVOKABLE void renameFolder(const QString &dir, const QString &name);
    Q_INVOKABLE void createFolder(const QString &parent, const QString &name);
    Q_INVOKABLE void trashFolder(const QString &dir);
    /// A pasta é uma pasta indexada (raiz): não se move, renomeia nem exclui por aqui.
    Q_INVOKABLE bool isRootFolder(const QString &dir) const;
    /// Alguma destas fotos (ou a pasta) está num disco de rede: a confirmação
    /// da Lixeira avisa (no Windows o sistema pode apagar de vez).
    Q_INVOKABLE bool photosOnNetwork(const QStringList &photoIds) const;
    Q_INVOKABLE bool folderOnNetwork(const QString &dir) const;

signals:
    void busyChanged();
    void failed(const QString &key, const QVariantMap &params);
    void notice(const QString &key, const QVariantMap &params);

private:
    struct Result
    {
        QList<pix::fileops::Moved> moved;
        QStringList trashed;
        QString created;
        QString error;          ///< o primeiro erro (chave de i18n)
        QString errorName;      ///< em qual arquivo/pasta
        int total = 0;
    };

    /// Conferências comuns: nada em andamento, nenhuma varredura.
    bool ready();
    /// Roda ``work`` na thread dos arquivos e ``done`` na principal.
    void run(std::function<Result()> work, std::function<void(const Result &)> done);
    /// A grade mostrava uma pasta que mudou de lugar ou saiu: segue junto.
    void follow(const QString &from, const QString &to);

    pix::Catalog *m_catalog;
    pix::Pipeline *m_pipeline;
    LibraryController *m_library;
    QThreadPool m_pool;
    bool m_busy = false;
};
