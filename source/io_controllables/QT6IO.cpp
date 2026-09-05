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

#include <QAudioFormat>
#include <QMediaDevices.h>
#include <QQuickWindow>

#include "meen_i8080_arcade/io_controllables/QT6IO.h"

namespace meen_i8080_arcade
{
    void QT6IO::Init()
    {

    }

    int QT6IO::windowWidth() const
    {
        return windowWidth_;
    }

    int QT6IO::windowHeight() const
    {
        return windowHeight_;
    }

    std::errc QT6IO::ConfigureVideoDevice(int width, int height, int fullscreen)
    {
        // QT6 IO controller can only be successsfully configured once
        if (app_ != nullptr && engine_ != nullptr && QT6IODisplay_ != nullptr)
        {
            return std::errc::operation_not_permitted;
        }

        int argc = 0;
        char** argv = nullptr;

        app_ = std::make_unique<QGuiApplication>(argc, argv);
        engine_ = std::make_unique<QQmlApplicationEngine>();

        qmlRegisterSingletonInstance("meen_i8080_arcade", 1, 1, "QT6IO", this);
        qmlRegisterType<QT6IODisplay>("meen_i8080_arcade", 1, 1, "QT6IODisplay");

        engine_->loadFromModule("meen_i8080_arcade", "QT6IOMain");

        // QT6IOMain module faile to load
        if (engine_->rootObjects().isEmpty() == true)
        {
            return std::errc::connection_aborted;
        }

        auto* window = qobject_cast<QQuickWindow*>(engine_->rootObjects().constFirst());

        // QML has no window declared
        if (window == nullptr)
        {
            return std::errc::not_connected;
        }

        // Grab the qml display so we can render frames to it
        QT6IODisplay_ = window->findChild<QT6IODisplay*>();

        // QML has no QTIO6Display declared
        if (QT6IODisplay_ == nullptr)
        {
            return std::errc::not_connected;
        }

        windowWidth_ = width;
        windowHeight_ = height;

        // tell qml that our window dimensions have changed, this should update the window dimension to the correct size
        emit windowWidthChanged();
        emit windowHeightChanged();

        return std::errc{};
    }

    std::errc QT6IO::ConfigureAudioDevice(int sampleRate, int channels, int sampleSize)
    {
        if (QT6IOAudioSink_ != nullptr && QT6IOAudio_.isOpen() == true)
        {
            return std::errc::operation_not_permitted;
        }

        QAudioFormat format;
        format.setSampleRate(sampleRate);
        format.setChannelCount(channels);
        format.setSampleFormat(QAudioFormat::Int16);

        QT6IOAudioSink_ = std::make_unique<QAudioSink>(QMediaDevices::defaultAudioOutput(), format);
        QT6IOAudioSink_->start(&QT6IOAudio_);

        QObject::connect(QT6IOAudioSink_.get(), &QAudioSink::stateChanged, [](QAudio::State state)
        {
            switch (state)
            {
                case QAudio::State::ActiveState:
                {
                    qDebug() << "audio sink state: active";
                    break;
                }
                case QAudio::State::IdleState:
                {
                    qDebug() << "audio sink state: idle";
                    break;
                }
                case QAudio::State::StoppedState:
                {
                    qDebug() << "audio sink state: stopped";
                    break;
                }
                case QAudio::State::SuspendedState:
                {
                    qDebug() << "audio sink state: suspended";
                    break;
                }
                default:
                {
                    qFatal() << "audio sink unknown state: " << state;
                    break;
                }
            }
        });

        return std::errc{};
    }

    std::errc QT6IO::ConfigurePeripheralDevice()
    {
        return std::errc{};
    }

    std::errc QT6IO::LoadAudioSamples(int sampleRate, int channels, int sampleSize)
    {
        return std::errc{};
    }

    std::errc QT6IO::LoadVideoTextures(int bpp, int textureWidth, int textureHeight, int* numScanlines)
    {
        return QT6IODisplay_->AllocateImage(textureWidth, textureHeight, bpp, numScanlines);
    }

    std::errc QT6IO::ScreenTransition(Screen curr, Screen next)
    {
        return std::errc{};
    }

    std::array<uint8_t, 16> QT6IO::Uuid() const
    {
        return { 0xcb, 0xb6, 0x05, 0x1d, 0xc9, 0x7f, 0x4c, 0x22, 0x95, 0xa4, 0x0, 0x2b, 0x45, 0xbe, 0xc1, 0x8e };
    }

    std::errc QT6IO::RenderAudioFrame(meen_hw::MH_ResourcePool<std::vector<int32_t>>::ResourcePtr&& frame, uint64_t timestamp)
    {
        QT6IOAudio_.PushAudioFrame(std::move(frame));
        return std::errc{};
    }

    std::errc QT6IO::RenderVideoFrame(int scanlineStart, int numScanlines, uint64_t timestamp)
    {
        return std::errc{};
    }

    std::errc QT6IO::DisplayVideoFrame(uint64_t timestamp)
    {
        QT6IODisplay_->update();
        return std::errc{};
    }

    std::errc QT6IO::GetVideoFrameBuffer(const uint8_t** dst, int* dstRowBytes, int scanlineStart, int numScanlines) const
    {
        return QT6IODisplay_->GetImageBuffer(dst, dstRowBytes, scanlineStart, numScanlines);
    }

    uint32_t QT6IO::ReadPeripheralDevice()
    {
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        return input_;
    }

    std::errc QT6IO::RenderErrorString(const std::string& error)
    {
        qCritical() << error;
        return std::errc{};
    }

    std::errc QT6IO::ClearDisplay(BoundingBox&& rect)
    {
        return std::errc{};
    }

    void QT6IO::KeyPressed(Qt::Key key)
    {
        if (keyToInput_.contains(key) == true)
        {
            input_ |= keyToInput_[key];
        }
    }

    void QT6IO::KeyReleased(Qt::Key key)
    {
        if (keyToInput_.contains(key) == true)
        {
            input_ &= ~keyToInput_[key];
        }
    }
} // namespace meen_i8080_arcade