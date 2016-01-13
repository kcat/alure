
#include "ringbuf.h"

#include <cstdlib>
#include <cstring>
#include <atomic>
#include <limits>


/* NOTE: This lockless ringbuffer implementation is based on JACK's, extended
 * to include an element size. Consequently, parameters and return values for a
 * size or count is in 'elements', not bytes. Additionally, it only supports
 * single-consumer/single-provider operation.
 */
struct ll_ringbuffer {
    std::atomic<size_t> write_ptr;
    std::atomic<size_t> read_ptr;
    size_t size;
    size_t size_mask;
    size_t elem_size;
    int mlocked;

    alignas(16) char buf[];
};

/* Create a new ringbuffer to hold at least `sz' elements of `elem_sz' bytes.
 * The number of elements is rounded up to the next power of two. */
ll_ringbuffer_t *ll_ringbuffer_create(size_t sz, size_t elem_sz)
{
    size_t power_of_two = 1;
    if(sz > 1)
    {
        power_of_two = sz - 1;
        power_of_two |= power_of_two>>1;
        power_of_two |= power_of_two>>2;
        power_of_two |= power_of_two>>4;
        power_of_two |= power_of_two>>8;
        power_of_two |= power_of_two>>16;
        if(sizeof(power_of_two) > 4)
            power_of_two |= power_of_two>>32;
        ++power_of_two;
    }
    if(power_of_two < sz || std::numeric_limits<size_t>::max()/elem_sz <= power_of_two ||
       std::numeric_limits<size_t>::max()-sizeof(ll_ringbuffer_t) < power_of_two*elem_sz)
        return nullptr;

    ll_ringbuffer_t *rb = reinterpret_cast<ll_ringbuffer_t*>(new char[sizeof(ll_ringbuffer_t) + power_of_two*elem_sz]);
    rb->size = power_of_two;
    rb->size_mask = rb->size - 1;
    rb->elem_size = elem_sz;
    rb->write_ptr = 0;
    rb->read_ptr = 0;
    rb->mlocked = 0;
    return rb;
}

/* Free all data associated with the ringbuffer `rb'. */
void ll_ringbuffer_free(ll_ringbuffer_t *rb)
{
    if(rb)
    {
#ifdef USE_MLOCK
        if(rb->mlocked)
            munlock(rb, sizeof(*rb) + rb->size*rb->elem_size);
#endif /* USE_MLOCK */
        delete[] reinterpret_cast<char*>(rb);
    }
}

/* Lock the data block of `rb' using the system call 'mlock'. */
int ll_ringbuffer_mlock(ll_ringbuffer_t *rb)
{
#ifdef USE_MLOCK
    if(!rb->locked && mlock(rb, sizeof(*rb) + rb->size*rb->elem_size))
        return -1;
#endif /* USE_MLOCK */
    rb->mlocked = 1;
    return 0;
}

/* Reset the read and write pointers to zero. This is not thread safe. */
void ll_ringbuffer_reset(ll_ringbuffer_t *rb)
{
    rb->read_ptr = 0;
    rb->write_ptr = 0;
    memset(rb->buf, 0, rb->size*rb->elem_size);
}

/* Return the number of elements available for reading. This is the number of
 * elements in front of the read pointer and behind the write pointer. */
size_t ll_ringbuffer_read_space(const ll_ringbuffer_t *rb)
{
    size_t w = rb->write_ptr.load();
    size_t r = rb->read_ptr.load();
    return (rb->size+w-r) & rb->size_mask;
}
/* Return the number of elements available for writing. This is the number of
 * elements in front of the write pointer and behind the read pointer. */
size_t ll_ringbuffer_write_space(const ll_ringbuffer_t *rb)
{
    size_t w = rb->write_ptr.load();
    size_t r = rb->read_ptr.load();
    return (rb->size+r-w-1) & rb->size_mask;
}

/* The copying data reader. Copy at most `cnt' elements from `rb' to `dest'.
 * Returns the actual number of elements copied. */
size_t ll_ringbuffer_read(ll_ringbuffer_t *rb, char *dest, size_t cnt)
{
    size_t read_ptr;
    size_t free_cnt;
    size_t cnt2;
    size_t to_read;
    size_t n1, n2;

    read_ptr = rb->read_ptr.load();
    free_cnt = (rb->size+rb->write_ptr.load()-read_ptr) & rb->size_mask;
    if(free_cnt == 0) return 0;

    to_read = (cnt > free_cnt) ? free_cnt : cnt;
    cnt2 = read_ptr + to_read;
    if(cnt2 > rb->size)
    {
        n1 = rb->size - read_ptr;
        n2 = cnt2 & rb->size_mask;
        memcpy(dest, &(rb->buf[read_ptr*rb->elem_size]), n1*rb->elem_size);
        memcpy(dest + n1*rb->elem_size, rb->buf, n2*rb->elem_size);
        rb->read_ptr.store((read_ptr + n1 + n2) & rb->size_mask);
    }
    else
    {
        n1 = to_read;
        memcpy(dest, &(rb->buf[read_ptr*rb->elem_size]), n1*rb->elem_size);
        rb->read_ptr.store((read_ptr + n1) & rb->size_mask);
    }

    return to_read;
}

/* The copying data reader w/o read pointer advance. Copy at most `cnt'
 * elements from `rb' to `dest'. Returns the actual number of elements copied.
 */
