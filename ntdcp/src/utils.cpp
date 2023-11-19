#include "ntdcp/utils.hpp"

#include <cstring>

using namespace ntdcp;

// ---------------------------
// SerialReadAccessor

bool SerialReadAccessor::empty() const
{
    return size() == 0;
}

void SerialReadAccessor::extract(uint8_t* buf, size_t size)
{
    get(buf, size);
    skip(size);
}

Buffer::ptr SerialReadAccessor::extract_buf(size_t extraction_size)
{
    extraction_size = std::min(extraction_size, size());
    Buffer::ptr result = Buffer::create(extraction_size);
    extract(result->data(), result->size());
    return result;
}

Buffer::ptr SerialReadAccessor::extract_buf()
{
    Buffer::ptr result = Buffer::create(size());
    extract(result->data(), result->size());
    return result;
}

bool SerialWriteAccessor::put(SerialReadAccessor& buffer)
{
    return put(buffer, buffer.size());
}

bool SerialWriteAccessor::put(SerialReadAccessor&& buffer)
{
    return put(buffer);
}


// ---------------------------
// SerialWriteAccessor

SerialWriteAccessor& SerialWriteAccessor::operator<<(SerialReadAccessor&& accessor)
{
    return *this << accessor;
}

SerialWriteAccessor& SerialWriteAccessor::operator<<(SerialReadAccessor& accessor)
{
    put(accessor, accessor.size());
    return *this;
}

SerialWriteAccessor::RawStream SerialWriteAccessor::raw()
{
    return RawStream(*this);
}

// ---------------------------
// Buffer

std::shared_ptr<Buffer> Buffer::create(size_t size, const void* init_data)
{
    return std::shared_ptr<Buffer>(new Buffer(size, init_data));
}

Buffer::ptr Buffer::create(SerialReadAccessor& data, size_t size)
{
    Buffer::ptr result = create();
    result->put(data, size);
    return result;
}

Buffer::ptr Buffer::create(SerialReadAccessor& data)
{
    return create(data, data.size());
}

Buffer::Buffer(size_t size, const void* init_data) :
    m_contents(size)
{
    if (init_data != nullptr)
    {
        memcpy(m_contents.data(), init_data, size);
    } else {
        memset(m_contents.data(), 0, size);
    }
}

void Buffer::extend(size_t size)
{
    m_contents.resize(m_contents.size() + size);
}

Buffer::ptr Buffer::clone() const
{
    Buffer::ptr copy = create(size(), data());
    return copy;
}

size_t Buffer::size() const
{
    return m_contents.size();
}

uint8_t* Buffer::data()
{
    return m_contents.data();
}

const uint8_t* Buffer::data() const
{
    return m_contents.data();
}

void Buffer::clear()
{
    m_contents.clear();
}

uint8_t& Buffer::at(size_t pos)
{
    return m_contents[pos];
}

uint8_t& Buffer::operator[](size_t pos)
{
    return at(pos);
}

bool Buffer::operator==(const Buffer& right) const
{
    return m_contents == right.m_contents;
}

MemBlock Buffer::contents() const
{
    return MemBlock(m_contents.data(), m_contents.size());
}

bool Buffer::put(const void* data, size_t size)
{
    if (size == 0)
        return true;

    size_t old_size = m_contents.size();
    extend(size);

    if (data != nullptr)
    {
        memcpy(m_contents.data() + old_size, data, size);
    } else {
        memset(m_contents.data() + old_size, 0x00, size);
    }
    return true;
}

bool Buffer::put(SerialReadAccessor& accessor, size_t size)
{
    if (size == 0)
        return true;

    size_t buffer_size = accessor.size();
    if (buffer_size < size)
        size = buffer_size;

    size_t old_size = m_contents.size();
    extend(size);
    accessor.extract(m_contents.data() + old_size, size);
    return true;
}

bool Buffer::will_fit(size_t)
{
    return true;
}


// ---------------------------
// RingBuffer

RingBuffer::RingBuffer(size_t capacity) :
    m_contents(capacity + 1)
{
}

size_t RingBuffer::free_space()
{
    if (m_p_read <= m_p_write)
    {
        return m_contents.size() + m_p_read - m_p_write - 1;
    } else {
        return m_p_read - m_p_write - 1;
    }
}

size_t RingBuffer::size() const
{
    if (m_p_read <= m_p_write)
    {
        return m_p_write - m_p_read;
    } else {
        return m_contents.size() + m_p_write - m_p_read;
    }
}

