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
#include <cmath>
#include <cstring>
#include <fstream>

#include "videoframe.h"
#include "videoformat.h"
#include "utils.h"

namespace AkVCam
{
    struct RGB32
    {
        uint8_t x;
        uint8_t b;
        uint8_t g;
        uint8_t r;
    };

    struct RGB24
    {
        uint8_t b;
        uint8_t g;
        uint8_t r;
    };

    struct RGB16
    {
        uint16_t b: 5;
        uint16_t g: 6;
        uint16_t r: 5;
    };

    struct RGB15
    {
        uint16_t b: 5;
        uint16_t g: 5;
        uint16_t r: 5;
        uint16_t x: 1;
    };

    struct BGR32
    {
        uint8_t r;
        uint8_t g;
        uint8_t b;
        uint8_t x;
    };

    struct BGR24
    {
        uint8_t r;
        uint8_t g;
        uint8_t b;
    };

    struct BGR16
    {
        uint16_t r: 5;
        uint16_t g: 6;
        uint16_t b: 5;
    };

    struct BGR15
    {
        uint16_t r: 5;
        uint16_t g: 5;
        uint16_t b: 5;
        uint16_t x: 1;
    };

    struct UYVY
    {
        uint8_t v0;
        uint8_t y0;
        uint8_t u0;
        uint8_t y1;
    };

    struct YUY2
    {
        uint8_t y0;
        uint8_t v0;
        uint8_t y1;
        uint8_t u0;
    };

    struct UV
    {
        uint8_t u;
        uint8_t v;
    };

    struct VU
    {
        uint8_t v;
        uint8_t u;
    };

    using VideoConvertFuntion = VideoFrame (*)(const VideoFrame *src);

    struct VideoConvert
    {
        FourCC from;
        FourCC to;
        VideoConvertFuntion convert;
    };

    class VideoFramePrivate
    {
        public:
            VideoFrame *self;
            VideoFormat m_format;
            VideoData m_data;
            std::vector<VideoConvert> m_convert;
            std::vector<PixelFormat> m_adjustFormats;

            explicit VideoFramePrivate(VideoFrame *self):
                self(self)
            {
                // Stream - 视频格式转换函数表
                this->m_convert = {
                    {PixelFormatBGR24, PixelFormatRGB32, bgr24_to_rgb32},
                    {PixelFormatBGR24, PixelFormatRGB24, bgr24_to_rgb24},
                    {PixelFormatBGR24, PixelFormatRGB16, bgr24_to_rgb16},
                    {PixelFormatBGR24, PixelFormatRGB15, bgr24_to_rgb15},
                    {PixelFormatBGR24, PixelFormatBGR32, bgr24_to_bgr32},
                    {PixelFormatBGR24, PixelFormatBGR16, bgr24_to_bgr16},
                    {PixelFormatBGR24, PixelFormatBGR15, bgr24_to_bgr15},
                    {PixelFormatBGR24, PixelFormatUYVY , bgr24_to_uyvy },
                    {PixelFormatBGR24, PixelFormatYUY2 , bgr24_to_yuy2 },
                    {PixelFormatBGR24, PixelFormatNV12 , bgr24_to_nv12 },
                    {PixelFormatBGR24, PixelFormatNV21 , bgr24_to_nv21 },

                    {PixelFormatRGB24, PixelFormatRGB32, rgb24_to_rgb32},
                    {PixelFormatRGB24, PixelFormatRGB16, rgb24_to_rgb16},
                    {PixelFormatRGB24, PixelFormatRGB15, rgb24_to_rgb15},
                    {PixelFormatRGB24, PixelFormatBGR32, rgb24_to_bgr32},
                    {PixelFormatRGB24, PixelFormatBGR24, rgb24_to_bgr24},
                    {PixelFormatRGB24, PixelFormatBGR16, rgb24_to_bgr16},
                    {PixelFormatRGB24, PixelFormatBGR15, rgb24_to_bgr15},
                    {PixelFormatRGB24, PixelFormatUYVY , rgb24_to_uyvy },
                    {PixelFormatRGB24, PixelFormatYUY2 , rgb24_to_yuy2 },
                    {PixelFormatRGB24, PixelFormatNV12 , rgb24_to_nv12 },
                    {PixelFormatRGB24, PixelFormatNV21 , rgb24_to_nv21 }
                };

                this->m_adjustFormats = {
                    PixelFormatBGR24,
                    PixelFormatRGB24
                };
            }

            template<typename T>
            static inline T bound(T min, T value, T max)
            {
                return value < min? min: value > max? max: value;
            }

            template<typename T>
            inline T mod(T value, T mod)
            {
                return (value % mod + mod) % mod;
            }

            inline int grayval(int r, int g, int b);

            // YUV utility functions
            inline static uint8_t rgb_y(int r, int g, int b);
            inline static uint8_t rgb_u(int r, int g, int b);
            inline static uint8_t rgb_v(int r, int g, int b);
            inline static uint8_t yuv_r(int y, int u, int v);
            inline static uint8_t yuv_g(int y, int u, int v);
            inline static uint8_t yuv_b(int y, int u, int v);

            // BGR to RGB formats
            static VideoFrame bgr24_to_rgb32(const VideoFrame *src);
            static VideoFrame bgr24_to_rgb24(const VideoFrame *src);
            static VideoFrame bgr24_to_rgb16(const VideoFrame *src);
            static VideoFrame bgr24_to_rgb15(const VideoFrame *src);

            // BGR to BGR formats
            static VideoFrame bgr24_to_bgr32(const VideoFrame *src);
            static VideoFrame bgr24_to_bgr16(const VideoFrame *src);
            static VideoFrame bgr24_to_bgr15(const VideoFrame *src);

            // BGR to Luminance+Chrominance formats
            static VideoFrame bgr24_to_uyvy(const VideoFrame *src);
            static VideoFrame bgr24_to_yuy2(const VideoFrame *src);

            // BGR to two planes -- one Y, one Cr + Cb interleaved
            static VideoFrame bgr24_to_nv12(const VideoFrame *src);
            static VideoFrame bgr24_to_nv21(const VideoFrame *src);

            // RGB to RGB formats
            static VideoFrame rgb24_to_rgb32(const VideoFrame *src);
            static VideoFrame rgb24_to_rgb16(const VideoFrame *src);
            static VideoFrame rgb24_to_rgb15(const VideoFrame *src);

            // RGB to BGR formats
            static VideoFrame rgb24_to_bgr32(const VideoFrame *src);
            static VideoFrame rgb24_to_bgr24(const VideoFrame *src);
            static VideoFrame rgb24_to_bgr16(const VideoFrame *src);
            static VideoFrame rgb24_to_bgr15(const VideoFrame *src);

            // RGB to Luminance+Chrominance formats
            static VideoFrame rgb24_to_uyvy(const VideoFrame *src);
            static VideoFrame rgb24_to_yuy2(const VideoFrame *src);

            // RGB to two planes -- one Y, one Cr + Cb interleaved
            static VideoFrame rgb24_to_nv12(const VideoFrame *src);
            static VideoFrame rgb24_to_nv21(const VideoFrame *src);

