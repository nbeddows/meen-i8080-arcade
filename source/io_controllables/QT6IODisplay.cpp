/*
Copyright (c) 2021-2026 Nicolas Beddows <nicolas.beddows@gmail.com>

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

#include "meen_i8080_arcade/io_controllables/QT6IODisplay.h"
#include "meen_i8080_arcade/io_controllables/QT6IOTexture.h"

namespace meen_i8080_arcade
{
    QT6IODisplay::QT6IODisplay(QQuickItem* parent)
        : QQuickItem(parent)
    {
        setFlag(ItemHasContents, true);
    }

    std::errc QT6IODisplay::AllocateImage(int width, int height, int bpp, int* numScanlines)
    {
        auto err = std::errc{};
        auto format = QImage::Format::Format_Invalid;

        switch (bpp)
        {
            case 8:
            {
                format = QImage::Format::Format_Grayscale8;
                break;
            }
            case 16:
            {
                format = QImage::Format::Format_Grayscale16;
                break;
            }
            default:
            {
                err = std::errc::invalid_argument;
            }
        }

        image_ = QImage(width, height, format);
        
        if (image_.isNull())
        {
            err = std::errc::not_enough_memory;
        }
        else
        {
            *numScanlines = image_.height();
        }

        //setTextureSize(QSize(width, height));
        //setAntialiasing(false);
        return err;
    }

    QSGNode* QT6IODisplay::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*)
    {
        auto* node = static_cast<QSGSimpleTextureNode*>(oldNode);

        if (node == nullptr)
        {
            node = new QSGSimpleTextureNode;
            auto* texture = new QT6IOTexture;
            node->setTexture(texture);
            node->setOwnsTexture(true);
            node->setFiltering(QSGTexture::Nearest);
        }

        if (!image_.isNull())
        {
            auto* texture = static_cast<QT6IOTexture*>(node->texture());
            texture->setImage(image_);
        }

        node->setRect(boundingRect());

        return node;
    }

    std::errc QT6IODisplay::GetImageBuffer(const uint8_t** dst, int* dstRowBytes, int scanlineStart, int numScanlines) const
    {
        // TODO: perform scanline checks, offset dst by scanlineStart into the image buffer
        // check ptrs for nullptr

        *dst = image_.bits();
        *dstRowBytes = image_.bytesPerLine();
        return std::errc{};
    }
} // namespace meen_i8080_arcade