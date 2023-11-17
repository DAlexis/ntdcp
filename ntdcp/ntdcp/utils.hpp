#pragma once

#include <vector>
#include <list>
#include <limits>
#include <memory>

#include <cstdint>
#include <cstdlib>


namespace ntdcp
{

class Buffer;
class RingBuffer;


class SerialReadAccessor
{
public:
    virtual ~SerialReadAccessor() = default;

    virtual void skip(size_t count) = 0;
    virtual void get(uint8_t* buf, size_t size) const = 0;
    virtual size_t size() const = 0;
    virtual uint8_t operator[](size_t pos) const = 0;

    virtual bool empty() const;
    virtual void extract(uint8_t* buf, size_t size);

    std::shared_ptr<Buffer> extract_buf(size_t extraction_size);
    std::shared_ptr<Buffer> extract_buf();

    template <typename T>
    T as(size_t pos)
    {
        T result;
        for (size_t i = 0; i < sizeof(T); i++)
        {
            reinterpret_cast<uint8_t*>(&result)[i] = (*this)[pos + i];
        }
        return result;
    }

    template <typename T>
    SerialReadAccessor& operator>>(T& target)
    {
        extract(reinterpret_cast<uint8_t*>(&target), sizeof(target));
        return *this;
    }
};

class SerialWriteAccessor
{
private:
    class RawStream;

public:
    virtual bool put(const void* data, size_t size) = 0;
    virtual bool put(SerialReadAccessor& buffer, size_t size) = 0;
    virtual bool put(SerialReadAccessor& buffer);
    virtual bool put(SerialReadAccessor&& buffer);

    template<typename T>
    bool put_copy(const T& data)
    {
        return put(&data, sizeof(data));
    }

    /**
     * @brief Check if data with given size will fit the container
     * @param size   Data size in bytes
     */
    virtual bool will_fit(size_t size) = 0;

    virtual SerialWriteAccessor& operator<<(SerialReadAccessor&& accessor);
    virtual SerialWriteAccessor& operator<<(SerialReadAccessor& accessor);

    virtual ~SerialWriteAccessor() = default;

    /**
     * @brief Create raw data accessor to trivially serialize anything by copying it
     * @return temporary object that redirect data to current SerialWriteAccessor
     */
    RawStream raw();

private:
    class RawStream
    {
    public:
        RawStream(SerialWriteAccessor& write_accessor) :
            m_write_accessor(write_accessor)
        {}

        template<typename T>
        RawStream& operator<<(const T& variable)
        {
            m_write_accessor.put(reinterpret_cast<const void*>(&variable), sizeof(T));
            return *this;
        }
    private:
        SerialWriteAccessor& m_write_accessor;
    };
};

// todo
struct MemBlock : public SerialReadAccessor
{
public:
    MemBlock(const MemBlock& mem_block, size_t offset);
    MemBlock(const uint8_t* begin = nullptr, size_t size = 0);
    MemBlock(const MemBlock&) = default;

    void skip(size_t count) override;
    void get(uint8_t* buf, size_t size) const override;
    size_t size() const override;
    uint8_t operator[](size_t pos) const override;

    MemBlock offset(size_t off) const;

    const uint8_t* begin() const;
    const uint8_t* end() const;

    template<typename T>
    MemBlock& operator>>(T& right)
    {
        right = *reinterpret_cast<const T*>(m_begin);
        m_begin += sizeof(T);
        return *this;
    }

    template<typename T>
    bool can_contain()
    {
        return m_end - m_begin >= sizeof(T);
    }

private:
    const uint8_t* m_begin;
    const uint8_t* m_end;
};


class BitExtractor
{
public:
    BitExtractor(const uint8_t* buffer, size_t max_size);

    uint8_t get(int bits_count);

private:
    const uint8_t* m_buffer;
    size_t m_bytes_left;

    uint8_t m_bits_left = 8;
};


class Buffer : public std::enable_shared_from_this<Buffer>, public SerialWriteAccessor
{
public:
    using ptr = std::shared_ptr<Buffer>;

    static ptr create(size_t size = 0, const void* init_data = nullptr);
    static ptr create(SerialReadAccessor& data, size_t size);
    static ptr create(SerialReadAccessor& data);

    template<typename T>
    static ptr serialize(const T& data)
    {
        return create(sizeof(data), &data);
    }

    bool put(const void* data, size_t size) override;
    bool put(SerialReadAccessor& accessor, size_t size) override;
    bool will_fit(size_t size) override;

    std::vector<uint8_t>& contents();

    ptr clone() const;

    size_t size() const;
    uint8_t* data();
    const uint8_t* data() const;

    void clear();

    uint8_t& at(size_t pos);
    uint8_t& operator[](size_t pos);

    bool operator==(const Buffer& right) const;

private:
    Buffer(size_t size = 0, const void* init_data = nullptr);
    void extend(size_t size);

    std::vector<uint8_t> m_contents;
};

/**
 * @brief The RingBufferClass class is always a serial accessor to itself,
 * because it stores reading pointer and it should be the only one
 */
class RingBuffer : public SerialReadAccessor, public SerialWriteAccessor
{
public:
    RingBuffer(size_t capacity);

    size_t free_space();
    /**
     * @brief Get size of currently stored not readed data
     * @return currently stored data size
     */
    size_t size() const override;

    bool put(const void* src, size_t size) override;
    bool put(SerialReadAccessor& accessor, size_t size) override;
    bool put(Buffer::ptr buf);
    bool will_fit(size_t size) override;

    void get(uint8_t* buf, size_t size) const override;
    void extract(uint8_t* buf, size_t size) override;
    void skip(size_t size) override;

    /**
     * @brief Get largest avaliable continious data block with its size
     *        May be bug here (old DAC case)??? Not used now
     * @param buf   OUT: Pointer to block
     * @param size  OUT: Block size
     */
    MemBlock get_continious_block(size_t size) const;

    bool empty() const override;
    void clear();

    uint8_t operator[](size_t pos) const override;
    uint8_t& operator[](size_t pos);

private:
    std::vector<uint8_t> m_contents;
    uint32_t m_p_write = 0, m_p_read = 0;
};


class SegmentBuffer
{
public:
    SegmentBuffer(Buffer::ptr buf = nullptr);
    void push_front(Buffer::ptr buf);
    void push_back(Buffer::ptr buf);

    void push_front(SegmentBuffer& buf);
    void push_back(SegmentBuffer& buf);

    Buffer::ptr merge();
    bool empty();
    size_t size();

    const std::list<Buffer::ptr>& segments();

private:

    std::list<Buffer::ptr> m_segments;
};

}