            inline static void extrapolateUp(int dstCoord,
                                             int num, int den, int s,
                                             int *srcCoordMin, int *srcCoordMax,
                                             int *kNum, int *kDen);
            inline static void extrapolateDown(int dstCoord,
                                               int num, int den, int s,
                                               int *srcCoordMin, int *srcCoordMax,
                                               int *kNum, int *kDen);
            inline uint8_t extrapolateComponent(uint8_t min, uint8_t max,
                                               int kNum, int kDen) const;
            inline RGB24 extrapolateColor(const RGB24 &colorMin,
                                          const RGB24 &colorMax,
                                          int kNum,
                                          int kDen) const;
            inline RGB24 extrapolateColor(int xMin, int xMax,
                                          int kNumX, int kDenX,
                                          int yMin, int yMax,
                                          int kNumY, int kDenY) const;
            inline void rgbToHsl(int r, int g, int b, int *h, int *s, int *l);
            inline void hslToRgb(int h, int s, int l, int *r, int *g, int *b);
    };

    std::vector<uint8_t> initGammaTable();

    inline std::vector<uint8_t> *gammaTable() {
        static auto gammaTable = initGammaTable();

        return &gammaTable;
    }

    std::vector<uint8_t> initContrastTable();

    inline std::vector<uint8_t> *contrastTable() {
        static auto contrastTable = initContrastTable();

        return &contrastTable;
    }

    struct BmpHeader
    {
        uint32_t size;
        uint16_t reserved1;
        uint16_t reserved2;
        uint32_t offBits;
    };

    struct BmpImageHeader
    {
        uint32_t size;
        uint32_t width;
        uint32_t height;
        uint16_t planes;
        uint16_t bitCount;
        uint32_t compression;
        uint32_t sizeImage;
        uint32_t xPelsPerMeter;
        uint32_t yPelsPerMeter;
        uint32_t clrUsed;
        uint32_t clrImportant;
    };
}

AkVCam::VideoFrame::VideoFrame()
{
    this->d = new VideoFramePrivate(this);
}

AkVCam::VideoFrame::VideoFrame(const std::string &fileName)
{
    this->d = new VideoFramePrivate(this);
    this->load(fileName);
}

AkVCam::VideoFrame::VideoFrame(const AkVCam::VideoFormat &format)
{
    this->d = new VideoFramePrivate(this);
    this->d->m_format = format;

    if (format.size() > 0)
        this->d->m_data.resize(format.size());
}

AkVCam::VideoFrame::VideoFrame(const AkVCam::VideoFrame &other)
{
    this->d = new VideoFramePrivate(this);
    this->d->m_format = other.d->m_format;
    this->d->m_data = other.d->m_data;
}

AkVCam::VideoFrame &AkVCam::VideoFrame::operator =(const AkVCam::VideoFrame &other)
{
    if (this != &other) {
        this->d->m_format = other.d->m_format;
        this->d->m_data = other.d->m_data;
    }

    return *this;
}

AkVCam::VideoFrame::~VideoFrame()
{
    delete this->d;
}

// http://www.dragonwins.com/domains/getteched/bmp/bmpfileformat.htm
bool AkVCam::VideoFrame::load(const std::string &fileName)
{
    if (fileName.empty())
        return false;

    std::ifstream stream(fileName);

    if (!stream.is_open())
        return false;

    char type[2];
    stream.read(type, 2);

    if (memcmp(type, "BM", 2) != 0)
        return false;

    BmpHeader header {};
    stream.read(reinterpret_cast<char *>(&header), sizeof(BmpHeader));

    BmpImageHeader imageHeader {};
    stream.read(reinterpret_cast<char *>(&imageHeader), sizeof(BmpImageHeader));
    VideoFormat format(PixelFormatRGB24,
                       int(imageHeader.width),
                       int(imageHeader.height));

    if (format.size() < 1)
        return false;

    stream.seekg(header.offBits, std::ios_base::beg);
    this->d->m_format = format;
    this->d->m_data.resize(format.size());

    VideoData data(imageHeader.sizeImage);
    stream.read(reinterpret_cast<char *>(data.data()),
                imageHeader.sizeImage);

    switch (imageHeader.bitCount) {
    case 24: {
        VideoFormat bmpFormat(PixelFormatBGR24,
                              int(imageHeader.width),
                              int(imageHeader.height));

        for (uint32_t y = 0; y < imageHeader.height; y++) {
            auto srcLine = reinterpret_cast<const BGR24 *>
                           (data.data() + y * bmpFormat.bypl(0));
            auto dstLine = reinterpret_cast<RGB24 *>
                           (this->line(0, size_t(imageHeader.height - y - 1)));

            for (uint32_t x = 0; x < imageHeader.width; x++) {
                dstLine[x].r = srcLine[x].r;
                dstLine[x].g = srcLine[x].g;
                dstLine[x].b = srcLine[x].b;
            }
        }

        break;
    }

    case 32: {
        VideoFormat bmpFormat(PixelFormatBGR32,
                              int(imageHeader.width),
                              int(imageHeader.height));

        for (uint32_t y = 0; y < imageHeader.height; y++) {
            auto srcLine = reinterpret_cast<const BGR32 *>
                           (data.data() + y * bmpFormat.bypl(0));
            auto dstLine = reinterpret_cast<RGB24 *>
                           (this->line(0, size_t(imageHeader.height - y - 1)));

            for (uint32_t x = 0; x < imageHeader.width; x++) {
                dstLine[x].r = srcLine[x].r;
                dstLine[x].g = srcLine[x].g;
                dstLine[x].b = srcLine[x].b;
            }
        }

        break;
    }

    default:
        this->d->m_format.clear();
        this->d->m_data.clear();

        return false;
    }

    return true;
}

AkVCam::VideoFormat AkVCam::VideoFrame::format() const
{
    return this->d->m_format;
}

AkVCam::VideoFormat &AkVCam::VideoFrame::format()
{
    return this->d->m_format;
}

AkVCam::VideoData AkVCam::VideoFrame::data() const
{
    return this->d->m_data;
}

AkVCam::VideoData &AkVCam::VideoFrame::data()
{
    return this->d->m_data;
}

uint8_t *AkVCam::VideoFrame::line(size_t plane, size_t y) const
{
    return this->d->m_data.data()
            + this->d->m_format.offset(plane)
            + y * this->d->m_format.bypl(plane);
}

void AkVCam::VideoFrame::clear()
{
    this->d->m_format.clear();
    this->d->m_data.clear();
}

