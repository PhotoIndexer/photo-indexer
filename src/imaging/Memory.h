// SPDX-License-Identifier: BSD-2-Clause
#pragma once

// Orçamento de memória das decodificações. O hash dos pixels pede a imagem
// INTEIRA na memória (uma foto de 50 MP são ~150 MB por cópia); com várias
// threads decodificando ao mesmo tempo, o processo passava de gigabytes.
// Antes de decodificar, o worker reserva o que a imagem vai ocupar; se não
// cabe, espera as outras devolverem. Uma imagem maior que o orçamento inteiro
// roda sozinha. Thread-safe.

#include <QtGlobal>

namespace pix::imaging {

/// Acima disto a imagem não é decodificada inteira (fica catalogada sem
/// miniatura nem hash): 250 MP em RGB já são 750 MB por cópia.
inline constexpr qint64 kMaxDecodePixels = 250'000'000;

/// Memória física da máquina, em bytes (0 se o SO não disser).
qint64 physicalMemory();

class MemoryBudget
{
public:
    /// O orçamento do processo: 25% da RAM, entre 512 MB e 4 GB.
    static MemoryBudget &instance();

    qint64 capacity() const;
    void setCapacity(qint64 bytes);
    /// Em uso agora (para o painel de tarefas).
    qint64 used() const;

    /// Bloqueia até ``bytes`` caberem (limitado à capacidade). Devolve o que
    /// foi reservado — o que precisa ir para release().
    qint64 acquire(qint64 bytes);
    void release(qint64 bytes);

private:
    MemoryBudget();
    struct Private;
    Private *d;
};

/// Reserva com escopo: devolve no destrutor.
class MemoryLease
{
public:
    explicit MemoryLease(qint64 bytes)
        : m_bytes(bytes > 0 ? MemoryBudget::instance().acquire(bytes) : 0)
    {
    }
    ~MemoryLease()
    {
        if (m_bytes > 0)
            MemoryBudget::instance().release(m_bytes);
    }
    MemoryLease(const MemoryLease &) = delete;
    MemoryLease &operator=(const MemoryLease &) = delete;

private:
    qint64 m_bytes;
};

} // namespace pix::imaging