bool RingBuffer::put(const void* src, size_t size)
{
    if (!will_fit(size))
        return false;

    const uint8_t* buf = (const uint8_t*) src;
    size_t free_tail = m_contents.size() - m_p_write;
    if (size < free_tail)
    {
        // Add to the end
        memcpy(&m_contents[m_p_write], buf, size);
        m_p_write += size;
    } else {
        // Part add to the end and part add to the beginning
        memcpy(&m_contents[m_p_write], buf, free_tail);
        uint32_t second_part_size = size - free_tail;
        memcpy(&m_contents[0], &buf[free_tail], second_part_size);
        m_p_write = second_part_size;
    }
    return true;
}

bool RingBuffer::put(Buffer::ptr buf)
{
    return put(buf->data(), buf->size());
}

bool RingBuffer::put(SerialReadAccessor& accessor, size_t size)
{
    if (!will_fit(size))
        return false;

    size_t free_tail = m_contents.size() - m_p_write;
    if (size < free_tail)
    {
        // Add to the end
        accessor.extract(&m_contents[m_p_write], size);
        m_p_write += size;
    } else {
        // Part add to the end and part add to the beginning
        accessor.extract(&m_contents[m_p_write], free_tail);
        uint32_t second_part_size = size - free_tail;
        accessor.extract(&m_contents[0], second_part_size);
        m_p_write = second_part_size;
    }

    return true;
}

bool RingBuffer::will_fit(size_t size)
{
    return free_space() >= size;
}

void RingBuffer::get(uint8_t* buf, size_t size) const
{
    uint32_t p_read = m_p_read;
    if (m_p_write >= p_read)
    {
        memcpy(buf, &m_contents[p_read], size);
        p_read += size;
    } else {
        uint32_t tail = m_contents.size() - p_read;
        if (tail > size)
        {
            memcpy(buf, &m_contents[p_read], size);
            p_read += size;
        } else {
            memcpy(buf, &m_contents[p_read], tail);
            memcpy(buf+tail, &m_contents[0], size - tail);
        }
    }
}

void RingBuffer::extract(uint8_t* buf, size_t size)
{
    if (m_p_write >= m_p_read)
    {
        memcpy(buf, &m_contents[m_p_read], size);
        m_p_read += size;
    } else {
        uint32_t tail = m_contents.size() - m_p_read;
        if (tail > size)
        {
            memcpy(buf, &m_contents[m_p_read], size);
            m_p_read += size;
        } else {
            memcpy(buf, &m_contents[m_p_read], tail);
            memcpy(buf+tail, &m_contents[0], size - tail);
            m_p_read = size - tail;
        }
    }
}

void RingBuffer::skip(size_t size)
{
    m_p_read += size;
    if (m_p_read >= m_contents.size())
        m_p_read -= m_contents.size();
}

MemBlock RingBuffer::get_continious_block(size_t size) const
{
    const uint8_t* begin = nullptr;
    size_t block_size = 0;
    if (m_p_read <= m_p_write)
    {
        block_size = std::min(uint32_t(size), m_p_write - m_p_read);
    } else {
        block_size = std::min(uint32_t(size), uint32_t(m_contents.size()) - m_p_read);
    }
    return MemBlock(&m_contents[m_p_read], block_size);
}

bool RingBuffer::empty() const
{
    return m_p_read == m_p_write;
}

void RingBuffer::clear()
{
    m_p_write = m_p_read = 0;
}

uint8_t RingBuffer::operator[](size_t pos) const
{
    return const_cast<RingBuffer*>(this)->operator[](pos);
}

uint8_t& RingBuffer::operator[](size_t pos)
{
    size_t target_pos = pos + m_p_read;
    if (target_pos >= m_contents.size())
    {
        target_pos -= m_contents.size();
    }
    return m_contents[target_pos];
}

// ---------------------------
// SegmentBuffer

SegmentBuffer::SegmentBuffer(Buffer::ptr buf)
{
    if (buf)
        push_back(buf);
}

void SegmentBuffer::push_front(Buffer::ptr buf)
{
    m_segments.push_front(buf);
}

void SegmentBuffer::push_back(Buffer::ptr buf)
{
    m_segments.push_back(buf);
}

void SegmentBuffer::push_front(SegmentBuffer& buf)
{
    for (auto it = buf.m_segments.rbegin(); it != buf.m_segments.rend(); --it)
    {
        push_front(*it);
    }
}

void SegmentBuffer::push_back(SegmentBuffer& buf)
{
    for (auto it = buf.m_segments.begin(); it != buf.m_segments.end(); ++it)
    {
        push_back(*it);
    }
}