AkVCam::VideoFrame AkVCam::VideoFrame::mirror(bool horizontalMirror,
                                              bool verticalMirror) const
{
    if (!horizontalMirror && !verticalMirror)
        return *this;

    auto it = std::find(this->d->m_adjustFormats.begin(),
                        this->d->m_adjustFormats.end(),
                        this->d->m_format.fourcc());

    if (it == this->d->m_adjustFormats.end())
        return {};

    VideoFrame dst(this->d->m_format);
    int width = this->d->m_format.width();
    int height = this->d->m_format.height();

    if (horizontalMirror && verticalMirror) {
        for (int y = 0; y < height; y++) {
            auto srcLine = reinterpret_cast<RGB24 *>(this->line(0, size_t(height - y - 1)));
            auto dstLine = reinterpret_cast<RGB24 *>(dst.line(0, size_t(y)));

            for (int x = 0; x < width; x++)
                dstLine[x] = srcLine[width - x - 1];
        }
    } else if (horizontalMirror) {
        for (int y = 0; y < height; y++) {
            auto srcLine = reinterpret_cast<RGB24 *>(this->line(0, size_t(y)));
            auto dstLine = reinterpret_cast<RGB24 *>(dst.line(0, size_t(y)));

            for (int x = 0; x < width; x++)
                dstLine[x] = srcLine[width - x - 1];
        }
    } else if (verticalMirror) {
        for (int y = 0; y < height; y++) {
            auto srcLine = reinterpret_cast<RGB24 *>(this->line(0, size_t(height - y - 1)));
            auto dstLine = reinterpret_cast<RGB24 *>(dst.line(0, size_t(y)));
            memcpy(dstLine, srcLine, size_t(width) * sizeof(RGB24));
        }
    }

    return dst;
}

AkVCam::VideoFrame AkVCam::VideoFrame::scaled(int width,
                                              int height,
                                              Scaling mode,
                                              AspectRatio aspectRatio) const
{
    if (this->d->m_format.width() == width
        && this->d->m_format.height() == height)
        return *this;

    auto it = std::find(this->d->m_adjustFormats.begin(),
                        this->d->m_adjustFormats.end(),
                        this->d->m_format.fourcc());

    if (it == this->d->m_adjustFormats.end())
        return {};

    int xDstMin = 0;
    int yDstMin = 0;
    int xDstMax = width;
    int yDstMax = height;

    if (aspectRatio == AspectRatioKeep) {
        if (width * this->d->m_format.height()
            > this->d->m_format.width() * height) {
            // Right and left black bars
            xDstMin = (width * this->d->m_format.height()
                       - this->d->m_format.width() * height)
                       / (2 * this->d->m_format.height());
            xDstMax = (width * this->d->m_format.height()
                       + this->d->m_format.width() * height)
                       / (2 * this->d->m_format.height());
        } else if (width * this->d->m_format.height()
                   < this->d->m_format.width() * height) {
            // Top and bottom black bars
            yDstMin = (this->d->m_format.width() * height
                       - width * this->d->m_format.height())
                       / (2 * this->d->m_format.width());
            yDstMax = (this->d->m_format.width() * height
                       + width * this->d->m_format.height())
                       / (2 * this->d->m_format.width());
        }
    }

    int iWidth = this->d->m_format.width() - 1;
    int iHeight = this->d->m_format.height() - 1;
    int oWidth = xDstMax - xDstMin - 1;
    int oHeight = yDstMax - yDstMin - 1;
    int xNum = iWidth;
    int xDen = oWidth;
    int xs = 0;
    int yNum = iHeight;
    int yDen = oHeight;
    int ys = 0;

    if (aspectRatio == AspectRatioExpanding) {
        if (mode == ScalingLinear) {
            iWidth--;
            iHeight--;
            oWidth--;
            oHeight--;
        }

        if (width * this->d->m_format.height()
            < this->d->m_format.width() * height) {
            // Right and left cut
            xNum = 2 * iHeight;
            xDen = 2 * oHeight;
            xs = iWidth * oHeight - oWidth * iHeight;
        } else if (width * this->d->m_format.height()
                   > this->d->m_format.width() * height) {
            // Top and bottom cut
            yNum = 2 * iWidth;
            yDen = 2 * oWidth;
            ys = oWidth * iHeight - iWidth * oHeight;
        }
    }

    auto format = this->d->m_format;
    format.width() = width;
    format.height() = height;
    VideoFrame dst(format);

    switch (mode) {
        case ScalingFast:
            for (int y = yDstMin; y < yDstMax; y++) {
                auto srcY = (yNum * (y - yDstMin) + ys) / yDen;
                auto srcLine = reinterpret_cast<RGB24 *>(this->line(0, size_t(srcY)));
                auto dstLine = reinterpret_cast<RGB24 *>(dst.line(0, size_t(y)));

                for (int x = xDstMin; x < xDstMax; x++) {
                    auto srcX = (xNum * (x - xDstMin) + xs) / xDen;
                    dstLine[x] = srcLine[srcX];
                }
            }

            return dst;

        case ScalingLinear: {
            auto extrapolateX =
                    this->d->m_format.width() < width?
                        &VideoFramePrivate::extrapolateUp:
                        &VideoFramePrivate::extrapolateDown;
            auto extrapolateY =
                    this->d->m_format.height() < height?
                        &VideoFramePrivate::extrapolateUp:
                        &VideoFramePrivate::extrapolateDown;

            for (int y = yDstMin; y < yDstMax; y++) {
                auto dstLine = reinterpret_cast<RGB24 *>(dst.line(0, size_t(y)));
                int yMin;
                int yMax;
                int kNumY;
                int kDenY;
                extrapolateY(y - yDstMin,
                             yNum, yDen, ys,
                             &yMin, &yMax,
                             &kNumY, &kDenY);

                for (int x = xDstMin; x < xDstMax; x++) {
                    int xMin;
                    int xMax;
                    int kNumX;
                    int kDenX;
                    extrapolateX(x - xDstMin,
                                 xNum, xDen, xs,
                                 &xMin, &xMax,
                                 &kNumX, &kDenX);

                    dstLine[x] =
                            this->d->extrapolateColor(xMin, xMax,
                                                      kNumX, kDenX,
                                                      yMin, yMax,
                                                      kNumY, kDenY);
                }
            }

            return dst;
        }
    }

    return dst;
}

AkVCam::VideoFrame AkVCam::VideoFrame::scaled(size_t maxArea,
                                              AkVCam::Scaling mode,
                                              int align) const
{
    auto width = int(sqrt(double(maxArea)
                          * double(this->d->m_format.width())
                          / double(this->d->m_format.height())));
    auto height = int(sqrt(double(maxArea)
                           * double(this->d->m_format.height())
                           / double(this->d->m_format.width())));
    int owidth = align * int(width / align);
    int oheight = height * owidth / width;

    return this->scaled(owidth, oheight, mode);
}

AkVCam::VideoFrame AkVCam::VideoFrame::swapRgb(bool swap) const
{
    if (swap)
        return this->swapRgb();

    return *this;
}

AkVCam::VideoFrame AkVCam::VideoFrame::swapRgb() const
{
    auto it = std::find(this->d->m_adjustFormats.begin(),
                        this->d->m_adjustFormats.end(),
                        this->d->m_format.fourcc());

    if (it == this->d->m_adjustFormats.end())
        return {};

    VideoFrame dst(this->d->m_format);

    for (int y = 0; y < this->d->m_format.height(); y++) {
        auto srcLine = reinterpret_cast<RGB24 *>(this->line(0, size_t(y)));
        auto destLine = reinterpret_cast<RGB24 *>(dst.line(0, size_t(y)));

        for (int x = 0; x < this->d->m_format.width(); x++) {
            destLine[x].r = srcLine[x].b;
            destLine[x].g = srcLine[x].g;
            destLine[x].b = srcLine[x].r;
        }
    }

    return dst;
}

