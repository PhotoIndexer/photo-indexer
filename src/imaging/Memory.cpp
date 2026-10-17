// SPDX-License-Identifier: BSD-2-Clause
#include "imaging/Memory.h"

#include <QMutex>
#include <QWaitCondition>

#include <algorithm>

#if defined(Q_OS_WIN)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace pix::imaging {

qint64 physicalMemory()
{
#if defined(Q_OS_WIN)
    MEMORYSTATUSEX status{};
    status.dwLength = sizeof(status);
    return GlobalMemoryStatusEx(&status) ? qint64(status.ullTotalPhys) : 0;
#else
    const long pages = sysconf(_SC_PHYS_PAGES);
    const long pageSize = sysconf(_SC_PAGE_SIZE);
    return pages > 0 && pageSize > 0 ? qint64(pages) * pageSize : 0;
#endif
}

struct MemoryBudget::Private
{
    mutable QMutex lock;
    QWaitCondition freed;
    qint64 capacity = 0;
    qint64 used = 0;
};

MemoryBudget::MemoryBudget()
    : d(new Private)
{
    constexpr qint64 kMB = 1024 * 1024;
    const qint64 ram = physicalMemory();
    d->capacity = ram > 0 ? std::clamp(ram / 4, 512 * kMB, 4096 * kMB) : 1024 * kMB;
}

MemoryBudget &MemoryBudget::instance()
{
    static MemoryBudget budget;
    return budget;
}

qint64 MemoryBudget::capacity() const
{
    QMutexLocker locker(&d->lock);
    return d->capacity;
}

void MemoryBudget::setCapacity(qint64 bytes)
{
    QMutexLocker locker(&d->lock);
    d->capacity = std::max<qint64>(bytes, 64 * 1024 * 1024);
    d->freed.wakeAll();
}

qint64 MemoryBudget::used() const
{
    QMutexLocker locker(&d->lock);
    return d->used;
}

qint64 MemoryBudget::acquire(qint64 bytes)
{
    QMutexLocker locker(&d->lock);
    const qint64 want = std::min(bytes, d->capacity);
    // Cabe, ou não há mais ninguém usando (a maior que o orçamento roda sozinha).
    while (d->used > 0 && d->used + want > d->capacity)
        d->freed.wait(&d->lock);
    d->used += want;
    return want;
}

void MemoryBudget::release(qint64 bytes)
{
    QMutexLocker locker(&d->lock);
    d->used = std::max<qint64>(0, d->used - bytes);
    d->freed.wakeAll();
}

} // namespace pix::imaging
