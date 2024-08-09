#pragma once
#include <atomic>

/// A single-writer, single-reader concurrent ring buffer.
template <class T>
class RingBuffer
{
private:
    std::atomic_size_t _write_ptr;
    std::atomic_size_t _read_ptr;
    
    T* _data;
    const std::size_t _capacity;

public:
    RingBuffer<T>(RingBuffer<T>&) = delete;

    RingBuffer<T>(RingBuffer<T> &&src) :
        _write_ptr(src._write_ptr.load()),
        _read_ptr(src._read_ptr.load()),
        _data(src._data),
        _capacity(src._capacity)
    {
        src._data = nullptr;
    }

    RingBuffer<T>(std::size_t capacity) : _capacity(capacity)
    {
        _write_ptr = 0;
        _read_ptr = 0;
        _data = new T[capacity];
    }


    ~RingBuffer<T>()
    {
        delete[] _data;
    }

    /// Attempt to write a number of elements to the ring buffer.
    /// Returns true on success, and false otherwise.
    bool write(T *src, std::size_t count)
    {
        if (available_for_write() < count) return false;
        std::size_t write = _write_ptr;
        /*std::size_t read = _read_ptr;
        std::size_t write = _write_ptr;
        
        std::size_t available;
        if (read > write) {
            available = read - write - 1;
        } else {
            available = _capacity - write + read - 1;
        }

        if (available < count) return false;*/

        for (std::size_t i = 0; i < count; i++) {
            write %= _capacity;
            _data[write] = src[i];
            write++;
        }

        _write_ptr = write;
        return true;
    }

    /// Attempt to read a number of elements from the ring buffer.
    /// Returns true on success, and false otherwise.
    bool read(T *dst, std::size_t count)
    {
        if (available_for_read() < count) return false;
        std::size_t read = _read_ptr;

        for (std::size_t i = 0; i < count; i++) {
            read %= _capacity;
            dst[i] = _data[read];
            read++;
        }

        _read_ptr = read;
        return true;
    }

    /// Discard a number of elements from the ring buffer.
    /// Returns true on success, and false otherwise.
    bool discard(std::size_t count)
    {
        if (available_for_read() < count) return false;
        std::size_t read = _read_ptr;

        for (std::size_t i = 0; i < count; i++) {
            read = (read % _capacity) + 1;
        }

        _read_ptr = read;
        return true;
    }

    /// Returns the amount of space available for writing.
    std::size_t available_for_write() const
    {
        std::size_t read = _read_ptr;
        std::size_t write = _write_ptr;

        std::size_t available;
        if (read > write) {
            available = read - write - 1;
        } else {
            available = _capacity - write + read;
        }

        return available;
    }

    /// Returns the amount of space available for reading.
    std::size_t available_for_read() const
    {
        std::size_t read = _read_ptr;
        std::size_t write = _write_ptr;

        std::size_t available;
        if (write >= read) {
            available = write - read;
        } else {
            available = _capacity - read + write;
        }

        return available;
    }

    std::size_t size() const
    {
        return _capacity;
    }
};