bool AkVCam::VideoFrame::canConvert(FourCC input, FourCC output) const
{
    if (input == output)
        return true;

    for (auto &convert: this->d->m_convert)
        if (convert.from == input
            && convert.to == output) {
            return true;
        }

    return false;
}

AkVCam::VideoFrame AkVCam::VideoFrame::convert(AkVCam::FourCC fourcc) const
{
    if (this->d->m_format.fourcc() == fourcc)
        return *this;

    VideoConvert *converter = nullptr;

    for (auto &convert: this->d->m_convert)
        if (convert.from == this->d->m_format.fourcc()
            && convert.to == fourcc) {
            converter = &convert;

            break;
        }

    if (!converter)
        return {};

    return converter->convert(this);
}

AkVCam::VideoFrame AkVCam::VideoFrame::adjustHsl(int hue,
                                                 int saturation,
                                                 int luminance)
{
    if (hue == 0 && saturation == 0 && luminance == 0)
        return *this;

    auto it = std::find(this->d->m_adjustFormats.begin(),
                        this->d->m_adjustFormats.end(),
                        this->d->m_format.fourcc());

    if (it == this->d->m_adjustFormats.end())
        return {};

    VideoFrame dst(this->d->m_format);

    for (int y = 0; y < this->d->m_format.height(); y++) {
        auto srcLine = reinterpret_cast<RGB24 *>(this->line(0, size_t(y)));
        auto destLine = reinterpret_cast<RGB24 *>(dst.line(0, size_t(y)));

        for (int x = 0; x < this->d->m_format.width(); x++) {
            int h;
            int s;
            int l;
            this->d->rgbToHsl(srcLine[x].r, srcLine[x].g, srcLine[x].b,
                              &h, &s, &l);

            h = this->d->mod(h + hue, 360);
            s = VideoFramePrivate::bound(0, s + saturation, 255);
            l = VideoFramePrivate::bound(0, l + luminance, 255);

            int r;
            int g;
            int b;
            this->d->hslToRgb(h, s, l, &r, &g, &b);

            destLine[x].r = uint8_t(r);
            destLine[x].g = uint8_t(g);
            destLine[x].b = uint8_t(b);
        }
    }

    return dst;
}

AkVCam::VideoFrame AkVCam::VideoFrame::adjustGamma(int gamma)
{
    if (gamma == 0)
        return *this;

    auto it = std::find(this->d->m_adjustFormats.begin(),
                        this->d->m_adjustFormats.end(),
                        this->d->m_format.fourcc());

    if (it == this->d->m_adjustFormats.end())
        return {};

    VideoFrame dst(this->d->m_format);
    auto dataGt = gammaTable()->data();
    gamma = VideoFramePrivate::bound(-255, gamma, 255);
    size_t gammaOffset = size_t(gamma + 255) << 8;

    for (int y = 0; y < this->d->m_format.height(); y++) {
        auto srcLine = reinterpret_cast<RGB24 *>(this->line(0, size_t(y)));
        auto destLine = reinterpret_cast<RGB24 *>(dst.line(0, size_t(y)));

        for (int x = 0; x < this->d->m_format.width(); x++) {
            destLine[x].r = dataGt[gammaOffset | srcLine[x].r];
            destLine[x].g = dataGt[gammaOffset | srcLine[x].g];
            destLine[x].b = dataGt[gammaOffset | srcLine[x].b];
        }
    }

    return dst;
}

AkVCam::VideoFrame AkVCam::VideoFrame::adjustContrast(int contrast)
{
    if (contrast == 0)
        return *this;

    auto it = std::find(this->d->m_adjustFormats.begin(),
                        this->d->m_adjustFormats.end(),
                        this->d->m_format.fourcc());

    if (it == this->d->m_adjustFormats.end())
        return {};

    VideoFrame dst(this->d->m_format);
    auto dataCt = contrastTable()->data();
    contrast = VideoFramePrivate::bound(-255, contrast, 255);
    size_t contrastOffset = size_t(contrast + 255) << 8;

    for (int y = 0; y < this->d->m_format.height(); y++) {
        auto srcLine = reinterpret_cast<RGB24 *>(this->line(0, size_t(y)));
        auto destLine = reinterpret_cast<RGB24 *>(dst.line(0, size_t(y)));

        for (int x = 0; x < this->d->m_format.width(); x++) {
            destLine[x].r = dataCt[contrastOffset | srcLine[x].r];
            destLine[x].g = dataCt[contrastOffset | srcLine[x].g];
            destLine[x].b = dataCt[contrastOffset | srcLine[x].b];
        }
    }

    return dst;
}

AkVCam::VideoFrame AkVCam::VideoFrame::toGrayScale()
{
    auto it = std::find(this->d->m_adjustFormats.begin(),
                        this->d->m_adjustFormats.end(),
                        this->d->m_format.fourcc());

    if (it == this->d->m_adjustFormats.end())
        return {};

    VideoFrame dst(this->d->m_format);

    for (int y = 0; y < this->d->m_format.height(); y++) {
        auto srcLine = reinterpret_cast<RGB24 *>(this->line(0, size_t(y)));
        auto destLine = reinterpret_cast<RGB24 *>(dst.line(0, size_t(y)));

        for (int x = 0; x < this->d->m_format.width(); x++) {
            int luma = this->d->grayval(srcLine[x].r,
                                        srcLine[x].g,
                                        srcLine[x].b);

            destLine[x].r = uint8_t(luma);
            destLine[x].g = uint8_t(luma);
            destLine[x].b = uint8_t(luma);
        }
    }

    return dst;
}

AkVCam::VideoFrame AkVCam::VideoFrame::adjust(int hue,
                                              int saturation,
                                              int luminance,
                                              int gamma,
                                              int contrast,
                                              bool gray)
{
    if (hue == 0
        && saturation == 0
        && luminance == 0
        && gamma == 0
        && contrast == 0
        && !gray)
        return *this;

    auto it = std::find(this->d->m_adjustFormats.begin(),
                        this->d->m_adjustFormats.end(),
                        this->d->m_format.fourcc());

    if (it == this->d->m_adjustFormats.end())
        return {};

    VideoFrame dst(this->d->m_format);
    auto dataGt = gammaTable()->data();
    auto dataCt = contrastTable()->data();

    gamma = VideoFramePrivate::bound(-255, gamma, 255);
    size_t gammaOffset = size_t(gamma + 255) << 8;

    contrast = VideoFramePrivate::bound(-255, contrast, 255);
    size_t contrastOffset = size_t(contrast + 255) << 8;

    for (int y = 0; y < this->d->m_format.height(); y++) {
        auto srcLine = reinterpret_cast<RGB24 *>(this->line(0, size_t(y)));
        auto destLine = reinterpret_cast<RGB24 *>(dst.line(0, size_t(y)));

        for (int x = 0; x < this->d->m_format.width(); x++) {
            int r = srcLine[x].r;
            int g = srcLine[x].g;
            int b = srcLine[x].b;

            if (hue != 0 || saturation != 0 ||  luminance != 0) {
                int h;
                int s;
                int l;
                this->d->rgbToHsl(r, g, b, &h, &s, &l);

                h = this->d->mod(h + hue, 360);
                s = VideoFramePrivate::bound(0, s + saturation, 255);
                l = VideoFramePrivate::bound(0, l + luminance, 255);
                this->d->hslToRgb(h, s, l, &r, &g, &b);
            }

            if (gamma != 0) {
                r = dataGt[gammaOffset | size_t(r)];
                g = dataGt[gammaOffset | size_t(g)];
                b = dataGt[gammaOffset | size_t(b)];
            }

            if (contrast != 0) {
                r = dataCt[contrastOffset | size_t(r)];
                g = dataCt[contrastOffset | size_t(g)];
                b = dataCt[contrastOffset | size_t(b)];
            }

            if (gray) {
                int luma = this->d->grayval(r, g, b);

                r = luma;
                g = luma;
                b = luma;
            }

            destLine[x].r = uint8_t(r);
            destLine[x].g = uint8_t(g);
            destLine[x].b = uint8_t(b);
        }
    }

    return dst;
}