size_t ll_ringbuffer_peek(ll_ringbuffer_t *rb, char *dest, size_t cnt)
{
    size_t read_ptr;
    size_t free_cnt;
    size_t cnt2;
    size_t to_read;
    size_t n1, n2;

    read_ptr = rb->read_ptr.load();
    free_cnt = (rb->size+rb->write_ptr.load()-read_ptr) & rb->size_mask;
    if(free_cnt == 0) return 0;

    to_read = (cnt > free_cnt) ? free_cnt : cnt;
    cnt2 = read_ptr + to_read;
    if(cnt2 > rb->size)
    {
        n1 = rb->size - read_ptr;
        n2 = cnt2 & rb->size_mask;
        memcpy(dest, &(rb->buf[read_ptr*rb->elem_size]), n1*rb->elem_size);
        memcpy(dest + n1*rb->elem_size, rb->buf, n2*rb->elem_size);
    }
    else
    {
        n1 = to_read;
        memcpy(dest, &(rb->buf[read_ptr*rb->elem_size]), n1*rb->elem_size);
    }

    return to_read;
}

/* The copying data writer. Copy at most `cnt' elements to `rb' from `src'.
 * Returns the actual number of elements copied. */
size_t ll_ringbuffer_write(ll_ringbuffer_t *rb, const char *src, size_t cnt)
{
    size_t write_ptr;
    size_t free_cnt;
    size_t cnt2;
    size_t to_write;
    size_t n1, n2;

    write_ptr = rb->write_ptr.load();
    free_cnt = (rb->size+rb->read_ptr.load()-write_ptr-1) & rb->size_mask;
    if(free_cnt == 0) return 0;

    to_write = (cnt > free_cnt) ? free_cnt : cnt;
    cnt2 = write_ptr + to_write;
    if(cnt2 > rb->size)
    {
        n1 = rb->size - write_ptr;
        n2 = cnt2 & rb->size_mask;
        memcpy(&(rb->buf[write_ptr*rb->elem_size]), src, n1*rb->elem_size);
        memcpy(rb->buf, src + n1*rb->elem_size, n2*rb->elem_size);
        rb->write_ptr.store((write_ptr + n1 + n2) & rb->size_mask);
    }
    else
    {
        n1 = to_write;
        memcpy(&(rb->buf[write_ptr*rb->elem_size]), src, n1*rb->elem_size);
        rb->write_ptr.store((write_ptr + n1) & rb->size_mask);
    }

    return to_write;
}

/* Advance the read pointer `cnt' places. */
void ll_ringbuffer_read_advance(ll_ringbuffer_t *rb, size_t cnt)
{
    size_t tmp = (rb->read_ptr.load() + cnt) & rb->size_mask;
    rb->read_ptr.store(tmp);
}

/* Advance the write pointer `cnt' places. */
void ll_ringbuffer_write_advance(ll_ringbuffer_t *rb, size_t cnt)
{
    size_t tmp = (rb->write_ptr.load() + cnt) & rb->size_mask;
    rb->write_ptr.store(tmp);
}

/* The non-copying data reader. `vec' is an array of two places. Set the values
 * at `vec' to hold the current readable data at `rb'. If the readable data is
 * in one segment the second segment has zero length. */
void ll_ringbuffer_get_read_vector(const ll_ringbuffer_t *rb, ll_ringbuffer_data_t *vec)
{
    size_t free_cnt;
    size_t cnt2;
    size_t w, r;

    w = rb->write_ptr.load();
    r = rb->read_ptr.load();
    free_cnt = (rb->size+w-r) & rb->size_mask;

    cnt2 = r + free_cnt;
    if(cnt2 > rb->size)
    {
        /* Two part vector: the rest of the buffer after the current write ptr,
         * plus some from the start of the buffer. */
        vec[0].buf = (char*)&(rb->buf[r*rb->elem_size]);
        vec[0].len = rb->size - r;
        vec[1].buf = (char*)rb->buf;
        vec[1].len = cnt2 & rb->size_mask;
    }
    else
    {
        /* Single part vector: just the rest of the buffer */
        vec[0].buf = (char*)&(rb->buf[r*rb->elem_size]);
        vec[0].len = free_cnt;
        vec[1].buf = nullptr;
        vec[1].len = 0;
    }
}

/* The non-copying data writer. `vec' is an array of two places. Set the values
 * at `vec' to hold the current writeable data at `rb'. If the writeable data
 * is in one segment the second segment has zero length. */
void ll_ringbuffer_get_write_vector(const ll_ringbuffer_t *rb, ll_ringbuffer_data_t *vec)
{
    size_t free_cnt;
    size_t cnt2;
    size_t w, r;

    w = rb->write_ptr.load();
    r = rb->read_ptr.load();
    free_cnt = (rb->size+r-w-1) & rb->size_mask;

    cnt2 = w + free_cnt;
    if(cnt2 > rb->size)
    {
        /* Two part vector: the rest of the buffer after the current write ptr,
         * plus some from the start of the buffer. */
        vec[0].buf = (char*)&(rb->buf[w*rb->elem_size]);
        vec[0].len = rb->size - w;
        vec[1].buf = (char*)rb->buf;
        vec[1].len = cnt2 & rb->size_mask;
    }
    else
    {
        vec[0].buf = (char*)&(rb->buf[w*rb->elem_size]);
        vec[0].len = free_cnt;
        vec[1].buf = nullptr;
        vec[1].len = 0;
    }
}
