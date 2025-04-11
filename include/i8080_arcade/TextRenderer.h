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

#ifndef TEXTRENDERER_H
#define TEXTRENDERER_H

#include <array>
#include <string_view>
#include <system_error>

namespace i8080_arcade
{
    /** Text Rendering
    
        Render text via an 8x8 i8080 arcade specific font to a
        native (cocktail) orientation 1bpp i8080 arcade surface.
    */
    class TextRenderer
    {
        private:
            /**
             A packed 8x8 font for the ascii range 47 (/) to 90 (Z) defined in the native i8080 arcade video format (1 bit cocktail orientation).
             Each glyph consumes 40 bits. When written to the upper 40 bits of a 64 bit number the remaining 24 bits represents the space
             between glyphs. This gives us a quick monospace font based on the Space Invaders arcade hardware. Each glyph is placed in the
             upper left giving 3 pixles of space after the glyph and one pixel of space below yielding a 5x7 glyph in an 8x8 surface.
             NOTE: any entres with all zeros are unused and may be implemented at a later date if those characters are required in the future.
            */
            constexpr static std::array<uint64_t, 28> font_
            {
            //    /         0             1         2             3             4         5             6
                0x060C1830607C8A92, 0xA27C0242FE020246, 0x8A929262848292B2, 0xCC182848FE08E4A2, 0xA2A29C3C5292928C,
            //    7         8             9         :             ;             <         =             >
                0x808E90A0C06C9292, 0x926C629292947800, 0x0000000000000000, 0x0010284482002828, 0x2828288244281000,
            //    ?         @             A         B             C             D         E             F
                0x40809AA040000000, 0x00003E4888483EFE, 0x9292926C7C828282, 0x44FE8282827CFE92, 0x9292820000000000,
            //    G         H             I         J             K             L         M             N
                0x0000000000FE1010, 0x10FE8282FE828200, 0x0000000000000000, 0x00FE02020202FE40, 0x3040FEFE201008FE,
            //    O         P             Q         R             S             T         U             V
                0x7C8282827CFE9090, 0x90600000000000FE, 0x9098946264929292, 0x4C8080FE8080FC02, 0x0202FCF8040204F8,
            //    W         X             Y         Z
                0xFE041804FE000000, 0x0000C0201E20C000 ,0x0000000000000000
            };

            /** The base character for indexing puropses
            
                NOTE: the current value is a forward slash, however, it will have to be changed if we decide to update the
                supported ascii range.
            */
            static constexpr uint8_t asciiBase_ = static_cast<uint8_t>('/');
            
            /** Font compression ratio.
            
                We only use 5 bytes of the 8 so we pack the rest so we don't waste space
                (8 glyphs per 5 64bit numbers (5/8).
            */
            static constexpr double unpackRatio = 0.625;
            
        public:            
            /** Font blit template method
            
                Blit the text in the specified string to the location pointed to by the supplied iterator. New lines
                can be used in the given string to blit the text in rows.

                Three template parameters are required:

                1. surfaceSize: The size of the blittable surface in bytes in the the i8080 arcade native pixel format (1bpp).
                2. surfaceRowBytes: The length of each row in bytes of the surface pointed to by the given iterator.
                3. centre: True to centre justify the text, false for left justification. The supplied iterator is
                used as the anchor point.

                @param      it      The iterator to std::array holding the surface to blit to.
                @param      text    The characters to blit.
                
                @return             A std::error_code.

                @remark             The center template parameter hsa no effect on strings without new lines.
            */
            template<int surfaceSize, int surfaceRowBytes, bool centre>
            static std::error_code Blit(std::array<uint8_t, surfaceSize>::iterator&& start, std::array<uint8_t, surfaceSize>::iterator&& end, std::string_view text)
            {
                auto unpackGlyph = [](char glyphToUnpack)
                {
                    uint64_t glyph = 0;
                    // The index into the 64bit array font
                    double index = (static_cast<uint8_t>(glyphToUnpack) - asciiBase_) * unpackRatio;

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
                int maxTxtLen = 0;
                std::list<int> txtLen;
                auto errc = std::error_code{};
                auto it = start;
                int newLines = 0;

                auto centreTxt = [&txtLen, &maxTxtLen, &it, &end]
                {
                    if (txtLen.empty() == false)
                    {
                        auto len = txtLen.front();
                        txtLen.pop_front();
                        auto offset = ((maxTxtLen - len) / 2) * surfaceRowBytes;

                        if (std::distance(it, end) < offset)
                        {
                            return std::make_error_code(std::errc::no_buffer_space);
                        }

                        std::advance(it, offset);
                    }

                    return std::error_code{};
                };

                // Find the longest substring (newline delimiter) length in the array
                // So we can use this to center the remaining strings around.
                if constexpr (centre == true)
                {
                    size_t currPos = 0;
                    size_t lastPos = -1;

                    auto setMaxLen = [&txtLen, &maxTxtLen](size_t len)
                    {
                        if (len > maxTxtLen)
                        {
                            maxTxtLen = len;
                        }

                        txtLen.push_back(len);
                    };

                    do
                    {
                        auto len = 0;
                        currPos = text.find('\n', ++lastPos);

                        if (currPos == std::string::npos)
                        {
                            len = text.length() - lastPos;
                        }
                        else
                        {
                            len = currPos - lastPos;

                            // skip consecutive new lines
                            while (currPos + 1 < text.length() && text[currPos + 1] == '\n')
                            {
                                currPos++;
                            }
                        }

                        setMaxLen(len * 8); // 8 - in bits
                        lastPos = currPos;
                    }
                    while(currPos != std::string::npos);

                    // Centre the first row of text
                    errc = centreTxt();
                }

                for (auto c : text)
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

                            if constexpr (centre == true)
                            {
                                errc = centreTxt();
                            }

                            newLines = 0;
                        }
                        
                        // Our font only supports upper case - unknown characters will blit a space (clear to black)
                        uint64_t glyph = unpackGlyph(std::toupper(c));

                        for (auto rShift = 56; rShift >= 0; rShift -= 8)
                        {
                            *it = (glyph >> rShift) & 0xFF;

                            if (std::distance(it, end) < surfaceRowBytes)
                            {
                                // Set the error here and break, if there are more characters to print
                                // we return the error at the top of the loop
                                errc = std::make_error_code(std::errc::no_buffer_space);
                                break;
                            }

                            std::advance(it, surfaceRowBytes);
                        }
                    }
                }

                return std::error_code{};
            }
    };
};

#endif // TEXTRENDERER_H