int AkVCam::VideoFramePrivate::grayval(int r, int g, int b)
{
    return (11 * r + 16 * g + 5 * b) >> 5;
}

uint8_t AkVCam::VideoFramePrivate::rgb_y(int r, int g, int b)
{
    return uint8_t(((66 * r + 129 * g + 25 * b + 128) >> 8) + 16);
}

uint8_t AkVCam::VideoFramePrivate::rgb_u(int r, int g, int b)
{
    return uint8_t(((-38 * r - 74 * g + 112 * b + 128) >> 8) + 128);
}

uint8_t AkVCam::VideoFramePrivate::rgb_v(int r, int g, int b)
{
    return uint8_t(((112 * r - 94 * g - 18 * b + 128) >> 8) + 128);
}

uint8_t AkVCam::VideoFramePrivate::yuv_r(int y, int u, int v)
{
    UNUSED(u);
    int r = (298 * (y - 16) + 409 * (v - 128) + 128) >> 8;

    return uint8_t(bound(0, r, 255));
}

uint8_t AkVCam::VideoFramePrivate::yuv_g(int y, int u, int v)
{
    int g = (298 * (y - 16) - 100 * (u - 128) - 208 * (v - 128) + 128) >> 8;

    return uint8_t(bound(0, g, 255));
}

uint8_t AkVCam::VideoFramePrivate::yuv_b(int y, int u, int v)
{
    UNUSED(v);
    int b = (298 * (y - 16) + 516 * (u - 128) + 128) >> 8;

    return uint8_t(bound(0, b, 255));
}

AkVCam::VideoFrame AkVCam::VideoFramePrivate::bgr24_to_rgb32(const VideoFrame *src)
{
    auto format = src->format();
    format.fourcc() = PixelFormatRGB32;
    VideoFrame dst(format);
    auto width = src->format().width();
    auto height = src->format().height();

    for (int y = 0; y < height; y++) {
        auto src_line = reinterpret_cast<const BGR24 *>(src->line(0, size_t(y)));
        auto dst_line = reinterpret_cast<RGB32 *>(dst.line(0, size_t(y)));

        for (int x = 0; x < width; x++) {
            dst_line[x].x = 255;
            dst_line[x].r = src_line[x].r;
            dst_line[x].g = src_line[x].g;
            dst_line[x].b = src_line[x].b;
        }
    }

    return dst;
}

AkVCam::VideoFrame AkVCam::VideoFramePrivate::bgr24_to_rgb24(const VideoFrame *src)
{
    auto format = src->format();
    format.fourcc() = PixelFormatRGB24;
    VideoFrame dst(format);
    auto width = src->format().width();
    auto height = src->format().height();

    for (int y = 0; y < height; y++) {
        auto src_line = reinterpret_cast<const BGR24 *>(src->line(0, size_t(y)));
        auto dst_line = reinterpret_cast<RGB24 *>(dst.line(0, size_t(y)));

        for (int x = 0; x < width; x++) {
            dst_line[x].r = src_line[x].r;
            dst_line[x].g = src_line[x].g;
            dst_line[x].b = src_line[x].b;
        }
    }

    return dst;
}

AkVCam::VideoFrame AkVCam::VideoFramePrivate::bgr24_to_rgb16(const VideoFrame *src)
{
    auto format = src->format();
    format.fourcc() = PixelFormatRGB16;
    VideoFrame dst(format);
    auto width = src->format().width();
    auto height = src->format().height();

    for (int y = 0; y < height; y++) {
        auto src_line = reinterpret_cast<const BGR24 *>(src->line(0, size_t(y)));
        auto dst_line = reinterpret_cast<RGB16 *>(dst.line(0, size_t(y)));

        for (int x = 0; x < width; x++) {
            dst_line[x].r = src_line[x].r >> 3;
            dst_line[x].g = src_line[x].g >> 2;
            dst_line[x].b = src_line[x].b >> 3;
        }
    }

    return dst;
}

AkVCam::VideoFrame AkVCam::VideoFramePrivate::bgr24_to_rgb15(const VideoFrame *src)
{
    auto format = src->format();
    format.fourcc() = PixelFormatRGB15;
    VideoFrame dst(format);
    auto width = src->format().width();
    auto height = src->format().height();

    for (int y = 0; y < height; y++) {
        auto src_line = reinterpret_cast<const BGR24 *>(src->line(0, size_t(y)));
        auto dst_line = reinterpret_cast<RGB15 *>(dst.line(0, size_t(y)));

        for (int x = 0; x < width; x++) {
            dst_line[x].x = 1;
            dst_line[x].r = src_line[x].r >> 3;
            dst_line[x].g = src_line[x].g >> 3;
            dst_line[x].b = src_line[x].b >> 3;
        }
    }

    return dst;
}

AkVCam::VideoFrame AkVCam::VideoFramePrivate::bgr24_to_bgr32(const VideoFrame *src)
{
    auto format = src->format();
    format.fourcc() = PixelFormatBGR32;
    VideoFrame dst(format);
    auto width = src->format().width();
    auto height = src->format().height();

    for (int y = 0; y < height; y++) {
        auto src_line = reinterpret_cast<const BGR24 *>(src->line(0, size_t(y)));
        auto dst_line = reinterpret_cast<BGR32 *>(dst.line(0, size_t(y)));

        for (int x = 0; x < width; x++) {
            dst_line[x].x = 255;
            dst_line[x].r = src_line[x].r;
            dst_line[x].g = src_line[x].g;
            dst_line[x].b = src_line[x].b;
        }
    }

    return dst;
}

AkVCam::VideoFrame AkVCam::VideoFramePrivate::bgr24_to_bgr16(const VideoFrame *src)
{
    auto format = src->format();
    format.fourcc() = PixelFormatBGR16;
    VideoFrame dst(format);
    auto width = src->format().width();
    auto height = src->format().height();

    for (int y = 0; y < height; y++) {
        auto src_line = reinterpret_cast<const BGR24 *>(src->line(0, size_t(y)));
        auto dst_line = reinterpret_cast<BGR16 *>(dst.line(0, size_t(y)));

        for (int x = 0; x < width; x++) {
            dst_line[x].r = src_line[x].r >> 3;
            dst_line[x].g = src_line[x].g >> 2;
            dst_line[x].b = src_line[x].b >> 3;
        }
    }

    return dst;
}

