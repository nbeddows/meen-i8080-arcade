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

#ifndef GLYPHRENDERER_H
#define GLYPHRENDERER_H

#include <cstdint>
#include <string_view>
#include <system_error>
#include <vector>

namespace i8080_arcade
{
    /** Glyph Rendering

        Glyph renderering for 1bpp monospace fonts from a collection of built in fonts defined in the GlyphRenderer::Font enumeration.
    */
    class GlyphRenderer
    {
        public:
            /** A collection of built in fonts
            
                All fonts are monospace.
            */
            enum class Font
            {
                /**
                    A packed 8x8 font for the ascii range 47 (/) to 90 (Z) defined in the native i8080 arcade video format (1 bit cocktail orientation).
                    Each glyph consumes 40 bits. When written to the upper 40 bits of a 64 bit number the remaining 24 bits represents the space
                    between glyphs. This gives us a quick monospace font based on the Space Invaders arcade hardware. Each glyph is placed in the
                    upper left giving 3 pixles of space after the glyph and one pixel of space below yielding a 5x7 glyph in an 8x8 surface.
                    NOTE: any entres with all zeros are unused and are reserved for future use.
                */
                I8080ArcadeRegular8x8
            };

            /** Methods of aligning the text

                The justification is relative to the anchor point set via SetAnchorPoint.
            */
            enum class Justification
            {
                Left,   //< Left justify the glyphs.
                Centre, //< Centre justify the glyphs.
                Right   //< Right justify the glyphs (not supported).
            };

            /** Default constructor

                Deleted since a blittable surface row bytes value needs to be set before use.
            */
            GlyphRenderer() = delete;
            ~GlyphRenderer() = default;

            /** Initialisation constructor.

                @param      rowBytes    The number of bytes per row in the blittable surface passed to
                                        the Blit method.

                @remark     Setting a negative rowbytes will cause the Blit method to return std::errc::invalid_argument
                @remark     Setting a rowBytes value of 0 will cause the Blit method to return success
                            immediatley without performing any action. 
            */
            explicit GlyphRenderer(int rowBytes);

            /** Set anchor point
 
                The position on the blittable surface from which the glyphs will be rendered.

                @param  anchorPoint     The desired anchor point.

                @return                 std::error_code.

                                        std::error_code::invalid_arguemnt if the specified anchor point is less tham 0.
            */
            std::error_code SetAnchorPoint(int anchorPoint);

            /** Set Font
            
                The monospace font to use, see the Font enumeration for more details.

                @param  font            The desired font to render the glyphs in.

                @return                 std::error_code.

                                        std::errc::not_suported if the specified font is not supported.
            */
            std::error_code SetFont(Font font);

            /** Glyph positioning
            
                Justify the glyphs lines relative to the anchor point.

                @return                 std::error_code.

                                        std::errc::not_supported if Justification::Right is specified.
            */
            std::error_code SetJustification (Justification justification);

            /** The blittable text

                Set the text that will be rendered when the Blit method is called.

                @param  text            The text to render.
            */
            void SetText(std::string_view text);

            /** Glyph lines height
                
                @return     The number of lines (the height of the lines in bytes) of the glyphs represented by the string passed to the SetText method.
            */
            int GetWidth() const;

            /** Maximum glyph line width

                @return     The maximum width in bytes of a line of glyphs represented by the string passed to the SetText method.
            */
            int GetHeight() const;

            /**
                @param      text        The characters to update the text set with SetText.
                @param      position    The index into the text set via SetText at which it will be updated with
                                        the text parameter.

                @return                 A std::error_code

                                        std::errc::interrupted: the text parameter has caused a change to the values that are returned
                                                                by the methods GetWidth and/or GetHeight.

                @remark                 Acting on the error is important in certain scenarios, for example, if you want to make sure that
                                        the glyphs rendered are centered on the surface. In this case, you would need to call the method
                                        SetAnchorPoint again with a new point based on the updated values returned by the methods GetWidth
                                        and GetHeight.
            */
            std::error_code Update(std::string_view text, int position = 0);

            /** Blit the configured string to the given iterator

                Blit the text set via the SetText method with the configured font to the location pointed to by the supplied
                iterator. New lines can be used in the given string to blit the text in rows.

                @param      start               An iterator to std::vector holding the beginning of the blittable surface.
                @param      end                 An iterator to std::vector holding the end of the blittable surface
                @param      invertLineCount     The number of lines of glyphs that will blit inverted (black instead of white).
                                                The default value is 0 (disable inversion).
                                                The value of -1 can be used to invert all lines from the invertLineStart
                                                parameter.
                @param      invertLineStart     The line index to start inverting glyphs from. The default value is 0 (start from
                                                the first line).

                @return                         A std::error_code.

                                                std::errc::invalid_argument: the supplied anchor point is out of range of the suppiled iterators.
                                                                             the invertLineCount parameter is < -1 or greater than
                                                                             GetMaxWidth - invertLineStart.
                                                                             the invertLineStart parameter is < 0 or greater than GetMaxWidth.
                                                std::errc::no_buffer_space: when word wrapping or an out bounds write has been detected.

                @remark                         Center justification has no effect on a string without new lines.
                @remark                         The SetText method needs to be called with a string to render, otherwise this method does nothing.
                @remark                         The SetFont method needs to be called with a valid GlyphRenerer::Font, otherwise this method does nothing.
                @remark                         Unknown font glyphs print a space (clear to black (or white if inversion is enabled)).
                @remark                         The method GetMaxWidth can be used to obtain to total number of lines of glyphs.
            */
            std::error_code Blit(std::vector<uint8_t>::iterator start, std::vector<uint8_t>::iterator end, int invertLineCount = 0, int invertLineStart = 0) const;

        private:
            /** Font compression ratio
            
                We only use 5 bytes of the 8 so we pack the rest so we don't waste space
                (8 glyphs per 5 64bit numbers (5/8).
            */
            static constexpr double unpackRatio_ = 0.625;

            /** The font to render with
            
                This is set via the SetFont method.
            */
            std::vector<uint64_t> font_;
        
            /** The base character for indexing puropses
            
                This value is dependent on the font in use.
            */
            uint8_t asciiBase_{};
            
            /** Glyph alignment
            
                This is set via the Justification method.

                @remark Justification is relative to the anchor point
            */
            Justification justification_{};

            /** Render text
            
                This is set via the SetText method.
            */
            std::string text_;

            /** Store the widths of all glyph lines

                The width of all lines of text (separated by '\n') in the text_ string
            */
            std::vector<int> txtHeight_;

            /** Bytes per row
               
               The number of bytes per row in the blittable surface iterator passed to the Blit method.
            */
            int rowBytes_{};

            /** Total glyph lines
            
                The total number of lines in the string passed to the SetText method.

                @remark this is equivalent to the total number of bytes.
            */
            int maxTxtWidth_{};

            /** Maximum glyph line width
 
                The maximum number of bytes in a line of glyphs.
            */
            int maxTxtHeight_{};

            /** The point from which text is rendered
            
                Set via the AnchorPoint method.
            */
            int anchorPoint_{};

            /** Recalcuate the required metadata of the text set via the SetText method
                
                This will update the values obtained from the GetWidth and GetHeight methods.
            */
            void Configure();
    };
} // namespace i8080_arcade

#endif // GLYPHRENDERER_H