/* akvirtualcamera, virtual camera for Mac and Windows.
 * Copyright (C) 2020  Gonzalo Exequiel Pedone
 *
 * akvirtualcamera is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * akvirtualcamera is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with akvirtualcamera. If not, see <http://www.gnu.org/licenses/>.
 *
 * Web-Site: http://webcamoid.github.io/
 */

#include <algorithm>
#include <codecvt>
#include <locale>
#include <mutex>
#include <random>
#include <thread>
#include <CoreMediaIO/CMIOSampleBuffer.h>

#include "stream.h"
#include "clock.h"
#include "PlatformUtils/src/preferences.h"
#include "PlatformUtils/src/utils.h"
#include "VCamUtils/src/videoformat.h"
#include "VCamUtils/src/videoframe.h"
#include "VCamUtils/src/logger.h"

namespace AkVCam
{
    class StreamPrivate
    {
        public:
            Stream *self;
            IpcBridge *m_bridge {nullptr};
            ClockPtr m_clock;
            UInt64 m_sequence;
            CMTime m_pts;
            SampleBufferQueuePtr m_queue;
            CMIODeviceStreamQueueAlteredProc m_queueAltered {nullptr};
            VideoFrame m_currentFrame;
            VideoFrame m_testFrame;
            VideoFrame m_testFrameAdapted;
            void *m_queueAlteredRefCon {nullptr};
            CFRunLoopTimerRef m_timer {nullptr};
            std::string m_broadcaster;
            std::mutex m_mutex;
            Scaling m_scaling {ScalingFast};
            AspectRatio m_aspectRatio {AspectRatioIgnore};
            bool m_running {false};
            bool m_horizontalMirror {false};
            bool m_verticalMirror {false};
            bool m_swapRgb {false};

            explicit StreamPrivate(Stream *self);
            bool startTimer();
            void stopTimer();
            static void streamLoop(CFRunLoopTimerRef timer, void *info);
            void sendFrame(const VideoFrame &frame);
            void updateTestFrame();
            VideoFrame applyAdjusts(const VideoFrame &frame);
            VideoFrame randomFrame();
    };
}

AkVCam::Stream::Stream(bool registerObject,
                       Object *parent):
    Object(parent)
{
    this->d = new StreamPrivate(this);
    this->m_className = "Stream";
    this->m_classID = kCMIOStreamClassID;
    auto picture = Preferences::picture();

    if (!picture.empty())
        this->d->m_testFrame = loadPicture(picture);

    this->d->m_clock =
            std::make_shared<Clock>("CMIO::VirtualCamera::Stream",
                                    CMTimeMake(1, 10),
                                    100,
                                    10);
    this->d->m_queue = std::make_shared<SampleBufferQueue>(30);

    if (registerObject) {
        this->createObject();
        this->registerObject();
    }

    this->m_properties.setProperty(kCMIOStreamPropertyClock, this->d->m_clock);
}

AkVCam::Stream::~Stream()
{
    this->registerObject(false);
    delete this->d;
}

OSStatus AkVCam::Stream::createObject()
{
    AkLogFunction();

    if (!this->m_pluginInterface
        || !*this->m_pluginInterface
        || !this->m_parent)
        return kCMIOHardwareUnspecifiedError;

    CMIOObjectID streamID = 0;

    auto status =
            CMIOObjectCreate(this->m_pluginInterface,
                             this->m_parent->objectID(),
                             this->m_classID,
                             &streamID);

    if (status == kCMIOHardwareNoError) {
        this->m_isCreated = true;
        this->m_objectID = streamID;
        AkLogInfo() << "Created stream: " << this->m_objectID << std::endl;
    }

    return status;
}

OSStatus AkVCam::Stream::registerObject(bool regist)
{
    AkLogFunction();
    OSStatus status = kCMIOHardwareUnspecifiedError;

    if (!this->m_isCreated
        || !this->m_pluginInterface
        || !*this->m_pluginInterface
        || !this->m_parent)
        return status;

    if (regist) {
        status = CMIOObjectsPublishedAndDied(this->m_pluginInterface,
                                             this->m_parent->objectID(),
                                             1,
                                             &this->m_objectID,
                                             0,
                                             nullptr);
    } else {
        status = CMIOObjectsPublishedAndDied(this->m_pluginInterface,
                                             this->m_parent->objectID(),
                                             0,
                                             nullptr,
                                             1,
                                             &this->m_objectID);
    }

    return status;
}

