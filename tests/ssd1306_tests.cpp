#include <cstdint>
#include <gtest/gtest.h>
#include "../drivers/include/ssd1306.h"

/* Buffer side only: a plain array, no fake registers. Expected glyph bytes are
   written out by hand rather than read from the font table. */

/* Starts from non-zero, or a clear that does nothing would pass */
TEST(Ssd1306Clear, SetsAllBytesToZero) {
    uint8_t buf[SSD1306_BUF_SIZE] = {};
    for (int i = 0; i < SSD1306_BUF_SIZE; i++) {
        buf[i] = 0x5;
    }
    ssd1306_clear(buf);
    for (int i = 0; i < SSD1306_BUF_SIZE; i++) {
        EXPECT_EQ(buf[i], 0) << "index " << i;
    }
}

/* The glyph lands at page*128 + col, and nothing else moves */
TEST(Ssd1306DrawChar, DrawingOnlyAffectsCorrectBytes) {
    uint8_t buf[SSD1306_BUF_SIZE] = {};
    ssd1306_draw_char(buf, '1', 0, 1);
    int result = 0;
    for (int i = 0; i < SSD1306_BUF_SIZE; i++) {
        if (i > 133 || i < 128) {
            result += buf[i];
        }
    }
    EXPECT_EQ(result, 0);
    EXPECT_EQ(buf[128], 0x04);
    EXPECT_EQ(buf[129], 0x42);
    EXPECT_EQ(buf[130], 0x7F);
    EXPECT_EQ(buf[131], 0x40);
    EXPECT_EQ(buf[132], 0x00);
    EXPECT_EQ(buf[133], 0x00);
}

TEST(Ssd1306DrawChar, UnknownDrawsQMark){
    uint8_t buf[SSD1306_BUF_SIZE] = {};
    ssd1306_draw_char(buf, 'G', 0, 0);
    EXPECT_EQ(buf[0], 0x02);
    EXPECT_EQ(buf[1], 0x01);
    EXPECT_EQ(buf[2], 0x51);
    EXPECT_EQ(buf[3], 0x09);
    EXPECT_EQ(buf[4], 0x06);
    EXPECT_EQ(buf[5], 0x00);
}

/* Page 8 would start at index 1024, past the buffer. The 6 extra bytes are a
   guard zone that is safe to read, so a missing page check shows up there. */
TEST(Ssd1306DrawChar, Page8WritesNothing) {
    uint8_t buf[SSD1306_BUF_SIZE + 6] = {};
    ssd1306_draw_char(buf, '3', 0, 8);
    for (int i = SSD1306_BUF_SIZE; i < SSD1306_BUF_SIZE + 6; i++) {
        EXPECT_EQ(buf[i], 0) << "index " << i;
    }
}

/* Without the clip, columns 128 to 130 would wrap onto the start of page 1 */
TEST(Ssd1306DrawChar, EndOfPageIsCutOff) {
    uint8_t buf[SSD1306_BUF_SIZE] = {};
    ssd1306_draw_char(buf, '3', 125, 0);
    EXPECT_EQ(buf[125], 0x22);
    EXPECT_EQ(buf[126], 0x41);
    EXPECT_EQ(buf[127], 0x49);
    EXPECT_EQ(buf[128], 0x0);
    EXPECT_EQ(buf[129], 0x0);
    EXPECT_EQ(buf[130], 0x0);
}

/* Off column 0 and page 0, so a string that ignores either lands elsewhere */
TEST(Ssd1306DrawString, DrawsInOrderFromColAndPageThenStops) {
    uint8_t buf[SSD1306_BUF_SIZE] = {};
    ssd1306_draw_string(buf, "12", 1, 1);
    for (int i = 0; i < 129; i++) {
        EXPECT_EQ(buf[i], 0x00) << "index " << i;
    }
    EXPECT_EQ(buf[129], 0x04);
    EXPECT_EQ(buf[130], 0x42);
    EXPECT_EQ(buf[131], 0x7F);
    EXPECT_EQ(buf[132], 0x40);
    EXPECT_EQ(buf[133], 0x00);
    EXPECT_EQ(buf[134], 0x00);
    EXPECT_EQ(buf[135], 0x42);
    EXPECT_EQ(buf[136], 0x61);
    EXPECT_EQ(buf[137], 0x51);
    EXPECT_EQ(buf[138], 0x49);
    EXPECT_EQ(buf[139], 0x46);
    EXPECT_EQ(buf[140], 0x00);
    for (int i = 141; i < SSD1306_BUF_SIZE; i++) {
        EXPECT_EQ(buf[i], 0x00) << "index " << i;
    }
}

TEST(Ssd1306DrawString, StopsAtRightEdge) {
    uint8_t buf[SSD1306_BUF_SIZE] = {};
    /* 44 characters: the 22nd is clipped at 126, and without the edge check the
       44th would wrap to column 2, because col is 8 bits */
    ssd1306_draw_string(buf, "--------------------------------------------", 0, 0);
    for (int i = 0; i < 126; i++) {
        if ((i + 1) % 6 == 0) {   /* every 6th column is the gap after a dash */
            EXPECT_EQ(buf[i], 0x00) << "index " << i;
        } else {
            EXPECT_EQ(buf[i], 0x18) << "index " << i;
        }
    }
    EXPECT_EQ(buf[126], 0x18);
    EXPECT_EQ(buf[127], 0x18);
    for (int i = 128; i < SSD1306_BUF_SIZE; i++) {
        EXPECT_EQ(buf[i], 0x00) << "index " << i;
    }
}

TEST(Ssd1306DrawString, EmptyStringDrawsNothing) {
    uint8_t buf[SSD1306_BUF_SIZE] = {};
    ssd1306_draw_string(buf, "", 0, 0);
    for (int i = 0; i < SSD1306_BUF_SIZE; i++) {
        EXPECT_EQ(buf[i], 0x00) << "index " << i;
    }
}

