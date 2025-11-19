#pragma once

#include "Check.h"
#include "RtosCompat.h"
#include <memory>
#include <functional>

namespace tt {

class ScopedLock;

/** Represents a lock/mutex */
class Lock {

public:

    virtual ~Lock() = default;

    virtual bool lock(TickType_t timeout) const = 0;

    bool lock() const { return lock(portMAX_DELAY); }

    virtual bool unlock() const = 0;

    void withLock(TickType_t timeout, const std::function<void()>& onLockAcquired) const {
        if (lock(timeout)) {
            onLockAcquired();
            unlock();
        }
    }

    void withLock(TickType_t timeout, const std::function<void()>& onLockAcquired, const std::function<void()>& onLockFailure) const {
        if (lock(timeout)) {
            onLockAcquired();
            unlock();
        } else {
            onLockFailure();
        }
    }

    void withLock(const std::function<void()>& onLockAcquired) const { withLock(portMAX_DELAY, onLockAcquired); }

    void withLock(const std::function<void()>& onLockAcquired, const std::function<void()>& onLockFailed) const { withLock(portMAX_DELAY, onLockAcquired, onLockFailed); }

    ScopedLock asScopedLock() const;
};

/**
 * Represents a lockable instance that is scoped to a specific lifecycle.
 * Once the ScopedLock is destroyed, unlock() is called automatically.
 *
 * In other words:
 * You have to lock() this object manually, but unlock() happens automatically on destruction.
 */
class ScopedLock final : public Lock {

    const Lock& lockable;
    mutable bool acquired = false;

public:

    using Lock::lock;

    explicit ScopedLock(const Lock& lockable) : lockable(lockable) {}

    ~ScopedLock() override {
        if (acquired) {
            // Best-effort unlock only if we actually acquired it
            lockable.unlock();
            acquired = false;
        }
    }

    bool lock(TickType_t timeout) const override {
        if (lockable.lock(timeout)) {
            acquired = true;
            return true;
        }
        return false;
    }

    bool unlock() const override {
        if (!acquired) {
            // Nothing to unlock
            return true;
        }
        bool ok = lockable.unlock();
        if (ok) {
            acquired = false;
        }
        return ok;
    }
};

}
