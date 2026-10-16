// SPDX-License-Identifier: BSD-2-Clause
#pragma once

// O que o CLIP e as faces compartilham do ONNX Runtime: um Ort::Env só no
// processo, o caminho do modelo no formato do SO e os nomes de entradas e
// saídas de uma sessão. Só existe com PHOTOIDX_HAVE_ONNX.

#ifdef PHOTOIDX_HAVE_ONNX

#include <QString>

#include <onnxruntime_cxx_api.h>

#include <memory>
#include <string>
#include <vector>

namespace pix::imaging::onnx {

Ort::Env &env();

/// Sessão de CPU para ``path`` (lança Ort::Exception se não carregar).
std::unique_ptr<Ort::Session> open(const QString &path, int intraThreads);

std::vector<std::string> inputNames(Ort::Session &s);
std::vector<std::string> outputNames(Ort::Session &s);
std::vector<const char *> cstrs(const std::vector<std::string> &names);

} // namespace pix::imaging::onnx

#endif
