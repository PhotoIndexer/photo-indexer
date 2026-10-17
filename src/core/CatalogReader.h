// SPDX-License-Identifier: BSD-2-Clause
#pragma once

// Leituras da navegação fora da thread principal. Uma thread própria com um
// Catalog SÓ DE LEITURA no mesmo arquivo (Catalog::openReaderOf): árvores do
// painel, listas de pastas/discos/categorias/pessoas, totais e contagens.
// Com o banco em WAL a leitura vê um retrato consistente do último commit e
// não espera nem trava as gravações da principal — pode estar um instante
// atrás, o que para navegar não importa.
//
// O checkpoint do WAL (copiar o log para o banco) também mora aqui: com um
// catálogo de centenas de MB ele levava quase 1 s dentro de um commit da
// thread principal. A principal desliga o automático e esta thread, com uma
// conexão própria, faz um PASSIVE a cada 15 s (não espera ninguém).
//
//   reader->read<QVariantList>(this, [](const Catalog &c) { return c.tree("dates"); },
//                              [this](QVariantList nodes) { m_tree.setNodes(nodes); });
//
// ``work`` roda na thread do leitor (só métodos de consulta); ``done`` volta
// na thread de ``context`` e não roda se ele já não existe. Os pedidos são
// atendidos em ordem, um de cada vez. Catálogo em memória (testes): sem
// segunda conexão possível, ``work`` roda no próprio catálogo principal.

#include "core/Catalog.h"
#include "core/Perf.h"

#include <QObject>
#include <QPointer>
#include <QThread>

#include <functional>
#include <memory>

namespace pix {

class CatalogReader : public QObject
{
    Q_OBJECT

public:
    explicit CatalogReader(const Catalog &main, QObject *parent = nullptr);
    ~CatalogReader() override;

    /// Há uma conexão de leitura em outra thread (catálogo em arquivo).
    bool threaded() const { return m_catalog != nullptr; }

    template<typename R>
    void read(QObject *context, std::function<R(const Catalog &)> work, std::function<void(R)> done)
    {
        if (!m_catalog) {
            done(work(m_main));
            return;
        }
        QPointer<QObject> ctx(context);
        const Catalog *catalog = m_catalog.get();
        QMetaObject::invokeMethod(
            m_worker,
            [catalog, ctx, work = std::move(work), done = std::move(done)]() {
                R result{};
                try {
                    PIX_PERF("CatalogReader::read");
                    result = work(*catalog);
                } catch (const std::exception &) {
                    return;  // banco ocupado/indisponível: o próximo pedido tenta de novo
                }
                if (!ctx)
                    return;
                QMetaObject::invokeMethod(
                    ctx.data(), [ctx, done, result = std::move(result)]() {
                        if (ctx)
                            done(result);
                    },
                    Qt::QueuedConnection);
            },
            Qt::QueuedConnection);
    }

private:
    const Catalog &m_main;
    QThread m_thread;
    QObject *m_worker = nullptr;            ///< vive na m_thread
    std::unique_ptr<Catalog> m_catalog;     ///< criado e usado só na m_thread
    QString m_checkpointConnection;         ///< conexão de escrita só para o checkpoint (na m_thread)
};

} // namespace pix