AkVCam::VideoFrame AkVCam::VideoFramePrivate::bgr24_to_bgr15(const VideoFrame *src)
{
    auto format = src->format();
    format.fourcc() = PixelFormatBGR15;
    VideoFrame dst(format);
    auto width = src->format().width();
    auto height = src->format().height();

    for (int y = 0; y < height; y++) {
        auto src_line = reinterpret_cast<const BGR24 *>(src->line(0, size_t(y)));
        auto dst_line = reinterpret_cast<BGR15 *>(dst.line(0, size_t(y)));

        for (int x = 0; x < width; x++) {
            dst_line[x].x = 1;
            dst_line[x].r = src_line[x].r >> 3;
            dst_line[x].g = src_line[x].g >> 3;
            dst_line[x].b = src_line[x].b >> 3;
        }
    }

    return dst;
}

AkVCam::VideoFrame AkVCam::VideoFramePrivate::bgr24_to_uyvy(const VideoFrame *src)
{
    auto format = src->format();
    format.fourcc() = PixelFormatUYVY;
    VideoFrame dst(format);
    auto width = src->format().width();
    auto height = src->format().height();

    for (int y = 0; y < height; y++) {
        auto src_line = reinterpret_cast<const BGR24 *>(src->line(0, size_t(y)));
        auto dst_line = reinterpret_cast<UYVY *>(dst.line(0, size_t(y)));

        for (int x = 0; x < width; x++) {
            auto x_yuv = x / 2;

            auto r0 = src_line[x].r;
            auto g0 = src_line[x].g;
            auto b0 = src_line[x].b;

            x++;

            int r1 = src_line[x].r;
            int g1 = src_line[x].g;
            int b1 = src_line[x].b;

            dst_line[x_yuv].u0 = rgb_u(r0, g0, b0);
            dst_line[x_yuv].y0 = rgb_y(r0, g0, b0);
            dst_line[x_yuv].v0 = rgb_v(r0, g0, b0);
            dst_line[x_yuv].y1 = rgb_y(r1, g1, b1);
        }
    }

    return dst;
}

AkVCam::VideoFrame AkVCam::VideoFramePrivate::bgr24_to_yuy2(const VideoFrame *src)
{
    auto format = src->format();
    format.fourcc() = PixelFormatYUY2;
    VideoFrame dst(format);
    auto width = src->format().width();
    auto height = src->format().height();

    for (int y = 0; y < height; y++) {
        auto src_line = reinterpret_cast<const BGR24 *>(src->line(0, size_t(y)));
        auto dst_line = reinterpret_cast<YUY2 *>(dst.line(0, size_t(y)));

        for (int x = 0; x < width; x++) {
            auto x_yuv = x / 2;

            auto r0 = src_line[x].r;
            auto g0 = src_line[x].g;
            auto b0 = src_line[x].b;

            x++;

            auto r1 = src_line[x].r;
            auto g1 = src_line[x].g;
            auto b1 = src_line[x].b;

            dst_line[x_yuv].y0 = rgb_y(r0, g0, b0);
            dst_line[x_yuv].u0 = rgb_u(r0, g0, b0);
            dst_line[x_yuv].y1 = rgb_y(r1, g1, b1);
            dst_line[x_yuv].v0 = rgb_v(r0, g0, b0);
        }
    }

    return dst;
}

AkVCam::VideoFrame AkVCam::VideoFramePrivate::bgr24_to_nv12(const VideoFrame *src)
{
    auto format = src->format();
    format.fourcc() = PixelFormatNV12;
    VideoFrame dst(format);
    auto width = src->format().width();
    auto height = src->format().height();

    for (int y = 0; y < height; y++) {
        auto src_line = reinterpret_cast<const BGR24 *>(src->line(0, size_t(y)));
        auto dst_line_y = dst.line(0, size_t(y));
        auto dst_line_vu = reinterpret_cast<VU *>(dst.line(1, size_t(y) / 2));

        for (int x = 0; x < width; x++) {
            auto r = src_line[x].r;
            auto g = src_line[x].g;
            auto b = src_line[x].b;

            dst_line_y[y] = rgb_y(r, g, b);

            if (!(x & 0x1) && !(y & 0x1)) {
                dst_line_vu[x / 2].v = rgb_v(r, g, b);
                dst_line_vu[x / 2].u = rgb_u(r, g, b);
            }
        }
    }

    return dst;
}

AkVCam::VideoFrame AkVCam::VideoFramePrivate::bgr24_to_nv21(const VideoFrame *src)
{
    auto format = src->format();
    format.fourcc() = PixelFormatNV21;
    VideoFrame dst(format);
    auto width = src->format().width();
    auto height = src->format().height();

    for (int y = 0; y < height; y++) {
        auto src_line = reinterpret_cast<const BGR24 *>(src->line(0, size_t(y)));
        auto dst_line_y = dst.line(0, size_t(y));
        auto dst_line_vu = reinterpret_cast<UV *>(dst.line(1, size_t(y) / 2));

        for (int x = 0; x < width; x++) {
            auto r = src_line[x].r;
            auto g = src_line[x].g;
            auto b = src_line[x].b;

            dst_line_y[y] = rgb_y(r, g, b);

            if (!(x & 0x1) && !(y & 0x1)) {
                dst_line_vu[x / 2].v = rgb_v(r, g, b);
                dst_line_vu[x / 2].u = rgb_u(r, g, b);
            }
        }
    }

    return dst;
}

AkVCam::VideoFrame AkVCam::VideoFramePrivate::rgb24_to_rgb32(const VideoFrame *src)
{
    auto format = src->format();
    format.fourcc() = PixelFormatRGB32;
    VideoFrame dst(format);
    auto width = src->format().width();
    auto height = src->format().height();

    for (int y = 0; y < height; y++) {
        auto src_line = reinterpret_cast<const RGB24 *>(src->line(0, size_t(y)));
        auto dst_line = reinterpret_cast<RGB32 *>(dst.line(0, size_t(y)));

        for (int x = 0; x < width; x++) {
            dst_line[x].x = 255;
            dst_line[x].r = src_line[x].r;
            dst_line[x].g = src_line[x].g;
            dst_line[x].b = src_line[x].b;
        }
    }

    return dst;
}

AkVCam::VideoFrame AkVCam::VideoFramePrivate::rgb24_to_rgb16(const VideoFrame *src)
{
    auto format = src->format();
    format.fourcc() = PixelFormatRGB16;
    VideoFrame dst(format);
    auto width = src->format().width();
    auto height = src->format().height();

    for (int y = 0; y < height; y++) {
        auto src_line = reinterpret_cast<const RGB24 *>(src->line(0, size_t(y)));
        auto dst_line = reinterpret_cast<RGB16 *>(dst.line(0, size_t(y)));

        for (int x = 0; x < width; x++) {
            dst_line[x].r = src_line[x].r >> 3;
            dst_line[x].g = src_line[x].g >> 2;
            dst_line[x].b = src_line[x].b >> 3;
        }
    }

    return dst;
}

