#include <cstring>
#include <gtest/gtest.h>
#include "../drivers/include/ringbuffer.h"

/* The buffer wastes one slot so that head == tail can mean empty and nothing
   else, which is what keeps the single-producer/single-consumer case lock
   free. That costs one byte of capacity, so a RBSIZE of 64 holds 63. Several
   tests below assert that number directly: if anyone ever reclaims the spare
   slot, those are the tests that should fail. */
static constexpr int kCapacity = RBSIZE - 1;

static RingBuffer blank_buffer(void) {
    RingBuffer rb;
    std::memset(&rb, 0, sizeof(rb));
    return rb;
}

/* --- 7. a zeroed buffer is already valid ------------------------------- */

TEST(RingBufferInit, ZeroedBufferIsEmptyAndNotFull) {
    // There is deliberately no rb_init(). This test is what documents that.
    RingBuffer rb = blank_buffer();
    EXPECT_TRUE(rb_is_empty(&rb));
    EXPECT_FALSE(rb_is_full(&rb));
}

/* --- 1. the basic round trip -------------------------------------------- */

TEST(RingBufferRoundTrip, PutOneGetOneReturnsTheSameByte) {
    RingBuffer rb = blank_buffer();
    uint8_t out = 0;

    EXPECT_TRUE(rb_put(&rb, 0xA5));
    EXPECT_FALSE(rb_is_empty(&rb));
    EXPECT_TRUE(rb_get(&rb, &out));
    EXPECT_EQ(out, 0xA5);
    EXPECT_TRUE(rb_is_empty(&rb));
}

TEST(RingBufferRoundTrip, ZeroIsOrdinaryData) {
    /* Every uint8_t value is valid payload, which is exactly why rb_get
       reports emptiness through its return value instead of a sentinel. A
       stored 0x00 must come back as a successful get, not look like "empty". */
    RingBuffer rb = blank_buffer();
    uint8_t out = 0xFF;

    EXPECT_TRUE(rb_put(&rb, 0x00));
    EXPECT_TRUE(rb_get(&rb, &out));
    EXPECT_EQ(out, 0x00);
}

/* --- 2. filling completely ---------------------------------------------- */

TEST(RingBufferFull, AcceptsExactlyCapacityBytesThenRejects) {
    RingBuffer rb = blank_buffer();

    for (int i = 0; i < kCapacity; i++) {
        EXPECT_TRUE(rb_put(&rb, static_cast<uint8_t>(i))) << "put " << i << " should fit";
    }
    EXPECT_TRUE(rb_is_full(&rb));
    EXPECT_FALSE(rb_put(&rb, 0xFF)) << "put " << kCapacity << " should be rejected";
}

/* --- 3. emptying completely --------------------------------------------- */

TEST(RingBufferEmpty, YieldsExactlyWhatWasPutThenRejects) {
    RingBuffer rb = blank_buffer();
    uint8_t out = 0;

    for (int i = 0; i < kCapacity; i++) {
        ASSERT_TRUE(rb_put(&rb, static_cast<uint8_t>(i)));
    }
    for (int i = 0; i < kCapacity; i++) {
        EXPECT_TRUE(rb_get(&rb, &out)) << "get " << i << " should succeed";
        EXPECT_EQ(out, static_cast<uint8_t>(i));
    }
    EXPECT_TRUE(rb_is_empty(&rb));
    EXPECT_FALSE(rb_get(&rb, &out)) << "get " << kCapacity << " should be rejected";
}

/* --- 5. the empty/full distinction at head == tail ----------------------- */

TEST(RingBufferAmbiguity, SameIndexStateMeansEmptyNotFull) {
    /* head == tail is reachable two ways: at rest, and after a fill/drain
       cycle. Both must report empty. The full state is deliberately one slot
       short of head == tail, which is the entire reason for the spare slot. */
    RingBuffer rb = blank_buffer();
    uint8_t out = 0;

    ASSERT_EQ(rb.head, rb.tail);
    EXPECT_TRUE(rb_is_empty(&rb));
    EXPECT_FALSE(rb_is_full(&rb));

    for (int i = 0; i < kCapacity; i++) {
        ASSERT_TRUE(rb_put(&rb, static_cast<uint8_t>(i)));
    }
    EXPECT_TRUE(rb_is_full(&rb));
    EXPECT_FALSE(rb_is_empty(&rb));
    EXPECT_NE(rb.head, rb.tail) << "a full buffer must not look like an empty one";

    for (int i = 0; i < kCapacity; i++) {
        ASSERT_TRUE(rb_get(&rb, &out));
    }
    EXPECT_EQ(rb.head, rb.tail);
    EXPECT_TRUE(rb_is_empty(&rb));
    EXPECT_FALSE(rb_is_full(&rb));
}

/* --- 4. wrap-around ------------------------------------------------------ */

TEST(RingBufferWrap, SecondFillWorksWithIndicesPastTheEnd) {
    /* The first fill runs from index 0 and never crosses the end of the array,
       so it cannot exercise the & (RBSIZE - 1) masks. The second one starts
       mid-array and wraps, which is where an unmasked index reads out of
       bounds. */
    RingBuffer rb = blank_buffer();
    uint8_t out = 0;

    for (int i = 0; i < kCapacity; i++) {
        ASSERT_TRUE(rb_put(&rb, static_cast<uint8_t>(i)));
    }
    for (int i = 0; i < kCapacity; i++) {
        ASSERT_TRUE(rb_get(&rb, &out));
    }

    ASSERT_NE(rb.head, 0U) << "indices should not be back at the start";

    for (int i = 0; i < kCapacity; i++) {
        EXPECT_TRUE(rb_put(&rb, static_cast<uint8_t>(100 + i)));
    }
    EXPECT_TRUE(rb_is_full(&rb));
    for (int i = 0; i < kCapacity; i++) {
        EXPECT_TRUE(rb_get(&rb, &out));
        EXPECT_EQ(out, static_cast<uint8_t>(100 + i)) << "order broke at " << i;
    }
    EXPECT_TRUE(rb_is_empty(&rb));
}

