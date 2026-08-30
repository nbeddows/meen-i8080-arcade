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

#ifndef QT6IODISPLAY_H
#define QT6IODISPLAY_H

#include <QImage>
#include <QQuickItem>

namespace meen_i8080_arcade
{
    class QT6IODisplay : public QQuickItem
    {
        Q_OBJECT

    public:
        explicit QT6IODisplay(QQuickItem* parent = nullptr);

        std::errc AllocateImage(int width, int height, int bpp, int* numScanlines);
        std::errc GetImageBuffer(const uint8_t** dst, int* dstRowBytes, int scanlineStart, int numScanlines) const;

    protected:
        QSGNode* updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData* data) final;

    private:
        QImage image_;
    };
} // namespace meen_i8080_arcade

#endif // QT6IODISPLAY_H