AkVCam::VideoFrame AkVCam::VideoFramePrivate::rgb24_to_rgb15(const VideoFrame *src)
{
    auto format = src->format();
    format.fourcc() = PixelFormatRGB15;
    VideoFrame dst(format);
    auto width = src->format().width();
    auto height = src->format().height();

    for (int y = 0; y < height; y++) {
        auto src_line = reinterpret_cast<const RGB24 *>(src->line(0, size_t(y)));
        auto dst_line = reinterpret_cast<RGB15 *>(dst.line(0, size_t(y)));

        for (int x = 0; x < width; x++) {
            dst_line[x].x = 1;
            dst_line[x].r = src_line[x].r >> 3;
            dst_line[x].g = src_line[x].g >> 3;
            dst_line[x].b = src_line[x].b >> 3;
        }
    }

    return dst;
}

AkVCam::VideoFrame AkVCam::VideoFramePrivate::rgb24_to_bgr32(const VideoFrame *src)
{
    auto format = src->format();
    format.fourcc() = PixelFormatBGR32;
    VideoFrame dst(format);
    auto width = src->format().width();
    auto height = src->format().height();

    for (int y = 0; y < height; y++) {
        auto src_line = reinterpret_cast<const RGB24 *>(src->line(0, size_t(y)));
        auto dst_line = reinterpret_cast<BGR32 *>(dst.line(0, size_t(y)));

        for (int x = 0; x < width; x++) {
            dst_line[x].x = 255;
            dst_line[x].r = src_line[x].r;
            dst_line[x].g = src_line[x].g;
            dst_line[x].b = src_line[x].b;
        }
    }

    return dst;
}

AkVCam::VideoFrame AkVCam::VideoFramePrivate::rgb24_to_bgr24(const VideoFrame *src)
{
    auto format = src->format();
    format.fourcc() = PixelFormatBGR24;
    VideoFrame dst(format);
    auto width = src->format().width();
    auto height = src->format().height();

    for (int y = 0; y < height; y++) {
        auto src_line = reinterpret_cast<const RGB24 *>(src->line(0, size_t(y)));
        auto dst_line = reinterpret_cast<BGR24 *>(dst.line(0, size_t(y)));

        for (int x = 0; x < width; x++) {
            dst_line[x].r = src_line[x].r;
            dst_line[x].g = src_line[x].g;
            dst_line[x].b = src_line[x].b;
        }
    }

    return dst;
}

AkVCam::VideoFrame AkVCam::VideoFramePrivate::rgb24_to_bgr16(const VideoFrame *src)
{
    auto format = src->format();
    format.fourcc() = PixelFormatBGR16;
    VideoFrame dst(format);
    auto width = src->format().width();
    auto height = src->format().height();

    for (int y = 0; y < height; y++) {
        auto src_line = reinterpret_cast<const RGB24 *>(src->line(0, size_t(y)));
        auto dst_line = reinterpret_cast<BGR16 *>(dst.line(0, size_t(y)));

        for (int x = 0; x < width; x++) {
            dst_line[x].r = src_line[x].r >> 3;
            dst_line[x].g = src_line[x].g >> 2;
            dst_line[x].b = src_line[x].b >> 3;
        }
    }

    return dst;
}

AkVCam::VideoFrame AkVCam::VideoFramePrivate::rgb24_to_bgr15(const VideoFrame *src)
{
    auto format = src->format();
    format.fourcc() = PixelFormatBGR15;
    VideoFrame dst(format);
    auto width = src->format().width();
    auto height = src->format().height();

    for (int y = 0; y < height; y++) {
        auto src_line = reinterpret_cast<const RGB24 *>(src->line(0, size_t(y)));
        auto dst_line = reinterpret_cast<BGR15 *>(dst.line(0, size_t(y)));

        for (int x = 0; x < width; x++) {
            dst_line[x].x = 1;
            dst_line[x].r = src_line[x].r >> 3;
            dst_line[x].g = src_line[x].g >> 3;
            dst_line[x].b = src_line[x].b >> 3;
        }
    }

    return dst;
}

AkVCam::VideoFrame AkVCam::VideoFramePrivate::rgb24_to_uyvy(const VideoFrame *src)
{
    auto format = src->format();
    format.fourcc() = PixelFormatUYVY;
    VideoFrame dst(format);
    auto width = src->format().width();
    auto height = src->format().height();

    for (int y = 0; y < height; y++) {
        auto src_line = reinterpret_cast<const RGB24 *>(src->line(0, size_t(y)));
        auto dst_line = reinterpret_cast<UYVY *>(dst.line(0, size_t(y)));

        for (int x = 0; x < width; x++) {
            auto x_yuv = x / 2;

            auto r0 = src_line[x].r;
            auto g0 = src_line[x].g;
            auto b0 = src_line[x].b;

            x++;

            int r1 = src_line[x].r;
            int g1 = src_line[x].g;
            int b1 = src_line[x].b;

            dst_line[x_yuv].u0 = rgb_u(r0, g0, b0);
            dst_line[x_yuv].y0 = rgb_y(r0, g0, b0);
            dst_line[x_yuv].v0 = rgb_v(r0, g0, b0);
            dst_line[x_yuv].y1 = rgb_y(r1, g1, b1);
        }
    }

    return dst;
}

AkVCam::VideoFrame AkVCam::VideoFramePrivate::rgb24_to_yuy2(const VideoFrame *src)
{
    auto format = src->format();
    format.fourcc() = PixelFormatYUY2;
    VideoFrame dst(format);
    auto width = src->format().width();
    auto height = src->format().height();

    for (int y = 0; y < height; y++) {
        auto src_line = reinterpret_cast<const RGB24 *>(src->line(0, size_t(y)));
        auto dst_line = reinterpret_cast<YUY2 *>(dst.line(0, size_t(y)));

        for (int x = 0; x < width; x++) {
            auto x_yuv = x / 2;

            auto r0 = src_line[x].r;
            auto g0 = src_line[x].g;
            auto b0 = src_line[x].b;

            x++;

            auto r1 = src_line[x].r;
            auto g1 = src_line[x].g;
            auto b1 = src_line[x].b;

            dst_line[x_yuv].y0 = rgb_y(r0, g0, b0);
            dst_line[x_yuv].u0 = rgb_u(r0, g0, b0);
            dst_line[x_yuv].y1 = rgb_y(r1, g1, b1);
            dst_line[x_yuv].v0 = rgb_v(r0, g0, b0);
        }
    }

    return dst;
}

