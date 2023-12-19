#include "ntdcp/channel.hpp"

#include "gtest/gtest.h"

using namespace ntdcp;

TEST(ChannelLayerBinaryClass, SimpleOperating)
{
    const char test_data_1[] = ">Whatever you want here<";
    const char test_data_2[] = ">Anything else<";
    uint8_t tmp[255];
    RingBuffer ring_buffer(200);
    ChannelLayer channel;

    // Single frame test
    SegmentBuffer sg1(Buffer::create(sizeof(test_data_1), test_data_1));
    channel.encode(sg1);

    Buffer::ptr buf1(sg1.merge());
    MemBlock blk1 = buf1->contents();
    ring_buffer.put(blk1, blk1.size());

    std::vector<Buffer::ptr> frames = channel.decode(ring_buffer);

    ASSERT_EQ(frames.size(), 1);
    ASSERT_EQ(frames[0]->size(), sizeof(test_data_1));

    frames[0]->contents().extract(tmp, frames[0]->size());

    ASSERT_EQ(0, memcmp(test_data_1, tmp, frames[0]->size()));

    // 2 frames test
    SegmentBuffer sg2(Buffer::create(sizeof(test_data_2), test_data_2));
    channel.encode(sg2);

    sg1.push_back(sg2);
    Buffer::ptr buf2(sg1.merge());
    MemBlock blk2 = buf2->contents();
    ring_buffer.put(blk2, blk2.size());

    frames = channel.decode(ring_buffer);
    ASSERT_EQ(frames.size(), 2);
    ASSERT_EQ(frames[0]->size(), sizeof(test_data_1));
    frames[0]->contents().extract(tmp, frames[0]->size());

    ASSERT_EQ(0, memcmp(test_data_1, tmp, frames[0]->size()));

    ASSERT_EQ(frames[1]->size(), sizeof(test_data_2));
    frames[1]->contents().extract(tmp, frames[1]->size());

    ASSERT_EQ(0, memcmp(test_data_2, tmp, frames[1]->size()));
}

TEST(ChannelLayerBinaryClass, FalseHeaders)
{
    const char test_data_1[] = ">Whatever you want here<";
    const char test_data_2[] = ">Anything else<";
    uint8_t tmp[255];
    RingBuffer ring_buffer(200);
    ChannelLayer channel;

    ChannelHeader false_header;
    false_header.size = 5;

    // False header
    SegmentBuffer sg1;

    // Real data
    sg1.push_back(Buffer::create(sizeof(test_data_1), test_data_1));
    channel.encode(sg1);

    sg1.push_front(Buffer::create(sizeof(false_header), &false_header));

    // False header
    sg1.push_back(Buffer::create(sizeof(false_header), &false_header));
    // False header
    sg1.push_back(Buffer::create(sizeof(false_header), &false_header));

    // Real data
    SegmentBuffer sg2(Buffer::create(sizeof(test_data_2), test_data_2));
    channel.encode(sg2);

    sg1.push_back(sg2);
    Buffer::ptr merged = sg1.merge();
    MemBlock blk1 = merged->contents();
    ring_buffer.put(blk1, blk1.size());

    auto frames = channel.decode(ring_buffer);
    ASSERT_EQ(frames.size(), 2);
    ASSERT_EQ(frames[0]->size(), sizeof(test_data_1));
    frames[0]->contents().extract(tmp, frames[0]->size());

    ASSERT_EQ(0, memcmp(test_data_1, tmp, frames[0]->size()));

    ASSERT_EQ(frames[1]->size(), sizeof(test_data_2));
    frames[1]->contents().extract(tmp, frames[1]->size());

    ASSERT_EQ(0, memcmp(test_data_2, tmp, frames[1]->size()));
}

TEST(ChannelLayerBinaryClass, Corruption)
{
    const char test_data_1[] = ">Whatever you want here<";
    uint8_t tmp[255];
    RingBuffer ring_buffer(200);
    ChannelLayer channel;

    const char garbage[] = "#THIS IS A GARBAGE#";

    for (size_t i = 0; i < 100; i ++)
    {
        // Single frame test
        SegmentBuffer sg1(Buffer::create(sizeof(test_data_1), test_data_1));
        channel.encode(sg1);

        ring_buffer.put(garbage, sizeof(garbage));

        Buffer::ptr merged = sg1.merge();
        MemBlock blk1 = merged->contents();
        ring_buffer.put(blk1, blk1.size());

        ring_buffer.put(garbage, sizeof(garbage));

        auto frames = channel.decode(ring_buffer);

        ASSERT_EQ(frames.size(), 1);
        ASSERT_EQ(frames[0]->size(), sizeof(test_data_1));
        frames[0]->contents().extract(tmp, frames[0]->size());

        ASSERT_EQ(0, memcmp(test_data_1, tmp, frames[0]->size()));
    }
}
