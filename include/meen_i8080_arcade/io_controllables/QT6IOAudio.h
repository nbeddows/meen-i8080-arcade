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

#ifndef QT6IOAUDIO_H
#define QT6IOAUDIO_H

#include <QIODevice>

#include "meen_hw/MH_ResourcePool.h"

namespace meen_i8080_arcade
{
    /** A basic audio output device
    
        The output device maintains a cache of audio resource pointers from the
        external audio frame pool. Audio data is read out frame at a time which
        then allows the exhausted frame to be returned back to the frame pool
        which allows for audio frame buffer recycling.
    */
    class QT6IOAudio : public QIODevice
    {
    public:
        explicit QT6IOAudio(QObject* parent = nullptr);
        QT6IOAudio() = delete;
        ~QT6IOAudio() = default;

        /** Push an audio frame to the internal cache
        
            Qt will read audio data back at the required rate. Audio frames must be pushed
            to the internal cache at at least the required rate.

            @param      frame       The audio frame to move into the audio cache.
        */
        void PushAudioFrame(meen_hw::MH_ResourcePool<std::vector<int32_t>>::ResourcePtr&& frame);
    protected:
        /** Qt audio read override
        
            Qt framework will call this method when audio frames are required.

            @param      data        The audio data buffer to write to.
            @param      maxLen      The maximum length in bytes that the data buffer will accept.

            @return                 The number of audio data bytes written to the data parameter.
        */
        qint64 readData (char* data, qint64 maxLen) final;
        
        /** Qt write audio override
        
            Not used in this implementation.

            @param      data        The audio data buffer to read from.
            @param      maxLen      The maximum length in bytes that can be read from the data parameter.

            @remark     This method will always return -1.
        */
        qint64 writeData(const char* data, qint64 maxLen) final;

        /* Qt bytes available override
        
            The size in bytes of the internal audio buffer cache (the available bytes remaining from
            the audio frames sent to the PushAudioFrame method.
        */
        qint64 bytesAvailable() const final;
     private:
        /** Internal audio frame buffer cache
        
            A list of audio frames that are cached from the external audio frame pool.
        */
        std::list<meen_hw::MH_ResourcePool<std::vector<int32_t>>::ResourcePtr> frames_;
    };
} // namespace meen_i8080_arcade

#endif // QT6IOAUDIO_H