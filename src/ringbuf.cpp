
#include "ringbuf.h"

#include <stdexcept>
#include <cstdlib>
#include <cstring>
#include <atomic>
#include <limits>


namespace alure
{

RingBuffer::RingBuffer(size_t sz, size_t elem_sz)
  : mWritePtr(0), mReadPtr(0), mSize(0), mSizeMask(0), mElemSize(elem_sz)
  , mBuffer(nullptr)
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
    if(power_of_two < sz || std::numeric_limits<size_t>::max()/elem_sz <= power_of_two)
        throw std::bad_alloc();

    mSize = power_of_two;
    mSizeMask = mSize - 1;

    mBuffer = std::unique_ptr<char[]>(new char[mSize*mElemSize]);
    std::fill(&(mBuffer[0]), &(mBuffer[mSize*mElemSize]), 0);
}

void RingBuffer::reset()
{
    mReadPtr = 0;
    mWritePtr = 0;
    std::fill(&(mBuffer[0]), &(mBuffer[mSize*mElemSize]), 0);
}

size_t RingBuffer::read_space() const
{
    size_t w = mWritePtr.load();
    size_t r = mReadPtr.load();
    return (mSize+w-r) & mSizeMask;
}

size_t RingBuffer::write_space() const
{
    size_t w = mWritePtr.load();
    size_t r = mReadPtr.load();
    return (mSize+r-w-1) & mSizeMask;
}

size_t RingBuffer::read(char* dest, size_t cnt)
{
    size_t read_ptr;
    size_t free_cnt;
    size_t cnt2;
    size_t to_read;
    size_t n1, n2;

    read_ptr = mReadPtr.load();
    free_cnt = (mSize+mWritePtr.load()-read_ptr) & mSizeMask;
    if(free_cnt == 0) return 0;

    to_read = (cnt > free_cnt) ? free_cnt : cnt;
    cnt2 = read_ptr + to_read;
    if(cnt2 > mSize)
    {
        n1 = mSize - read_ptr;
        n2 = cnt2 & mSizeMask;
        memcpy(dest, &(mBuffer[read_ptr*mElemSize]), n1*mElemSize);
        memcpy(dest + n1*mElemSize, &(mBuffer[0]), n2*mElemSize);
        mReadPtr.store((read_ptr + n1 + n2) & mSizeMask);
    }
    else
    {
        n1 = to_read;
        memcpy(dest, &(mBuffer[read_ptr*mElemSize]), n1*mElemSize);
        mReadPtr.store((read_ptr + n1) & mSizeMask);
    }

    return to_read;
}

size_t RingBuffer::peek(char* dest, size_t cnt)
{
    size_t read_ptr;
    size_t free_cnt;
    size_t cnt2;
    size_t to_read;
    size_t n1, n2;

    read_ptr = mReadPtr.load();
    free_cnt = (mSize+mWritePtr.load()-read_ptr) & mSizeMask;
    if(free_cnt == 0) return 0;

    to_read = (cnt > free_cnt) ? free_cnt : cnt;
    cnt2 = read_ptr + to_read;
    if(cnt2 > mSize)
    {
        n1 = mSize - read_ptr;
        n2 = cnt2 & mSizeMask;
        memcpy(dest, &(mBuffer[read_ptr*mElemSize]), n1*mElemSize);
        memcpy(dest + n1*mElemSize, &(mBuffer[0]), n2*mElemSize);
    }
    else
    {
        n1 = to_read;
        memcpy(dest, &(mBuffer[read_ptr*mElemSize]), n1*mElemSize);
    }

    return to_read;
}

size_t RingBuffer::write(const char* src, size_t cnt)
{
    size_t write_ptr;
    size_t free_cnt;
    size_t cnt2;
    size_t to_write;
    size_t n1, n2;

    write_ptr = mWritePtr.load();
    free_cnt = (mSize+mReadPtr.load()-write_ptr-1) & mSizeMask;
    if(free_cnt == 0) return 0;

    to_write = (cnt > free_cnt) ? free_cnt : cnt;
    cnt2 = write_ptr + to_write;
    if(cnt2 > mSize)
    {
        n1 = mSize - write_ptr;
        n2 = cnt2 & mSizeMask;
        memcpy(&(mBuffer[write_ptr*mElemSize]), src, n1*mElemSize);
        memcpy(&(mBuffer[0]), src + n1*mElemSize, n2*mElemSize);
        mWritePtr.store((write_ptr + n1 + n2) & mSizeMask);
    }
    else
    {
        n1 = to_write;
        memcpy(&(mBuffer[write_ptr*mElemSize]), src, n1*mElemSize);
        mWritePtr.store((write_ptr + n1) & mSizeMask);
    }

    return to_write;
}

void RingBuffer::read_advance(size_t cnt)
{
    size_t tmp = (mReadPtr.load() + cnt) & mSizeMask;
    mReadPtr.store(tmp);
}

void RingBuffer::write_advance(size_t cnt)
{
    size_t tmp = (mWritePtr.load() + cnt) & mSizeMask;
    mWritePtr.store(tmp);
}

std::array<RingBuffer::Data,2> RingBuffer::get_read_vector() const
{
    std::array<Data,2> vec;
    size_t free_cnt;
    size_t cnt2;
    size_t w, r;

    w = mWritePtr.load();
    r = mReadPtr.load();
    free_cnt = (mSize+w-r) & mSizeMask;

    cnt2 = r + free_cnt;
    if(cnt2 > mSize)
    {
        /* Two part vector: the rest of the buffer after the current write ptr,
         * plus some from the start of the buffer. */
        vec[0].buf = &(mBuffer[r*mElemSize]);
        vec[0].len = mSize - r;
        vec[1].buf = &(mBuffer[0]);
        vec[1].len = cnt2 & mSizeMask;
    }
    else
    {
        /* Single part vector: just the rest of the buffer */
        vec[0].buf = &(mBuffer[r*mElemSize]);
        vec[0].len = free_cnt;
        vec[1].buf = nullptr;
        vec[1].len = 0;
    }

    return vec;
}

std::array<RingBuffer::Data,2> RingBuffer::get_write_vector() const
{
    std::array<Data,2> vec;
    size_t free_cnt;
    size_t cnt2;
    size_t w, r;

    w = mWritePtr.load();
    r = mReadPtr.load();
    free_cnt = (mSize+r-w-1) & mSizeMask;

    cnt2 = w + free_cnt;
    if(cnt2 > mSize)
    {
        /* Two part vector: the rest of the buffer after the current write ptr,
         * plus some from the start of the buffer. */
        vec[0].buf = &(mBuffer[w*mElemSize]);
        vec[0].len = mSize - w;
        vec[1].buf = &(mBuffer[0]);
        vec[1].len = cnt2 & mSizeMask;
    }
    else
    {
        vec[0].buf = &(mBuffer[w*mElemSize]);
        vec[0].len = free_cnt;
        vec[1].buf = nullptr;
        vec[1].len = 0;
    }

    return vec;
}

} // namespace alure
