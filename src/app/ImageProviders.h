// SPDX-License-Identifier: BSD-2-Clause
#pragma once

// Imagens do QML.
//
// Miniaturas e recortes de rosto são arquivos prontos em ~/.photo_indexer: o
// QML os abre por file:// (Image { asynchronous: true }). O que precisa ser
// GERADO vem de um provider ASSÍNCRONO — roda no pool de threads e a
// interface nunca trava:
//
//   image://photo/view/<id>            a foto grande do visualizador (o
//                                      original reduzido; RAW = prévia embutida)
//   image://photo/file/<chave>         um arquivo qualquer (foto enviada na
//                                      busca por rosto), orientado
//   image://icon/<nome>?color=#rrggbb  ícone Lucide recolorido
//
// O QML não fala com o banco: o controller (thread principal) registra no
// ImageRegistry quem produz a imagem de cada chave.

#include <QHash>
#include <QMutex>
#include <QQuickAsyncImageProvider>
#include <QQuickImageProvider>

#include <functional>

/// Pedidos de imagem registrados pelos controllers, por chave.
class ImageRegistry
{
public:
    using Producer = std::function<QImage()>;

    static ImageRegistry &instance();

    void put(const QString &kind, const QString &key, Producer producer);
    Producer take(const QString &kind, const QString &key) const;
    void clear();

private:
    mutable QMutex m_lock;
    QHash<QString, Producer> m_producers;
};

class PhotoImageProvider : public QQuickAsyncImageProvider
{
public:
    QQuickImageResponse *requestImageResponse(const QString &id, const QSize &requestedSize) override;
};

class IconProvider : public QQuickImageProvider
{
public:
    IconProvider()
        : QQuickImageProvider(QQuickImageProvider::Image)
    {
    }
    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;
};
