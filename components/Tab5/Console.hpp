/*-----------------------------------------------------------------------------
 * File        : Console.hpp
 * Description : Scrolling text console rendered within a bordered region of
 *               the M5GFX display.  New lines are appended at the bottom and
 *               old lines scroll upward when the visible area is full.
 * Author      : Mark Stevens
 * Copyright   : Copyright (c) 2026 Mark Stevens
 * Licence     : MIT — see LICENSE in the repository root for full terms.
 * Target      : M5Stack Tab5 (ESP32-P4)
 * Build system: ESP-IDF v5.5.1
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
    /**
     * @brief Constructs and draws the console on the display.
     *
     * Calculates the maximum number of visible lines from the console height
     * and the Font4 line height, draws the border, fills the interior black,
     * and flushes the framebuffer.
     *
     * @param display       Reference to the global M5GFX display instance.
     * @param x             Left edge of the console in display co-ordinates.
     * @param y             Top edge of the console in display co-ordinates.
     * @param width         Total width of the console including the border.
     * @param height        Total height of the console including the border.
     * @param borderColour  Colour of the one-pixel border (default TFT_WHITE).
     * @param textColour    Colour of the text (default TFT_WHITE).
     */
    Console(M5GFX &display,
            int32_t x,
            int32_t y,
            int32_t width,
            int32_t height,
            uint32_t borderColour = TFT_WHITE,
            uint32_t textColour = TFT_WHITE);

    /**
     * @brief Destructor.
     *
     * Deletes the mutex and releases all resources.
     */
    ~Console();

    /**
     * @brief Appends a line of text to the console.
     *
     * If the console is full the oldest line is removed before the new line is
     * appended.  The interior is then redrawn and the framebuffer is flushed.
     * Thread-safe.
     *
     * @param text  The line of text to display.
     */
    void Println(const std::string &text);

    /**
     * @brief Appends a formatted line of text to the console.
     *
     * Formats the string using printf-style format specifiers and then calls
     * Println().  Thread-safe.
     *
     * @param format  printf-style format string followed by optional arguments.
     */
    void Printf(const char *format, ...);

    /**
     * @brief Removes all lines from the console and redraws the empty interior.
     *
     * Thread-safe.
     */
    void Clear();

private:
    /**
     * @brief Draws a new line onto the display with minimal pixel updates.
     *
     * When scrolled is false the console was not yet full and only the new
     * last line is drawn.  When scrolled is true the existing content is
     * shifted up by one line height using copyRect, the last line area is
     * cleared, and the new last line is drawn.  Must be called with _mutex
     * held.
     *
     * @param scrolled  True if a line was discarded to make room (scroll
     *                  occurred), false if the line was simply appended.
     */
    void Redraw(bool scrolled);

    /** Padding in pixels between the border and the text area on every side. */
    static constexpr int32_t PADDING = 4;

    /** Extra vertical space in pixels added between consecutive text lines. */
    static constexpr int32_t LINE_SPACING = 2;

    /** Height in pixels of the Font4 glyph cell. */
    static constexpr int32_t FONT4_HEIGHT = 26;

    /** Size of the temporary buffer used by Printf(). */
    static constexpr size_t PRINTF_BUFFER_SIZE = 256;

    /** Reference to the display used for all drawing operations. */
    M5GFX &_display;

    /** Left edge of the console in display co-ordinates. */
    const int32_t _x;

    /** Top edge of the console in display co-ordinates. */
    const int32_t _y;

    /** Total width of the console including the border. */
    const int32_t _width;

    /** Total height of the console including the border. */
    const int32_t _height;

    /** Colour of the one-pixel border. */
    const uint32_t _borderColour;

    /** Colour of all rendered text. */
    const uint32_t _textColour;

    /** Height of one text row in pixels (FONT4_HEIGHT + LINE_SPACING). */
    int32_t _lineHeight;

    /** Maximum number of text lines that fit in the console interior. */
    int32_t _maxLines;

    /** The text lines currently stored in the console, oldest at the front. */
    std::deque<std::string> _lines;

    /** Mutex protecting _lines and all drawing operations. */
    SemaphoreHandle_t _mutex;
};
