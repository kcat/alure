#ifndef REFCOUNT_H
#define REFCOUNT_H

#include <atomic>

namespace alure {

typedef std::atomic<unsigned long> RefCount;

} // namespace alure

#endif /* REFCOUNT_H */