void AkVCam::Stream::setPicture(const std::string &picture)
{
    AkLogFunction();
    AkLogDebug() << "Picture: " << picture;
    this->d->m_testFrame = loadPicture(picture);
    this->d->updateTestFrame();
    this->d->m_mutex.lock();

    if (this->d->m_broadcaster.empty())
        this->d->m_currentFrame = this->d->m_testFrameAdapted;

    this->d->m_mutex.unlock();
}

void AkVCam::Stream::setBridge(IpcBridge *bridge)
{
    this->d->m_bridge = bridge;
}

void AkVCam::Stream::setFormats(const std::vector<VideoFormat> &formats)
{
    AkLogFunction();

    if (formats.empty())
        return;

    std::vector<VideoFormat> formatsAdjusted;

    for (auto format: formats) {
        int width;
        int height;
        AkVCam::VideoFormat::roundNearest(format.width(),
                                          format.height(),
                                          &width,
                                          &height);
        format.width() = width;
        format.height() = height;
        formatsAdjusted.push_back(format);
    }

    for (auto &format: formatsAdjusted)
        AkLogInfo() << "Format: "
                    << enumToString(format.fourcc())
                    << " "
                    << format.width()
                    << "x"
                    << format.height()
                    << std::endl;

    this->m_properties.setProperty(kCMIOStreamPropertyFormatDescriptions,
                                   formatsAdjusted);
    this->setFormat(formatsAdjusted[0]);
}

void AkVCam::Stream::setFormat(const VideoFormat &format)
{
    AkLogFunction();
    this->m_properties.setProperty(kCMIOStreamPropertyFormatDescription,
                                   format);
    this->m_properties.setProperty(kCMIOStreamPropertyFrameRates,
                                   format.frameRates());
    this->m_properties.setProperty(kCMIOStreamPropertyFrameRateRanges,
                                   format.frameRateRanges());
    this->m_properties.setProperty(kCMIOStreamPropertyMinimumFrameRate,
                                   format.minimumFrameRate().value());

    if (!format.frameRates().empty())
        this->setFrameRate(format.frameRates().front());
}

void AkVCam::Stream::setFrameRate(const Fraction &frameRate)
{
    this->m_properties.setProperty(kCMIOStreamPropertyFrameRate,
                                   frameRate.value());
}

bool AkVCam::Stream::start()
{
    AkLogFunction();

    if (this->d->m_running)
        return false;

    this->d->updateTestFrame();
    this->d->m_currentFrame = this->d->m_testFrameAdapted;
    this->d->m_sequence = 0;
    memset(&this->d->m_pts, 0, sizeof(CMTime));
    this->d->m_running = this->d->startTimer();
    AkLogInfo() << "Running: " << this->d->m_running << std::endl;

    return this->d->m_running;
}

void AkVCam::Stream::stop()
{
    AkLogFunction();

    if (!this->d->m_running)
        return;

    this->d->m_running = false;
    this->d->stopTimer();
    this->d->m_currentFrame.clear();
    this->d->m_testFrameAdapted.clear();
}

bool AkVCam::Stream::running()
{
    return this->d->m_running;
}

void AkVCam::Stream::serverStateChanged(IpcBridge::ServerState state)
{
    AkLogFunction();

    if (state == IpcBridge::ServerStateGone) {
        this->d->m_broadcaster.clear();
        this->d->m_horizontalMirror = false;
        this->d->m_verticalMirror = false;
        this->d->m_scaling = ScalingFast;
        this->d->m_aspectRatio = AspectRatioIgnore;
        this->d->m_swapRgb = false;
        this->d->updateTestFrame();

        this->d->m_mutex.lock();
        this->d->m_currentFrame = this->d->m_testFrameAdapted;
        this->d->m_mutex.unlock();
    }
}

