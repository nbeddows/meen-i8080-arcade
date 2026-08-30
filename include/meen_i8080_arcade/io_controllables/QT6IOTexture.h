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

#ifndef QT6IOTEXTURE_H
#define QT6IOTEXTURE_H

#include <QSGSimpleTextureNode>
#include <rhi/qrhi.h>

namespace meen_i8080_arcade
{
    class QT6IOTexture final : public QSGTexture
    {
    public:
        QT6IOTexture() = default;
        ~QT6IOTexture() final = default;

        void setImage(const QImage& image);
        QSize textureSize() const final;
        bool hasAlphaChannel() const final;
        bool hasMipmaps() const final;
        qint64 comparisonKey() const final;
        QRhiTexture* rhiTexture() const final;
        void commitTextureOperations(QRhi* rhi, QRhiResourceUpdateBatch* resourceUpdates) final;

    private:
        std::unique_ptr<QRhiTexture> texture_;
        QImage image_;
        bool dirty_{};
    };
} // namespace meen_i8080_arcade

#endif // QT6IOTEXTURE_H