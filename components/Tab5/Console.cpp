/*-----------------------------------------------------------------------------
 * File        : Console.cpp
 * Description : Implementation of the Console scrolling text display.
 *               Manages line storage, scroll behaviour, and M5GFX rendering
 *               for a bordered console region on the M5Stack Tab5 display.
 * Author      : Mark Stevens
 * Copyright   : Copyright (c) 2026 Mark Stevens
 * Licence     : MIT — see LICENSE in the repository root for full terms.
 * Target      : M5Stack Tab5 (ESP32-P4)
 * Build system: ESP-IDF v5.5.1
 *---------------------------------------------------------------------------*/

#include "Console.hpp"

#include <cstdarg>
#include <cstdio>

Console::Console(M5GFX &display, int32_t x, int32_t y, int32_t width, int32_t height, uint32_t borderColour, uint32_t textColour) : _display(display), _x(x), _y(y), _width(width), _height(height), _borderColour(borderColour), _textColour(textColour), _mutex(nullptr)
{
    _mutex = xSemaphoreCreateMutex();

    _lineHeight = FONT4_HEIGHT + LINE_SPACING;
    _maxLines = (_height - 2 - (2 * PADDING)) / _lineHeight;

    _display.startWrite();
    _display.drawRect(_x, _y, _width, _height, _borderColour);
    _display.fillRect(_x + 1, _y + 1, _width - 2, _height - 2, TFT_BLACK);
    _display.endWrite();
    _display.display();
}

Console::~Console()
{
    if (_mutex != nullptr)
    {
        vSemaphoreDelete(_mutex);
        _mutex = nullptr;
    }
}

void Console::Println(const std::string &text)
{
    xSemaphoreTake(_mutex, portMAX_DELAY);

    const bool scrolled = (static_cast<int32_t>(_lines.size()) >= _maxLines);

    if (scrolled)
    {
        _lines.pop_front();
    }

    _lines.push_back(text);
    Redraw(scrolled);

    xSemaphoreGive(_mutex);
}

void Console::Printf(const char *format, ...)
{
    char buffer[PRINTF_BUFFER_SIZE];

    va_list arguments;
    va_start(arguments, format);
    vsnprintf(buffer, sizeof(buffer), format, arguments);
    va_end(arguments);

    Println(std::string(buffer));
}

void Console::Clear()
{
    xSemaphoreTake(_mutex, portMAX_DELAY);

    _lines.clear();

    _display.startWrite();
    _display.fillRect(_x + 1, _y + 1, _width - 2, _height - 2, TFT_BLACK);
    _display.endWrite();
    _display.display();

    xSemaphoreGive(_mutex);
}

void Console::Redraw(bool scrolled)
{
    const int32_t interiorX = _x + 1;
    const int32_t interiorY = _y + 1;
    const int32_t interiorWidth = _width - 2;
    const int32_t interiorHeight = _height - 2;
    const int32_t textX = interiorX + PADDING;
    const int32_t lineCount = static_cast<int32_t>(_lines.size());
    const int32_t lastLineY = interiorY + PADDING + (lineCount - 1) * _lineHeight;

    _display.startWrite();
    _display.setClipRect(interiorX, interiorY, interiorWidth, interiorHeight);
    _display.setFont(&fonts::Font4);
    _display.setTextColor(_textColour, TFT_BLACK);
    _display.setTextDatum(textdatum_t::top_left);

    if (scrolled)
    {
        // Shift all existing lines up by one line height in the framebuffer,
        // then only clear and draw the new bottom line.
        const int32_t shiftHeight = (lineCount - 1) * _lineHeight;
        _display.copyRect(interiorX, interiorY + PADDING, interiorWidth, shiftHeight, interiorX, interiorY + PADDING + _lineHeight);
        _display.fillRect(interiorX, lastLineY, interiorWidth, _lineHeight, TFT_BLACK);
    }

    _display.drawString(_lines.back().c_str(), textX, lastLineY);

    _display.clearClipRect();
    _display.endWrite();
    _display.display();
}