// Device - 输入视频流
void AkVCam::Stream::frameReady(const AkVCam::VideoFrame &frame)
{
    AkLogFunction();
    AkLogInfo() << "Running: " << this->d->m_running << std::endl;
    AkLogInfo() << "Broadcaster: " << this->d->m_broadcaster << std::endl;

    if (!this->d->m_running)
        return;

    this->d->m_mutex.lock();

    if (!this->d->m_broadcaster.empty())
        this->d->m_currentFrame = this->d->applyAdjusts(frame);

    this->d->m_mutex.unlock();
}

void AkVCam::Stream::setBroadcasting(const std::string &broadcaster)
{
    AkLogFunction();
    AkLogDebug() << "Broadcaster: " << broadcaster << std::endl;

    if (this->d->m_broadcaster == broadcaster)
        return;

    this->d->m_mutex.lock();
    this->d->m_broadcaster = broadcaster;

    if (broadcaster.empty())
        this->d->m_currentFrame = this->d->m_testFrameAdapted;

    this->d->m_mutex.unlock();
}

void AkVCam::Stream::setHorizontalMirror(bool horizontalMirror)
{
    AkLogFunction();
    AkLogDebug() << "Mirror: " << horizontalMirror << std::endl;

    if (this->d->m_horizontalMirror == horizontalMirror)
        return;

    this->d->m_horizontalMirror = horizontalMirror;
    this->d->updateTestFrame();
    this->d->m_mutex.lock();

    if (this->d->m_broadcaster.empty())
        this->d->m_currentFrame = this->d->m_testFrameAdapted;

    this->d->m_mutex.unlock();
}

void AkVCam::Stream::setVerticalMirror(bool verticalMirror)
{
    AkLogFunction();
    AkLogDebug() << "Mirror: " << verticalMirror << std::endl;

    if (this->d->m_verticalMirror == verticalMirror)
        return;

    this->d->m_verticalMirror = verticalMirror;
    this->d->updateTestFrame();
    this->d->m_mutex.lock();

    if (this->d->m_broadcaster.empty())
        this->d->m_currentFrame = this->d->m_testFrameAdapted;

    this->d->m_mutex.unlock();
}

void AkVCam::Stream::setScaling(Scaling scaling)
{
    AkLogFunction();
    AkLogDebug() << "Scaling: " << scaling << std::endl;

    if (this->d->m_scaling == scaling)
        return;

    this->d->m_scaling = scaling;
    this->d->updateTestFrame();
    this->d->m_mutex.lock();

    if (this->d->m_broadcaster.empty())
        this->d->m_currentFrame = this->d->m_testFrameAdapted;

    this->d->m_mutex.unlock();
}

void AkVCam::Stream::setAspectRatio(AspectRatio aspectRatio)
{
    AkLogFunction();
    AkLogDebug() << "Aspect ratio: " << aspectRatio << std::endl;

    if (this->d->m_aspectRatio == aspectRatio)
        return;

    this->d->m_aspectRatio = aspectRatio;
    this->d->updateTestFrame();
    this->d->m_mutex.lock();

    if (this->d->m_broadcaster.empty())
        this->d->m_currentFrame = this->d->m_testFrameAdapted;

    this->d->m_mutex.unlock();
}

void AkVCam::Stream::setSwapRgb(bool swap)
{
    AkLogFunction();
    AkLogDebug() << "Swap: " << swap << std::endl;

    if (this->d->m_swapRgb == swap)
        return;

    this->d->m_swapRgb = swap;
    this->d->updateTestFrame();
    this->d->m_mutex.lock();

    if (this->d->m_broadcaster.empty())
        this->d->m_currentFrame = this->d->m_testFrameAdapted;

    this->d->m_mutex.unlock();
}