Buffer::ptr SegmentBuffer::merge()
{
    for (auto it = std::next(m_segments.begin()); it != m_segments.end(); it = m_segments.erase(it))
    {
        m_segments.front()->put((*it)->data(), (*it)->size());
    }
    return m_segments.front();
}

bool SegmentBuffer::empty()
{
    return m_segments.empty();
}

const std::list<Buffer::ptr>& SegmentBuffer::segments()
{
    return m_segments;
}

size_t SegmentBuffer::size()
{
    size_t total_size = 0;
    for (const auto& segment : m_segments)
    {
        total_size += segment->size();
    }
    return total_size;
}



// ---------------------------
// MemBlock

MemBlock::MemBlock(const MemBlock& mem_block, size_t offset)
{
    *this = mem_block.offset(offset);
}

MemBlock::MemBlock(const uint8_t* begin, size_t size) :
    m_begin(begin), m_end(m_begin + size)
{ }

MemBlock MemBlock::offset(size_t off) const
{
    MemBlock result = *this;
    result.offset(off);
    return result;
}

void MemBlock::skip(size_t count)
{
    m_begin += count;
    if (m_begin > m_end)
        m_begin = m_end;
}

void MemBlock::get(uint8_t* buf, size_t buf_size) const
{
    memcpy(buf, m_begin, std::min(buf_size, size()));
}

size_t MemBlock::size() const
{
    return m_end - m_begin;
}


uint8_t MemBlock::operator[](size_t pos) const
{
    return m_begin[pos];
}

const uint8_t* MemBlock::begin() const
{
    return m_begin;
}

const uint8_t* MemBlock::end() const
{
    return m_end;
}

bool MemBlock::operator==(const MemBlock& right) const
{
    if (size() != right.size())
        return false;

    if (m_begin == right.m_begin)
        return true;

    return memcmp(m_begin, right.m_begin, size()) == 0;
}


// ---------------------------
// BitExtractor

BitReader::BitReader(const uint8_t* buffer, size_t max_size) :
    m_buffer(buffer), m_bytes_left(max_size)
{
}

uint8_t BitReader::get(int bits_count)
{
    uint8_t result = 0;
    if (bits_count >= m_bits_left)
    {
        m_bits_left -= bits_count;
        result = *m_buffer & ((1 << bits_count) - 1);
    }
    /// @todo Here
    return result;
}



