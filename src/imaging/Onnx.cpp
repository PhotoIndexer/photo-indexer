// SPDX-License-Identifier: BSD-2-Clause
#include "imaging/Onnx.h"

#ifdef PHOTOIDX_HAVE_ONNX

#include <QDir>
#include <QFile>

namespace pix::imaging::onnx {

Ort::Env &env()
{
    static Ort::Env instance(ORT_LOGGING_LEVEL_WARNING, "photoidx");
    return instance;
}

std::unique_ptr<Ort::Session> open(const QString &path, int intraThreads)
{
    Ort::SessionOptions opts;
    opts.SetIntraOpNumThreads(intraThreads);
    opts.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
#ifdef _WIN32
    const std::wstring native = QDir::toNativeSeparators(path).toStdWString();
    return std::make_unique<Ort::Session>(env(), native.c_str(), opts);
#else
    const std::string native = QFile::encodeName(path).toStdString();
    return std::make_unique<Ort::Session>(env(), native.c_str(), opts);
#endif
}

std::vector<std::string> inputNames(Ort::Session &s)
{
    Ort::AllocatorWithDefaultOptions alloc;
    std::vector<std::string> names;
    for (size_t i = 0; i < s.GetInputCount(); ++i)
        names.emplace_back(s.GetInputNameAllocated(i, alloc).get());
    return names;
}

std::vector<std::string> outputNames(Ort::Session &s)
{
    Ort::AllocatorWithDefaultOptions alloc;
    std::vector<std::string> names;
    for (size_t i = 0; i < s.GetOutputCount(); ++i)
        names.emplace_back(s.GetOutputNameAllocated(i, alloc).get());
    return names;
}

std::vector<const char *> cstrs(const std::vector<std::string> &names)
{
    std::vector<const char *> out;
    out.reserve(names.size());
    for (const std::string &n : names)
        out.push_back(n.c_str());
    return out;
}

} // namespace pix::imaging::onnx

#endif
