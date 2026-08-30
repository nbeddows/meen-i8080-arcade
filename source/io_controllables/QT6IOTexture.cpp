#include "meen_i8080_arcade/io_controllables/QT6IOTexture.h"

namespace meen_i8080_arcade
{
    void QT6IOTexture::setImage(const QImage& image)
    {
        image_ = image;
        dirty_ = true;
    }

    QSize QT6IOTexture::textureSize() const
    {
        return image_.size();
    }

    bool QT6IOTexture::hasAlphaChannel() const
    {
        return false;
    }

    bool QT6IOTexture::hasMipmaps() const
    {
        return false;
    }

    qint64 QT6IOTexture::comparisonKey() const
    {
        return qint64(quintptr(this));
    }

    QRhiTexture* QT6IOTexture::rhiTexture() const
    {
        return texture_.get();
    }

    void QT6IOTexture::commitTextureOperations(QRhi* rhi, QRhiResourceUpdateBatch* resourceUpdates)
    {
        if (image_.isNull())
        {
            return;
        }

        if (texture_ == nullptr)
        {
            texture_ = std::unique_ptr<QRhiTexture>(rhi->newTexture(QRhiTexture::R8, image_.size()));

            if (texture_->create() == false)
            {
                // todo: log the error
                texture_ = nullptr;
                return;
            }
        }

        if (dirty_ == true)
        {
            resourceUpdates->uploadTexture(texture_.get(), image_);
            dirty_ = false;
        }
    }
} // namespace meen_i8080_arcade