AkVCam::VideoFrame AkVCam::VideoFramePrivate::rgb24_to_nv12(const VideoFrame *src)
{
    auto format = src->format();
    format.fourcc() = PixelFormatNV12;
    VideoFrame dst(format);
    auto width = src->format().width();
    auto height = src->format().height();

    for (int y = 0; y < height; y++) {
        auto src_line = reinterpret_cast<const RGB24 *>(src->line(0, size_t(y)));
        auto dst_line_y = dst.line(0, size_t(y));
        auto dst_line_vu = reinterpret_cast<VU *>(dst.line(1, size_t(y) / 2));

        for (int x = 0; x < width; x++) {
            auto r = src_line[x].r;
            auto g = src_line[x].g;
            auto b = src_line[x].b;

            dst_line_y[y] = rgb_y(r, g, b);

            if (!(x & 0x1) && !(y & 0x1)) {
                dst_line_vu[x / 2].v = rgb_v(r, g, b);
                dst_line_vu[x / 2].u = rgb_u(r, g, b);
            }
        }
    }

    return dst;
}

AkVCam::VideoFrame AkVCam::VideoFramePrivate::rgb24_to_nv21(const VideoFrame *src)
{
    auto format = src->format();
    format.fourcc() = PixelFormatNV21;
    VideoFrame dst(format);
    auto width = src->format().width();
    auto height = src->format().height();

    for (int y = 0; y < height; y++) {
        auto src_line = reinterpret_cast<const RGB24 *>(src->line(0, size_t(y)));
        auto dst_line_y = dst.line(0, size_t(y));
        auto dst_line_vu = reinterpret_cast<UV *>(dst.line(1, size_t(y) / 2));

        for (int x = 0; x < width; x++) {
            auto r = src_line[x].r;
            auto g = src_line[x].g;
            auto b = src_line[x].b;

            dst_line_y[y] = rgb_y(r, g, b);

            if (!(x & 0x1) && !(y & 0x1)) {
                dst_line_vu[x / 2].v = rgb_v(r, g, b);
                dst_line_vu[x / 2].u = rgb_u(r, g, b);
            }
        }
    }

    return dst;
}

void AkVCam::VideoFramePrivate::extrapolateUp(int dstCoord,
                                              int num, int den, int s,
                                              int *srcCoordMin, int *srcCoordMax,
                                              int *kNum, int *kDen)
{
    *srcCoordMin = (num * dstCoord + s) / den;
    *srcCoordMax = *srcCoordMin + 1;
    int dstCoordMin = (den * *srcCoordMin - s) / num;
    int dstCoordMax = (den * *srcCoordMax - s) / num;
    *kNum = dstCoord - dstCoordMin;
    *kDen = dstCoordMax - dstCoordMin;
}

void AkVCam::VideoFramePrivate::extrapolateDown(int dstCoord,
                                                int num, int den, int s,
                                                int *srcCoordMin, int *srcCoordMax,
                                                int *kNum, int *kDen)
{
    *srcCoordMin = (num * dstCoord + s) / den;
    *srcCoordMax = *srcCoordMin;
    *kNum = 0;
    *kDen = 1;
}

uint8_t AkVCam::VideoFramePrivate::extrapolateComponent(uint8_t min, uint8_t max,
                                                        int kNum, int kDen) const
{
    return uint8_t((kNum * (max - min) + kDen * min) / kDen);
}

AkVCam::RGB24 AkVCam::VideoFramePrivate::extrapolateColor(const RGB24 &colorMin,
                                                          const RGB24 &colorMax,
                                                          int kNum,
                                                          int kDen) const
{
    return RGB24 {
        extrapolateComponent(colorMin.b, colorMax.b, kNum, kDen),
        extrapolateComponent(colorMin.g, colorMax.g, kNum, kDen),
        extrapolateComponent(colorMin.r, colorMax.r, kNum, kDen)
    };
}

AkVCam::RGB24 AkVCam::VideoFramePrivate::extrapolateColor(int xMin, int xMax,
                                                          int kNumX, int kDenX,
                                                          int yMin, int yMax,
                                                          int kNumY, int kDenY) const
{
    auto minLine = reinterpret_cast<RGB24 *>(this->self->line(0, size_t(yMin)));
    auto maxLine = reinterpret_cast<RGB24 *>(this->self->line(0, size_t(yMax)));
    auto colorMin = extrapolateColor(minLine[xMin], minLine[xMax], kNumX, kDenX);
    auto colorMax = extrapolateColor(maxLine[xMin], maxLine[xMax], kNumX, kDenX);

    return extrapolateColor(colorMin, colorMax, kNumY, kDenY);
}

// https://en.wikipedia.org/wiki/HSL_and_HSV
void AkVCam::VideoFramePrivate::rgbToHsl(int r, int g, int b, int *h, int *s, int *l)
{
    int max = std::max(r, std::max(g, b));
    int min = std::min(r, std::min(g, b));
    int c = max - min;

    *l = (max + min) / 2;

    if (!c) {
        *h = 0;
        *s = 0;
    } else {
        if (max == r)
            *h = this->mod(g - b, 6 * c);
        else if (max == g)
            *h = b - r + 2 * c;
        else
            *h = r - g + 4 * c;

        *h = 60 * (*h) / c;
        *s = 255 * c / (255 - abs(max + min - 255));
    }
}

void AkVCam::VideoFramePrivate::hslToRgb(int h, int s, int l, int *r, int *g, int *b)
{
    int c = s * (255 - abs(2 * l - 255)) / 255;
    int x = c * (60 - abs((h % 120) - 60)) / 60;

    if (h >= 0 && h < 60) {
        *r = c;
        *g = x;
        *b = 0;
    } else if (h >= 60 && h < 120) {
        *r = x;
        *g = c;
        *b = 0;
    } else if (h >= 120 && h < 180) {
        *r = 0;
        *g = c;
        *b = x;
    } else if (h >= 180 && h < 240) {
        *r = 0;
        *g = x;
        *b = c;
    } else if (h >= 240 && h < 300) {
        *r = x;
        *g = 0;
        *b = c;
    } else if (h >= 300 && h < 360) {
        *r = c;
        *g = 0;
        *b = x;
    } else {
        *r = 0;
        *g = 0;
        *b = 0;
    }

    int m = 2 * l - c;

    *r = (2 * (*r) + m) / 2;
    *g = (2 * (*g) + m) / 2;
    *b = (2 * (*b) + m) / 2;
}

std::vector<uint8_t> AkVCam::initGammaTable()
{
    std::vector<uint8_t> gammaTable;

    for (int i = 0; i < 256; i++) {
        auto ig = uint8_t(255. * pow(i / 255., 255));
        gammaTable.push_back(ig);
    }

    for (int gamma = -254; gamma < 256; gamma++) {
        double k = 255. / (gamma + 255);

        for (int i = 0; i < 256; i++) {
            auto ig = uint8_t(255. * pow(i / 255., k));
            gammaTable.push_back(ig);
        }
    }

    return gammaTable;
}

std::vector<uint8_t> AkVCam::initContrastTable()
{
    std::vector<uint8_t> contrastTable;

    for (int contrast = -255; contrast < 256; contrast++) {
        double f = 259. * (255 + contrast) / (255. * (259 - contrast));

        for (int i = 0; i < 256; i++) {
            int ic = int(f * (i - 128) + 128.);
            contrastTable.push_back(uint8_t(VideoFramePrivate::bound(0, ic, 255)));
        }
    }

    return contrastTable;
}
