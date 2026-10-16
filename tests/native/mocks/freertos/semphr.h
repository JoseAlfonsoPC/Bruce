#pragma once
#include <mutex>

struct FakeRecursiveMutex { std::recursive_mutex mutex; };
using SemaphoreHandle_t = FakeRecursiveMutex *;
inline FakeRecursiveMutex fakeRecursiveMutex;
inline SemaphoreHandle_t xSemaphoreCreateRecursiveMutex() { return &fakeRecursiveMutex; }
inline int xSemaphoreTakeRecursive(SemaphoreHandle_t handle, int timeout) {
    if (timeout == 0) return handle->mutex.try_lock();
    handle->mutex.lock();
    return 1;
}
inline void xSemaphoreGiveRecursive(SemaphoreHandle_t handle) { handle->mutex.unlock(); }
