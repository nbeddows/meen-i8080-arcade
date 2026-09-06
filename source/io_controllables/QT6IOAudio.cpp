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

#include <QDebug>

#include "meen_i8080_arcade/io_controllables/QT6IOAudio.h"

namespace meen_i8080_arcade
{
    QT6IOAudio::QT6IOAudio(QObject* parent) : QIODevice(parent)
    {
        open(QIODevice::ReadOnly);
    };

    void QT6IOAudio::PushAudioFrame(meen_hw::MH_ResourcePool<std::vector<int32_t>>::ResourcePtr&& frame)
    {
        frames_.emplace_back(std::move(frame));
    }

    qint64 QT6IOAudio::writeData(const char* data, qint64 maxLen)
    {
        return -1;
    }

    qint64 QT6IOAudio::readData (char* data, qint64 maxLen)
    {
        int bytesCopied = 0;

        if (frames_.empty() == false)
        {
            if (maxLen > 0)
            {
                auto frame = std::move(frames_.front());
                assert(frame->size() <= maxLen);

                bytesCopied = std::min(static_cast<qint64>(frame->size()), maxLen) * sizeof(int32_t);
                memcpy(data, frame->data(), bytesCopied);

                // Return the frame back to the audio resource pool
                frame = nullptr;
                frames_.pop_front();
            }
        }
        else
        {
            qWarning() << "Audio bytes requested but the audio cache is empty!";
        }

        return bytesCopied;
    }

    qint64 QT6IOAudio::bytesAvailable() const
    {
        int totalBytes = 0;

        for (const auto& frame : frames_)
        {
            totalBytes += frame->size();
        }

        return QIODevice::bytesAvailable() + totalBytes;
    }
} // namespace meen_i8080_arcade