// ///////////////////////////////////
// Buffer class
/*
std::shared_ptr<Buffer> Buffer::create(size_t size, const void* init_data)
{
    return std::shared_ptr<Buffer>(new Buffer(size, init_data));
}


Buffer::Buffer(size_t size, const void* init_data) :
    m_contents(size)
{
    if (init_data != nullptr)
    {
        memcpy(m_contents.data(), init_data, size);
    } else {
        memset(m_contents.data(), 0, size);
    }
}

void Buffer::extend(size_t size)
{
    m_contents.resize(m_contents.size() + size);
}

Buffer::ptr Buffer::clone() const
{
    Buffer::ptr copy = create(size(), data());
    return copy;
}

size_t Buffer::size() const
{
    return m_contents.size();
}

uint8_t* Buffer::data()
{
    return m_contents.data();
}

const uint8_t* Buffer::data() const
{
    return m_contents.data();
}

void Buffer::clear()
{
    m_contents.clear();
}

uint8_t& Buffer::at(size_t pos)
{
    return m_contents[pos];
}

uint8_t& Buffer::operator[](size_t pos)
{
    return at(pos);
}

bool Buffer::operator==(const Buffer& right) const
{
    return m_contents == right.m_contents;
}

std::vector<uint8_t>& Buffer::contents()
{
    return m_contents;
}

bool Buffer::put(const void* data, size_t size)
{
    if (size == 0)
        return true;

    size_t old_size = m_contents.size();
    extend(size);

    if (data != nullptr)
    {
        memcpy(m_contents.data() + old_size, data, size);
    } else {
        memset(m_contents.data() + old_size, 0x00, size);
    }
    return true;
}

bool Buffer::will_fit(size_t)
{
    return true;
}


// ///////////////////////////////////
// RingBuffer class
RingBuffer::RingBuffer(size_t capacity) :
    m_contents(capacity + 1)
{
}

size_t RingBuffer::free_space()
{
    if (m_p_read <= m_p_write)
    {
        return m_contents.size() + m_p_read - m_p_write - 1;
    } else {
        return m_p_read - m_p_write - 1;
    }
}

size_t RingBuffer::size() const
{
    if (m_p_read <= m_p_write)
    {
        return m_p_write - m_p_read;
    } else {
        return m_contents.size() + m_p_write - m_p_read;
    }
}

bool RingBuffer::put(const void* src, size_t size)
{
    if (!will_fit(size))
        return false;

    const uint8_t* buf = (const uint8_t*) src;
    size_t free_tail = m_contents.size() - m_p_write;
    if (size < free_tail)
    {
        // Add to the end
        memcpy(&m_contents[m_p_write], buf, size);
        m_p_write += size;
    } else {
        // Part add to the end and part add to the beginning
        memcpy(&m_contents[m_p_write], buf, free_tail);
        uint32_t second_part_size = size - free_tail;
        memcpy(&m_contents[0], &buf[free_tail], second_part_size);
        m_p_write = second_part_size;
    }
    return true;
}

bool RingBuffer::put(Buffer::ptr buf)
{
    return put(buf->data(), buf->size());
}

bool RingBuffer::put(SerialReadAccessor& accessor, size_t size)
{
    if (!will_fit(size))
        return false;

    size_t free_tail = m_contents.size() - m_p_write;
    if (size < free_tail)
    {
        // Add to the end
        accessor.extract(&m_contents[m_p_write], size);
        m_p_write += size;
    } else {
        // Part add to the end and part add to the beginning
        accessor.extract(&m_contents[m_p_write], free_tail);
        uint32_t second_part_size = size - free_tail;
        accessor.extract(&m_contents[0], second_part_size);
        m_p_write = second_part_size;
    }

    return true;
}

bool RingBuffer::will_fit(size_t size)
{
    return free_space() >= size;
}

void RingBuffer::get(uint8_t* buf, size_t size) const
{
    uint32_t p_read = m_p_read;
    if (m_p_write >= p_read)
    {
        memcpy(buf, &m_contents[p_read], size);
        p_read += size;
    } else {
        uint32_t tail = m_contents.size() - p_read;
        if (tail > size)
        {
            memcpy(buf, &m_contents[p_read], size);
            p_read += size;
        } else {
            memcpy(buf, &m_contents[p_read], tail);
            memcpy(buf+tail, &m_contents[0], size - tail);
        }
    }
}

void RingBuffer::extract(uint8_t* buf, size_t size)
{
    if (m_p_write >= m_p_read)
    {
        memcpy(buf, &m_contents[m_p_read], size);
        m_p_read += size;
    } else {
        uint32_t tail = m_contents.size() - m_p_read;
        if (tail > size)
        {
            memcpy(buf, &m_contents[m_p_read], size);
            m_p_read += size;
        } else {
            memcpy(buf, &m_contents[m_p_read], tail);
            memcpy(buf+tail, &m_contents[0], size - tail);
            m_p_read = size - tail;
        }
    }
}

void RingBuffer::skip(size_t size)
{
    m_p_read += size;
    if (m_p_read >= m_contents.size())
        m_p_read -= m_contents.size();
}

MemBlock RingBuffer::get_continious_block(size_t size) const
{
    const uint8_t* begin = nullptr;
    size_t block_size = 0;
    if (m_p_read <= m_p_write)
    {
        block_size = std::min(uint32_t(size), m_p_write - m_p_read);
    } else {
        block_size = std::min(uint32_t(size), uint32_t(m_contents.size()) - m_p_read);
    }
    return MemBlock(&m_contents[m_p_read], block_size);
}

bool RingBuffer::empty() const
{
    return m_p_read == m_p_write;
}

void RingBuffer::clear()
{
    m_p_write = m_p_read = 0;
}

uint8_t RingBuffer::operator[](size_t pos) const
{
    return const_cast<RingBuffer*>(this)->operator[](pos);
}

uint8_t& RingBuffer::operator[](size_t pos)
{
    size_t target_pos = pos + m_p_read;
    if (target_pos >= m_contents.size())
    {
        target_pos -= m_contents.size();
    }
    return m_contents[target_pos];
}
*/

uint32_t ntdcp::hash_Ly(uint8_t next_byte, uint32_t prev_hash)
{
    return (prev_hash * 1664525) + next_byte + 1013904223;
}

uint32_t hash_Ly(const void * buf, uint32_t size, uint32_t hash)
{
    for(uint32_t i=0; i<size; i++)
        hash = ntdcp::hash_Ly(reinterpret_cast<const uint8_t*>(buf)[i], hash);

    return hash;
}

