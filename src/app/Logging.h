// SPDX-License-Identifier: BSD-2-Clause
#pragma once

// Log em ~/.photo_indexer/logs/app.log (rotativo, 5 MB x 3) e, quando há terminal,
// também no stderr. O app instalado não tem terminal: log que não vai para
// arquivo não existe. Nos módulos: Q_LOGGING_CATEGORY + qCInfo/qCWarning.
//
// Nível: --debug liga o debug de tudo; PHOTOIDX_LOG_LEVEL (debug | info |
// warning) também, sem precisar de argumento.

#include <QString>

namespace pix::app {

void setupLogging(bool debug);
QString logFile();

} // namespace pix::app