TEST(RingBufferWrap, IndicesStayInsideTheArray) {
    // An unmasked advance would let head run to 255 and index past arr[63].
    RingBuffer rb = blank_buffer();
    uint8_t out = 0;

    for (int cycle = 0; cycle < 10; cycle++) {
        for (int i = 0; i < kCapacity; i++) {
            ASSERT_TRUE(rb_put(&rb, static_cast<uint8_t>(i)));
            ASSERT_LT(rb.head, static_cast<uint32_t>(RBSIZE));
            ASSERT_LT(rb.tail, static_cast<uint32_t>(RBSIZE));
        }
        for (int i = 0; i < kCapacity; i++) {
            ASSERT_TRUE(rb_get(&rb, &out));
            ASSERT_LT(rb.head, static_cast<uint32_t>(RBSIZE));
            ASSERT_LT(rb.tail, static_cast<uint32_t>(RBSIZE));
        }
    }
}

/* --- 6. interleaved traffic --------------------------------------------- */

TEST(RingBufferInterleaved, FifoOrderHoldsOverManyWraps) {
    /* One in, one out, far more times than the array is long. This is the
       closest thing here to how the UART will actually use it, and it wraps
       the indices repeatedly. */
    RingBuffer rb = blank_buffer();
    uint8_t out = 0;

    for (int i = 0; i < 1000; i++) {
        ASSERT_TRUE(rb_put(&rb, static_cast<uint8_t>(i)));
        ASSERT_TRUE(rb_get(&rb, &out));
        ASSERT_EQ(out, static_cast<uint8_t>(i)) << "order broke at iteration " << i;
    }
    EXPECT_TRUE(rb_is_empty(&rb));
}

TEST(RingBufferInterleaved, PartialDrainsKeepOrder) {
    /* Puts outrun gets, so the occupancy drifts upward and the two indices
       wrap at different moments rather than in lockstep. */
    RingBuffer rb = blank_buffer();
    uint8_t out = 0;
    int next_expected = 0;
    int produced = 0;

    for (int round = 0; round < 200; round++) {
        for (int i = 0; i < 3; i++) {
            if (rb_put(&rb, static_cast<uint8_t>(produced))) {
                produced++;
            }
        }
        for (int i = 0; i < 2; i++) {
            if (rb_get(&rb, &out)) {
                ASSERT_EQ(out, static_cast<uint8_t>(next_expected)) << "order broke in round " << round;
                next_expected++;
            }
        }
    }
    while (rb_get(&rb, &out)) {
        ASSERT_EQ(out, static_cast<uint8_t>(next_expected));
        next_expected++;
    }
    EXPECT_EQ(next_expected, produced) << "every byte accepted should come back out";
}

/* --- 8 and 9. the failure paths ----------------------------------------- */

TEST(RingBufferRejection, FailedGetLeavesTheOutputUntouched) {
    /* Catches a refactor that assigns through out before checking emptiness.
       A caller that ignores the return value would then read a stale slot. */
    RingBuffer rb = blank_buffer();
    uint8_t out = 0x5C;

    ASSERT_TRUE(rb_is_empty(&rb));
    EXPECT_FALSE(rb_get(&rb, &out));
    EXPECT_EQ(out, 0x5C) << "rb_get wrote through out despite returning false";
}

TEST(RingBufferRejection, FailedPutDoesNotCorruptStoredData) {
    /* A rejected put must be a no-op: it must not overwrite the oldest byte,
       and it must not move an index. */
    RingBuffer rb = blank_buffer();
    uint8_t out = 0;

    for (int i = 0; i < kCapacity; i++) {
        ASSERT_TRUE(rb_put(&rb, static_cast<uint8_t>(i)));
    }
    const uint8_t head_before = rb.head;
    const uint8_t tail_before = rb.tail;

    EXPECT_FALSE(rb_put(&rb, 0xEE));
    EXPECT_EQ(rb.head, head_before) << "a rejected put moved head";
    EXPECT_EQ(rb.tail, tail_before) << "a rejected put moved tail";

    for (int i = 0; i < kCapacity; i++) {
        ASSERT_TRUE(rb_get(&rb, &out));
        EXPECT_EQ(out, static_cast<uint8_t>(i)) << "stored data changed at " << i;
    }
    EXPECT_TRUE(rb_is_empty(&rb));
}

/* --- independence of instances ------------------------------------------ */

TEST(RingBufferInstances, TwoBuffersDoNotShareState) {
    /* The UART needs separate TX and RX buffers, which only works because
       every function takes a RingBuffer * and the struct holds all the state. */
    RingBuffer tx = blank_buffer();
    RingBuffer rx = blank_buffer();
    uint8_t out = 0;

    ASSERT_TRUE(rb_put(&tx, 0x11));
    EXPECT_TRUE(rb_is_empty(&rx)) << "writing tx affected rx";

    ASSERT_TRUE(rb_put(&rx, 0x22));
    ASSERT_TRUE(rb_get(&tx, &out));
    EXPECT_EQ(out, 0x11);
    ASSERT_TRUE(rb_get(&rx, &out));
    EXPECT_EQ(out, 0x22);
}