OSStatus AkVCam::Stream::copyBufferQueue(CMIODeviceStreamQueueAlteredProc queueAlteredProc,
                                         void *queueAlteredRefCon,
                                         CMSimpleQueueRef *queue)
{
    AkLogFunction();

    this->d->m_queueAltered = queueAlteredProc;
    this->d->m_queueAlteredRefCon = queueAlteredRefCon;
    *queue = queueAlteredProc? this->d->m_queue->ref(): nullptr;

    if (*queue)
        CFRetain(*queue);

    return kCMIOHardwareNoError;
}

OSStatus AkVCam::Stream::deckPlay()
{
    AkLogFunction();
    AkLogDebug() << "STUB" << std::endl;

    return kCMIOHardwareUnspecifiedError;
}

OSStatus AkVCam::Stream::deckStop()
{
    AkLogFunction();
    AkLogDebug() << "STUB" << std::endl;

    return kCMIOHardwareUnspecifiedError;
}

OSStatus AkVCam::Stream::deckJog(SInt32 speed)
{
    UNUSED(speed);
    AkLogFunction();
    AkLogDebug() << "STUB" << std::endl;

    return kCMIOHardwareUnspecifiedError;
}

OSStatus AkVCam::Stream::deckCueTo(Float64 frameNumber, Boolean playOnCue)
{
    UNUSED(frameNumber);
    UNUSED(playOnCue);
    AkLogFunction();
    AkLogDebug() << "STUB" << std::endl;

    return kCMIOHardwareUnspecifiedError;
}

AkVCam::StreamPrivate::StreamPrivate(AkVCam::Stream *self):
    self(self)
{
}

bool AkVCam::StreamPrivate::startTimer()
{
    AkLogFunction();

    if (this->m_timer)
        return false;

    Float64 fps = 0;
    this->self->m_properties.getProperty(kCMIOStreamPropertyFrameRate, &fps);

    CFTimeInterval interval = 1.0 / fps;
    CFRunLoopTimerContext context {0, this, nullptr, nullptr, nullptr};
    this->m_timer =
            CFRunLoopTimerCreate(kCFAllocatorDefault,
                                 0.0,
                                 interval,
                                 0,
                                 0,
                                 StreamPrivate::streamLoop,
                                 &context);

    if (!this->m_timer)
        return false;

    CFRunLoopAddTimer(CFRunLoopGetMain(),
                      this->m_timer,
                      kCFRunLoopCommonModes);

    return true;
}

void AkVCam::StreamPrivate::stopTimer()
{
    AkLogFunction();

    if (!this->m_timer)
        return;

    CFRunLoopTimerInvalidate(this->m_timer);
    CFRunLoopRemoveTimer(CFRunLoopGetMain(),
                         this->m_timer,
                         kCFRunLoopCommonModes);
    CFRelease(this->m_timer);
    this->m_timer = nullptr;
}

// Device - 循环读取视频流
void AkVCam::StreamPrivate::streamLoop(CFRunLoopTimerRef timer, void *info)
{
    UNUSED(timer);
    AkLogFunction();

    auto self = reinterpret_cast<StreamPrivate *>(info);
    AkLogInfo() << "Running: " << self->m_running << std::endl;

    if (!self->m_running)
        return;

    self->m_mutex.lock();

    if (self->m_currentFrame.format().size() < 1)
        self->sendFrame(self->randomFrame());
    else
        self->sendFrame(self->m_currentFrame);

    self->m_mutex.unlock();
}

