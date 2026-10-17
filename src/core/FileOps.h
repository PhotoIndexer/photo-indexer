// SPDX-License-Identifier: BSD-2-Clause
#pragma once

// Operações de arquivo do modo Pastas (CLAUDE.md, regra 10.1): mover,
// renomear, criar pasta e mandar para a Lixeira. Só arquivo — sem banco: rodam
// fora da thread principal e quem chama atualiza o catálogo depois.
//
// Nunca sobrescrevem nada e nunca apagam de vez: mover entre volumes copia,
// confere (tamanho + SHA-256) e só então remove o original; sem Lixeira (um
// compartilhamento de rede), a exclusão é recusada. Os erros são chaves de
// i18n ("files.*"), vazias quando deu certo.

#include <QList>
#include <QString>

namespace pix::fileops {

/// Um arquivo que mudou de lugar.
struct Moved
{
    QString from;
    QString to;
};

/// Move (ou renomeia) um arquivo para ``to`` (caminho completo, que não pode
/// existir). Mesmo volume: rename; outro: copia, confere e remove o original.
QString moveFile(const QString &from, const QString &to);

/// Move (ou renomeia) uma pasta inteira para ``to`` (que não pode existir).
/// Entre volumes, arquivo por arquivo: ``moved`` diz quais foram (num erro no
/// meio, os que já foram ficam no destino e o resto na origem — nada some).
QString moveDir(const QString &from, const QString &to, QList<Moved> *moved);

/// Cria ``name`` dentro de ``parent``.
QString makeDir(const QString &parent, const QString &name, QString *created);

/// Manda para a Lixeira do sistema. Sem Lixeira: "files.noTrash".
QString trash(const QString &path);

/// ``fileName`` livre em ``dir``: "IMG_1.JPG", senão "IMG_1 (2).JPG"...
QString freeName(const QString &dir, const QString &fileName);

/// Nome válido para arquivo/pasta (sem separador, sem "." nem "..").
bool validName(const QString &name);

/// ``path`` é ``dir`` ou está dentro dela (comparando caminhos limpos).
bool isInside(const QString &path, const QString &dir);

} // namespace pix::fileops
