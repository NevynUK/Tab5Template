/*-----------------------------------------------------------------------------
 * File        : Console.hpp
 * Description : Scrolling text console rendered within a bordered region of
 *               the M5GFX display.  New lines are appended at the bottom and
 *               old lines scroll upward when the visible area is full.
 * Author      : Mark Stevens
 * Copyright   : Copyright (c) 2026 Mark Stevens
 * Licence     : MIT — see LICENSE in the repository root for full terms.
 * Target      : M5Stack Tab5 (ESP32-P4)
 *---------------------------------------------------------------------------*/

#pragma once

#include <M5GFX.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <deque>
#include <string>

/**
 * @brief Scrolling text console rendered on the M5GFX display.
 *
 * Draws a one-pixel border and a black interior at the specified position and
 * size.  Text lines are appended via Println() or Printf(); when the interior
 * is full the oldest line is discarded and the remaining lines scroll up so
 * that the newest line always appears at the bottom.
 *
 * Drawing is done directly on the display using setClipRect() to prevent
 * overflow.  To avoid flicker, only the minimum set of pixels is updated on
 * each call:
 *   - Append: only the new line is drawn (one drawString).
 *   - Scroll: copyRect shifts existing content up by one line height, then
 *     only the last line is cleared and redrawn.
 *   - Clear: a single fillRect covering the interior.
 *
 * All display properties (position, size, border colour, text colour) are
 * fixed at construction time and cannot be changed afterwards.
 *
 * Println() and Printf() are thread-safe and may be called from any FreeRTOS
 * task.
 */
class Console
{
public:
    Console(M5GFX &display, int32_t x, int32_t y, int32_t width, int32_t height, uint32_t borderColour = TFT_WHITE, uint32_t textColour = TFT_WHITE);
    ~Console();

    void Println(const std::string &text);
    void Printf(const char *format, ...);
    void Clear();

private:
    void Redraw(bool scrolled);

    /**
     * @brief Padding in pixels between the border and the text area on every side.
     */
    static constexpr int32_t PADDING = 4;

    /**
     * @brief Extra vertical space in pixels added between consecutive text lines.
     */
    static constexpr int32_t LINE_SPACING = 2;

    /**
     * @brief Height in pixels of the Font4 glyph cell.
     */
    static constexpr int32_t FONT4_HEIGHT = 26;

    /**
     * @brief Size of the temporary buffer used by Printf().
     */
    static constexpr size_t PRINTF_BUFFER_SIZE = 256;

    /**
     * @brief Reference to the display used for all drawing operations.
     */
    M5GFX &_display;

    /**
     * @brief Left edge of the console in display co-ordinates.
     */
    const int32_t _x;

    /**
     * @brief Top edge of the console in display co-ordinates.
     */
    const int32_t _y;

    /**
     * @brief Total width of the console including the border.
     */
    const int32_t _width;

    /**
     * @brief Total height of the console including the border.
     */
    const int32_t _height;

    /**
     * @brief Colour of the one-pixel border.
     */
    const uint32_t _borderColour;

    /**
     * @brief Colour of all rendered text.
     */
    const uint32_t _textColour;

    /**
     * @brief Height of one text row in pixels (FONT4_HEIGHT + LINE_SPACING).
     */
    int32_t _lineHeight;

    /**
     * @brief Maximum number of text lines that fit in the console interior.
     */
    int32_t _maxLines;

    /**
     * @brief The text lines currently stored in the console, oldest at the front.
     */
    std::deque<std::string> _lines;

    /**
     * @brief Mutex protecting _lines and all drawing operations.
     */
    SemaphoreHandle_t _mutex;
};