void AkVCam::StreamPrivate::sendFrame(const VideoFrame &frame)
{
    AkLogFunction();

    if (this->m_queue->fullness() >= 1.0f)
        return;

    FourCC fourcc = frame.format().fourcc();
    int width = frame.format().width();
    int height = frame.format().height();

    AkLogInfo() << "Sending Frame: "
                << enumToString(fourcc)
                << " "
                << width
                << "x"
                << height
                << std::endl;

    bool resync = false;
    auto hostTime = CFAbsoluteTimeGetCurrent();
    auto pts = CMTimeMake(int64_t(hostTime), 1e9);
    auto ptsDiff = CMTimeGetSeconds(CMTimeSubtract(this->m_pts, pts));

    if (CMTimeCompare(pts, this->m_pts) == 0)
        return;

    Float64 fps = 0;
    this->self->m_properties.getProperty(kCMIOStreamPropertyFrameRate, &fps);

    if (CMTIME_IS_INVALID(this->m_pts)
        || ptsDiff < 0
        || ptsDiff > 2. / fps) {
        this->m_pts = pts;
        resync = true;
    }

    CMIOStreamClockPostTimingEvent(this->m_pts,
                                   UInt64(hostTime),
                                   resync,
                                   this->m_clock->ref());

    CVImageBufferRef imageBuffer = nullptr;
    CVPixelBufferCreate(kCFAllocatorDefault,
                        size_t(width),
                        size_t(height),
                        formatToCM(PixelFormat(fourcc)),
                        nullptr,
                        &imageBuffer);

    if (!imageBuffer)
        return;

    CVPixelBufferLockBaseAddress(imageBuffer, 0);
    auto data = CVPixelBufferGetBaseAddress(imageBuffer);
    memcpy(data, frame.data().data(), frame.data().size());
    CVPixelBufferUnlockBaseAddress(imageBuffer, 0);

    CMVideoFormatDescriptionRef format = nullptr;
    CMVideoFormatDescriptionCreateForImageBuffer(kCFAllocatorDefault,
                                                 imageBuffer,
                                                 &format);

    auto duration = CMTimeMake(1e3, int32_t(1e3 * fps));
    CMSampleTimingInfo timingInfo {
        duration,
        this->m_pts,
        this->m_pts
    };

    CMSampleBufferRef buffer = nullptr;
    CMIOSampleBufferCreateForImageBuffer(kCFAllocatorDefault,
                                         imageBuffer,
                                         format,
                                         &timingInfo,
                                         this->m_sequence,
                                         resync?
                                             kCMIOSampleBufferDiscontinuityFlag_UnknownDiscontinuity:
                                             kCMIOSampleBufferNoDiscontinuities,
                                         &buffer);
    CFRelease(format);
    CFRelease(imageBuffer);

    this->m_queue->enqueue(buffer);
    this->m_pts = CMTimeAdd(this->m_pts, duration);
    this->m_sequence++;

    if (this->m_queueAltered)
        this->m_queueAltered(this->self->m_objectID,
                             buffer,
                             this->m_queueAlteredRefCon);
}

void AkVCam::StreamPrivate::updateTestFrame()
{
    AkLogFunction();
    this->m_testFrameAdapted = this->applyAdjusts(this->m_testFrame);
}

// Stream - 输入的视频流转换成虚拟摄像头支持的格式
AkVCam::VideoFrame AkVCam::StreamPrivate::applyAdjusts(const VideoFrame &frame)
{
    AkLogFunction();

    VideoFormat format;
    this->self->m_properties.getProperty(kCMIOStreamPropertyFormatDescription,
                                         &format);

    FourCC fourcc = format.fourcc();
    int width = format.width();
    int height = format.height();

    if (width * height > frame.format().width() * frame.format().height()) {
        return frame.mirror(this->m_horizontalMirror,
                            this->m_verticalMirror)
                    .swapRgb(this->m_swapRgb)
                    .scaled(width, height,
                            this->m_scaling,
                            this->m_aspectRatio)
                    .convert(fourcc);
    }

    return frame.scaled(width, height,
                        this->m_scaling,
                        this->m_aspectRatio)
                .mirror(this->m_horizontalMirror,
                        this->m_verticalMirror)
                .swapRgb(this->m_swapRgb)
            .convert(fourcc);
}

AkVCam::VideoFrame AkVCam::StreamPrivate::randomFrame()
{
    VideoFormat format;
    this->self->m_properties.getProperty(kCMIOStreamPropertyFormatDescription,
                                         &format);
    VideoData data(format.size());
    static std::uniform_int_distribution<uint8_t> distribution(std::numeric_limits<uint8_t>::min(),
                                                               std::numeric_limits<uint8_t>::max());
    static std::default_random_engine engine;
    std::generate(data.begin(), data.end(), [] () {
        return distribution(engine);
    });

    VideoFrame frame;
    frame.format() = format;
    frame.data() = data;

    return frame;
}
