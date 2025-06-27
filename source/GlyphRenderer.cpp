/*
Copyright (c) 2021-2025 Nicolas Beddows <nicolas.beddows@gmail.com>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include "i8080_arcade/GlyphRenderer.h"

namespace meen_i8080_arcade
{
    void GlyphRenderer::Configure()
    {
        size_t found = 0;
        size_t lastPos = -1;

        // clear all values;
        maxTxtWidth_ = 0;
        maxTxtHeight_ = 0;
        txtHeight_.clear();

        do
        {
            found = text_.find('\n', ++lastPos);
            auto currPos = found == std::string::npos ? text_.length() : found;
            auto txtHeight = (currPos - lastPos) * 8; // 8 - store all heights in uncompressed bytes

            // skip consecutive new lines
            while (currPos + 1 < text_.length() && text_[currPos + 1] == '\n')
            {
                ++currPos;
                ++maxTxtWidth_;
            }

            if (txtHeight > maxTxtHeight_)
            {
                maxTxtHeight_ = txtHeight;
            }

            txtHeight_.push_back(txtHeight);
            lastPos = currPos;
            ++maxTxtWidth_;
        } while (found != std::string::npos);
    }

    GlyphRenderer::GlyphRenderer(int rowBytes)
        : rowBytes_{ rowBytes }
    {
    }

    std::error_code GlyphRenderer::SetFont(GlyphRenderer::Font font)
    {
        switch (font)
        {
            case GlyphRenderer::Font::I8080ArcadeRegular8x8:
            {
                font_ = std::span(i8080ArcadeRegular8x8_);
                asciiBase_ = static_cast<uint8_t>('/');
                break;
            }
            default:
            {
                return std::make_error_code(std::errc::not_supported);
            }
        }

        return std::error_code{};
    }

    std::error_code GlyphRenderer::SetJustification (GlyphRenderer::Justification justification)
    {
        if(justification == GlyphRenderer::Justification::Right)
        {
            return std::make_error_code(std::errc::not_supported);
        }

        justification_ = justification;
        return std::error_code{};
    }

    std::error_code GlyphRenderer::SetAnchorPoint(int anchorPoint)
    {
        if (anchorPoint < 0)
        {
            return std::make_error_code(std::errc::invalid_argument);
        }

        anchorPoint_ = anchorPoint;
        return std::error_code{};
    }

    void GlyphRenderer::SetText(std::string_view text)
    {
        if (text.empty() == false)
        {
            text_ = text;
            Configure();
        }
    }

    void GlyphRenderer::SetText(std::string&& text)
    {
        if (text.empty() == false)
        {
            text_ = std::move(text);
            Configure();
        }
    }

    int GlyphRenderer::GetWidth() const
    {
        return maxTxtWidth_;
    }

    int GlyphRenderer::GetHeight() const
    {
        return maxTxtHeight_;
    }

    std::error_code GlyphRenderer::Update(std::string_view text, int position)
    {
        if (position < 0 || position + text.length() > text_.length())
        {
            return std::make_error_code(std::errc::invalid_argument);
        }

        std::error_code err{};

        // Do nothing if we are empty
        if (text.empty() == false)
        {
            for (auto c : text)
            {
                if (c == '\n' && text_[position] != '\n')
                {
                    err = std::make_error_code(std::errc::interrupted);
                }

                text_[position++] = c;
            }

            if (err.value() == static_cast<int>(std::errc::interrupted))
            {
                Configure();
            }
        }

        return err;
    }

    std::error_code GlyphRenderer::Blit(std::vector<uint8_t>::iterator start, std::vector<uint8_t>::iterator end, int invertLineCount, int invertLineStart) const
    {
        if (std::distance(start, end) < anchorPoint_ || rowBytes_ < 0 ||
            (invertLineStart < 0 || invertLineStart >= maxTxtWidth_) ||
            (invertLineCount < -1 || (invertLineStart + invertLineCount) > maxTxtWidth_))
        {
            return std::make_error_code(std::errc::invalid_argument);
        }

        if (rowBytes_ == 0 || text_.empty() == true || font_.empty() == true)
        {
            return std::error_code{};
        }

        auto unpackGlyph = [this](char glyphToUnpack)
        {
            uint64_t glyph = 0;
            // The index into the 64bit array font
            double index = (static_cast<uint8_t>(glyphToUnpack) - asciiBase_) * unpackRatio_;

            // Make sure there is no illegal ascii characters
            if (index >= 0 && index < font_.size())
            {
                // The byte index within the 64bit index to start reading from
                int bIndex = 8 * (index - static_cast<int>(index));

                // Unpack the glyph
                switch (bIndex)
                {
                    case 0:
                        glyph = (font_[index] >> 24) << 24;
                        break;
                    case 1:
                        glyph = (font_[index] >> 16) << 24;
                        break;
                    case 2:
                        glyph = (font_[index] >> 8) << 24;
                        break;
                    case 3:
                        glyph = (font_[index] >> 0) << 24;
                        break;
                    case 4:
                        glyph = (font_[index] << 32) | ((font_[index + 1] >> 56) << 24);
                        break;
                    case 5:
                        glyph = (font_[index] << 40) | ((font_[index + 1] >> 48) << 24);
                        break;
                    case 6:
                        glyph = (font_[index] << 48) | ((font_[index + 1] >> 40) << 24);
                        break;
                    case 7:
                        glyph = (font_[index] << 56) | ((font_[index + 1] >> 32) << 24);
                        break;
                    default:
                        // Invalid start of font glyph byte. This should not happen, a blank space will
                        // be printed in such a scenario.
                        break;
                }
            }
            //else
            //{
            //    An unsupported character, a blank space will be printed in such a scenario.
            //}

            return glyph;
        };
        auto errc = std::error_code{};
        int newLines = 0;
        int lineIndex = 0;
        int centreTxtIndex = 0;
        std::advance(start, anchorPoint_);
        auto it = start;
        auto centreTxt = [this, &it, end](int txtHeightIndex)
        {
            if (txtHeight_.empty() == false)
            {
                auto txtHeight = txtHeight_[txtHeightIndex];
                auto offset = ((maxTxtHeight_ - txtHeight) / 2) * rowBytes_;

                if (std::distance(it, end) < offset)
                {
                    return std::make_error_code(std::errc::no_buffer_space);
                }

                std::advance(it, offset);
            }

            return std::error_code{};
        };

        invertLineCount = invertLineCount == -1 ? maxTxtWidth_ - invertLineStart : invertLineCount;

        //todo: need to skip leading '\n' characters and std::advance those new lines

        // Centre the first row of text
        if (justification_ == Justification::Centre)
        {
            errc = centreTxt(centreTxtIndex);
        }

        for (auto c : text_)
        {
            if (errc)
            {
                return errc;
            }

            // Handle known special characters
            if (c == '\n')
            {
                newLines++;
            }
            else
            {
                if (newLines > 0)
                {
                    if (std::distance(start, end) < newLines)
                    {
                        return std::make_error_code(std::errc::no_buffer_space);
                    }

                    std::advance(start, newLines);
                    it = start;

                    if (justification_ == Justification::Centre)
                    {
                        errc = centreTxt(++centreTxtIndex);
                    }

                    lineIndex++;
                    newLines = 0;
                }

                // Our font only supports upper case - unknown characters will blit a space (clear to black)
                uint64_t glyph = unpackGlyph(c);

                for (auto rShift = 56; rShift >= 0; rShift -= 8)
                {
                    *it = (glyph >> rShift) & 0xFF;

                    if (invertLineCount > 0)
                    {
                        if (lineIndex >= invertLineStart && lineIndex < invertLineStart + invertLineCount)
                        {
                            *it = ~(*it | 0x01);
                        }
                    }

                    if (std::distance(it, end) <= rowBytes_)
                    {
                        // Set the error here and break, if there are more characters to print
                        // we return the error at the top of the loop
                        errc = std::make_error_code(std::errc::no_buffer_space);
                        break;
                    }

                    std::advance(it, rowBytes_);
                }
            }
        }

        return std::error_code{};
    }
} // namespace meen_i8080_arcade