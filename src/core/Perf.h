// SPDX-License-Identifier: BSD-2-Clause
#pragma once

// Telemetria de desempenho: onde a interface trava. Vai para
// ~/.photo_indexer/logs/perf.log (rotativo, 5 MB × 2).
//
//   PIX_PERF("Catalog::tree");      // no começo de uma função/bloco pesado
//
// Três coisas:
//   1. Escopos medidos (PIX_PERF): na thread principal, cada um acima de
//      kSlowMs entra no log com o tempo; em todas as threads, soma contagem,
//      total e máximo por rótulo.
//   2. Vigia da interface (startWatchdog): um "batimento" na thread principal
//      a cada 50 ms e uma thread que o observa. Sem batimento por mais de
//      250 ms = interface travada: registra quanto tempo e o que estava
//      rodando (amostra o escopo atual a cada 100 ms durante a travada;
//      "(fora de escopo medido)" = QML, desenho ou algo não instrumentado).
//   3. Resumo a cada 60 s: os rótulos que mais ocuparam a thread principal.
//
// Custo: um QElapsedTimer por escopo e um mutex só ao fechar escopos lentos
// ou de outras threads — desprezível perto do que se mede.

#include <QString>

namespace pix::perf {

/// Escopo na thread principal acima disto (ms) entra no log.
inline constexpr int kSlowMs = 40;

/// Liga o vigia e o arquivo perf.log (chamar uma vez, na thread principal).
void startWatchdog();
void stopWatchdog();

/// Grava uma linha no perf.log (thread-safe).
void log(const QString &line);

class Scope
{
public:
    explicit Scope(const char *label);
    ~Scope();
    Scope(const Scope &) = delete;
    Scope &operator=(const Scope &) = delete;

private:
    const char *m_label;
    const char *m_parent = nullptr;
    qint64 m_start = 0;
    bool m_main = false;
};

} // namespace pix::perf

#define PIX_PERF_CAT2(a, b) a##b
#define PIX_PERF_CAT(a, b) PIX_PERF_CAT2(a, b)
#define PIX_PERF(label) ::pix::perf::Scope PIX_PERF_CAT(pixPerfScope_, __LINE__)(label)
