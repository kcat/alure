#ifndef RINGBUF_H
#define RINGBUF_H

#include "alure2.h"

#include <atomic>
#include <array>

namespace alure
{

/* NOTE: This lockless ringbuffer implementation is based on JACK's, extended
 * to include an element size. Consequently, parameters and return values for a
 * size or count is in 'elements', not bytes. Additionally, it only supports
 * single-consumer/single-provider operation.
 */
class RingBuffer {
    std::atomic<size_t> mWritePtr;
    std::atomic<size_t> mReadPtr;
    size_t mSize;
    size_t mSizeMask;
    size_t mElemSize;

    UniquePtr<char[]> mBuffer;

public:
    struct Data {
        char *buf;
        size_t len;
    };

    /* Create a new ringbuffer to hold at least `sz' elements of `elem_sz'
     * bytes. The number of elements is rounded up to the next power of two.
     */
    RingBuffer(size_t sz, size_t elem_sz);
    RingBuffer(const RingBuffer&) = delete;

    /* Reset the read and write pointers to zero. This is not thread safe. */
    void reset();

    /* Return the number of elements available for reading. This is the number
     * of elements in front of the read pointer and behind the write pointer.
     */
    size_t read_space() const;

    /* Return the number of elements available for writing. This is the number
     * of elements in front of the write pointer and behind the read pointer.
     */
    size_t write_space() const;

    /* The copying data reader. Copy at most `cnt' elements to `dest'. Returns
     * the actual number of elements copied.
     */
    size_t read(char *dest, size_t cnt);

    /* The copying data reader w/o read pointer advance. Copy at most `cnt'
     * elements to `dest'. Returns the actual number of elements copied.
     */
    size_t peek(char *dest, size_t cnt);

    /* The copying data writer. Copy at most `cnt' elements from `src'. Returns
     * the actual number of elements copied.
     */
    size_t write(const char *src, size_t cnt);

    /* Advance the read pointer `cnt' places. */
    void read_advance(size_t cnt);

    /* Advance the write pointer `cnt' places. */
    void write_advance(size_t cnt);

    /* The non-copying data reader. `vec' is an array of two places. Set the
     * values at `vec' to hold the current readable data. If the readable data
     * is in one segment the second segment has zero length.
     */
    Array<Data,2> get_read_vector() const;

    /* The non-copying data writer. `vec' is an array of two places. Set the
     * values at `vec' to hold the current writeable data. If the writeable
     * data is in one segment the second segment has zero length.
     */
    Array<Data,2> get_write_vector() const;
};

} // namespace alure

#endif /* RINGBUF_H */
