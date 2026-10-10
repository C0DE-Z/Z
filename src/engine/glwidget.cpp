#include "glwidget.h"
#include <QOpenGLFramebufferObjectFormat>
#include <QApplication>
#include <QCursor>
#include <QPainter>
#include <QPaintEvent>
#include <QPainterPath>
#include <QPainterPathStroker>
#include <QStringList>
#include <cmath>
#include <algorithm>
#include <QDebug>
#include <QFile>
#include <QIODevice>
#include <cstring>
#include "engine/audioengine.h"
#include "../utils/profiler.h"

#include "engine/shaders.h"
#include "engine/maskgenerator.h"

GLWidget::GLWidget(QWidget* parent) : QOpenGLWidget(parent) {
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setStyleSheet("background: transparent;");

    QSurfaceFormat format;
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::NoProfile);
    format.setAlphaBufferSize(8);
    format.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
    setFormat(format);
    fpsTimer.start();
    setAutoFillBackground(false);
    setMouseTracking(true);
    overlayLabel = new QLabel(this);
    overlayLabel->setStyleSheet(
        "QLabel {"
        "  color: #FFB8D2;"
        "  background-color: rgba(8, 8, 10, 220);"
        "  border: 1px solid #80324F;"
        "  font-family: 'JetBrains Mono', Consolas, monospace;"
        "  font-size: 11px;"
        "  font-weight: bold;"
        "  padding: 4px 8px;"
        "  border-radius: 4px;"
        "}"
    );
    overlayLabel->move(8, 8);
    overlayLabel->setText("FPS: --\nRes: --");
    overlayLabel->adjustSize();
    overlayLabel->show();
}

GLWidget::~GLWidget() {
    makeCurrent();
    if (videoTexture) glDeleteTextures(1, &videoTexture);
    if (videoTexture2) glDeleteTextures(1, &videoTexture2);
    if (baseTexture) glDeleteTextures(1, &baseTexture);
    if (maskTexture) glDeleteTextures(1, &maskTexture);
    glDeleteBuffers(static_cast<GLsizei>(uploadPbos.size()), uploadPbos.data());
    quadVao.destroy();
    quadVbo.destroy();
    delete passthroughShader;
    delete transparencyGridShader;
    delete maskCompositeShader;
    delete alphaGuardShader;
    delete exportBlitShader;
    delete fboPing;
    delete fboPong;
    delete fboFeedback;
    delete fboMask;
    delete exportFbo;
    doneCurrent();
}

void GLWidget::initializeGL() {
    initializeOpenGLFunctions();

    // QOpenGLWidget can recreate its context. Plugin shader program IDs from
    // the previous context become invalid and must be rebuilt, otherwise
    // effects silently render as no-op while incurring per-frame GL overhead.
    for (auto* shader : findChildren<QOpenGLShaderProgram*>()) {
        delete shader;
    }
    passthroughShader = nullptr;
    transparencyGridShader = nullptr;
    maskCompositeShader = nullptr;
    alphaGuardShader = nullptr;
    exportBlitShader = nullptr;
    uniformLocations.clear();
    for (auto& plugin : PluginManager::instance().getPlugins()) {
        plugin.compileAttempted = false;
        plugin.isCompiled = false;
        plugin.shaderProgram = 0;
    }

    glClearColor(0.04f, 0.04f, 0.06f, 0.0f);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    initShaders();

    static GLfloat vertices[] = {
        -1.0f, -1.0f,
         1.0f, -1.0f,
        -1.0f,  1.0f,
         1.0f,  1.0f
    };
    quadVbo.create();
    quadVbo.bind();
    quadVbo.allocate(vertices, sizeof(vertices));

    quadVao.create();
    quadVao.bind();
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    quadVao.release();

    quadVbo.release();

    glGenTextures(1, &videoTexture);
    glBindTexture(GL_TEXTURE_2D, videoTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenTextures(1, &baseTexture);
    glBindTexture(GL_TEXTURE_2D, baseTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenTextures(1, &videoTexture2);
    glBindTexture(GL_TEXTURE_2D, videoTexture2);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    unsigned char blackPixel[4] = { 0, 0, 0, 255 };
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, blackPixel);
    glBindTexture(GL_TEXTURE_2D, videoTexture2);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, blackPixel);

    glGenTextures(1, &maskTexture);
    glBindTexture(GL_TEXTURE_2D, maskTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    unsigned char whitePixel = 255;
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, 1, 1, 0, GL_RED, GL_UNSIGNED_BYTE, &whitePixel);
    hasMaskTexture = false;

    // Triple-buffered unpack buffers let the driver DMA a prior frame while the
    // CPU prepares the next one, avoiding the usual glTexSubImage2D stall.
    glGenBuffers(static_cast<GLsizei>(uploadPbos.size()), uploadPbos.data());
}

void GLWidget::initShaders() {
    passthroughShader = new QOpenGLShaderProgram(this);
    passthroughShader->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShaderSource);
    passthroughShader->addShaderFromSourceCode(QOpenGLShader::Fragment, passthroughShaderSource);
    if (!passthroughShader->link()) {
        qWarning() << "Failed to link passthrough shader:" << passthroughShader->log();
    }

    transparencyGridShader = new QOpenGLShaderProgram(this);
    transparencyGridShader->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShaderSource);
    transparencyGridShader->addShaderFromSourceCode(QOpenGLShader::Fragment, transparencyGridShaderSource);
    if (!transparencyGridShader->link()) {
        qWarning() << "Failed to link transparency grid shader:" << transparencyGridShader->log();
    }

    maskCompositeShader = new QOpenGLShaderProgram(this);
    maskCompositeShader->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShaderSource);
    maskCompositeShader->addShaderFromSourceCode(QOpenGLShader::Fragment, maskCompositeShaderSource);
    if (!maskCompositeShader->link()) {
        qWarning() << "Failed to link mask composite shader:" << maskCompositeShader->log();
    }

    alphaGuardShader = new QOpenGLShaderProgram(this);
    alphaGuardShader->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShaderSource);
    alphaGuardShader->addShaderFromSourceCode(QOpenGLShader::Fragment, alphaGuardShaderSource);
    if (!alphaGuardShader->link()) {
        qWarning() << "Failed to link alpha guard shader:" << alphaGuardShader->log();
    }

    exportBlitShader = new QOpenGLShaderProgram(this);
    exportBlitShader->addShaderFromSourceCode(QOpenGLShader::Vertex, exportVertexShaderSource);
    exportBlitShader->addShaderFromSourceCode(QOpenGLShader::Fragment, passthroughShaderSource);
    if (!exportBlitShader->link()) {
        qWarning() << "Failed to link export blit shader:" << exportBlitShader->log();
    }

}

void GLWidget::allocateFBOs(int w, int h) {
    if (w <= 0 || h <= 0) return;

    if (fboPing && fboPing->width() == w && fboPing->height() == h) {
        return;
    }

    delete fboPing;
    delete fboPong;
    delete fboFeedback;
    delete fboMask;
    delete exportFbo;

    QOpenGLFramebufferObjectFormat format;
    format.setAttachment(QOpenGLFramebufferObject::NoAttachment);
    format.setInternalTextureFormat(GL_RGBA);

    fboPing = new QOpenGLFramebufferObject(w, h, format);
    fboPong = new QOpenGLFramebufferObject(w, h, format);
    fboFeedback = new QOpenGLFramebufferObject(w, h, format);
    fboMask = new QOpenGLFramebufferObject(w, h, format);
    exportFbo = new QOpenGLFramebufferObject(w, h, format);

    for (QOpenGLFramebufferObject* fbo : { fboPing, fboPong, fboFeedback, fboMask, exportFbo }) {
        if (!fbo) continue;
        fbo->bind();
        glViewport(0, 0, w, h);
        glDisable(GL_BLEND);
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        fbo->release();
    }
}

QImage GLWidget::grabRenderedFrame() {
    if (!renderedTexture || width() <= 0 || height() <= 0) {
        return {};
    }

    makeCurrent();
    if (!exportFbo || exportFbo->width() != width() || exportFbo->height() != height()) {
        delete exportFbo;
        QOpenGLFramebufferObjectFormat format;
        format.setAttachment(QOpenGLFramebufferObject::NoAttachment);
        format.setInternalTextureFormat(GL_RGBA);
        exportFbo = new QOpenGLFramebufferObject(width(), height(), format);
    }

    exportFbo->bind();
    glViewport(0, 0, width(), height());
    glDisable(GL_BLEND);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    passthroughShader->bind();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, renderedTexture);
    passthroughShader->setUniformValue("videoTexture", 0);
    renderQuad();
    passthroughShader->release();

    QImage image(width(), height(), QImage::Format_RGBA8888);
    glReadPixels(0, 0, width(), height(), GL_RGBA, GL_UNSIGNED_BYTE, image.bits());
    exportFbo->release();
    doneCurrent();
    return image.mirrored(false, true);
}

void GLWidget::renderFrameNow() {
    if (!context() || !passthroughShader || !passthroughShader->isLinked()) return;
    makeCurrent();
    paintGL();
    doneCurrent();
}

bool GLWidget::renderExportFrame(int targetW, int targetH, std::vector<uint8_t>& outRgbaBuffer) {
    if (!context() || !passthroughShader || !passthroughShader->isLinked() || targetW <= 0 || targetH <= 0) {
        return false;
    }
    const size_t totalBytes = static_cast<size_t>(targetW) * static_cast<size_t>(targetH) * 4;
    if (outRgbaBuffer.size() != totalBytes) {
        outRgbaBuffer.resize(totalBytes);
    }

    makeCurrent();
    // Use the same texture orientation and effect path as preview. The output
    // framebuffer is only a readback surface; the exporter performs the one
    // required vertical flip for OpenGL's bottom-up glReadPixels rows.
    renderPipeline(targetW, targetH, false, false);

    if (!exportFbo || !renderedTexture) {
        doneCurrent();
        return false;
    }

    exportFbo->bind();
    glViewport(0, 0, targetW, targetH);
    glDisable(GL_BLEND);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    passthroughShader->bind();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, renderedTexture);
    passthroughShader->setUniformValue("videoTexture", 0);
    renderQuad();
    passthroughShader->release();
    glReadPixels(0, 0, targetW, targetH, GL_RGBA, GL_UNSIGNED_BYTE, outRgbaBuffer.data());
    exportFbo->release();

    // glReadPixels returns rows bottom-up. Normalize once here so FFmpeg gets
    // ordinary top-down RGBA and the export vertex shader cannot add a second
    // vertical flip relative to the preview render path.
    const size_t rowBytes = static_cast<size_t>(targetW) * 4;
    std::vector<uint8_t> rowBuffer(rowBytes);
    for (int y = 0; y < targetH / 2; ++y) {
        uint8_t* top = outRgbaBuffer.data() + static_cast<size_t>(y) * rowBytes;
        uint8_t* bottom = outRgbaBuffer.data() + static_cast<size_t>(targetH - 1 - y) * rowBytes;
        std::memcpy(rowBuffer.data(), top, rowBytes);
        std::memcpy(top, bottom, rowBytes);
        std::memcpy(bottom, rowBuffer.data(), rowBytes);
    }

    if (m_showDetections && !detections.empty()) {
        QImage image(outRgbaBuffer.data(), targetW, targetH, static_cast<int>(rowBytes), QImage::Format_RGBA8888);
        QPainter painter(&image);
        drawDetectionExportOverlay(painter, targetW, targetH);
        painter.end();
    }
    doneCurrent();
    return true;
}

void GLWidget::resizeGL(int w, int h) {
    glViewport(0, 0, w, h);
    allocateFBOs(w, h);
}

void GLWidget::updateFrame(const DecodedVideoFrame& frame) {
    std::lock_guard<std::mutex> lock(m_frameMutex);
    sharedCurrentFrame.reset();
    currentFrame = frame;
    hasNewFrame = true;
    isTransitioning = false;
    update();
}

void GLWidget::updateFrame(DecodedVideoFrame&& frame) {
    std::lock_guard<std::mutex> lock(m_frameMutex);
    sharedCurrentFrame.reset();
    currentFrame = std::move(frame);
    hasNewFrame = true;
    isTransitioning = false;
    update();
}


void GLWidget::updateFrame(std::shared_ptr<const DecodedVideoFrame> frame) {
    if (!frame) return;
    std::lock_guard<std::mutex> lock(m_frameMutex);
    if (sharedCurrentFrame == frame && !isTransitioning) {
        // Same decoded frame: effects still animate, but the texture is current.
        update();
        return;
    }
    sharedCurrentFrame = std::move(frame);
    currentFrame = {};
    hasNewFrame = true;
    isTransitioning = false;
    update();
}

void GLWidget::updateTransitionFrames(const DecodedVideoFrame& f1, const DecodedVideoFrame& f2, double progress, const std::string& pluginId) {
    std::lock_guard<std::mutex> lock(m_frameMutex);
    sharedCurrentFrame.reset();
    transitionFrame1 = f1;
    transitionFrame2 = f2;
    transitionProgress = progress;
    currentTransitionPlugin = pluginId;
    isTransitioning = true;
    hasNewFrame = true;
    update();
}
void GLWidget::clearFrame() {
    std::lock_guard<std::mutex> lock(m_frameMutex);
    sharedCurrentFrame.reset();
    int w = lastFrameWidth > 0 ? lastFrameWidth : 1280;
    int h = lastFrameHeight > 0 ? lastFrameHeight : 720;
    currentFrame.rgbData.assign(w * h * 3, 0);
    currentFrame.alphaData.assign(w * h, 0);
    currentFrame.width = w;
    currentFrame.height = h;
    currentFrame.hasAlpha = false;
    hasNewFrame = true;
    update();
}

void GLWidget::setPlaybackTime(double time) {
    m_time = time;
}

void GLWidget::setActiveEffects(const std::vector<AppliedEffect>& effects) {
    activeEffects = effects;
}

void GLWidget::setShowOverlay(bool show) {
    showOverlay = show;
    if (overlayLabel) overlayLabel->setVisible(show);
}

void GLWidget::setAsyncTextureUploads(bool enabled) {
    m_asyncTextureUploads = enabled;
}

void GLWidget::setRendererBackend(RenderBackendKind backend) {
    m_rendererBackend = backend;
    m_asyncTextureUploads = renderBackendUsesPbo(backend);
    update();
}

void GLWidget::uploadPrimaryVideoTexture(const DecodedVideoFrame& frame) {
    if (frame.rgbData.empty() || frame.width <= 0 || frame.height <= 0) return;
    const bool resized = lastFrameWidth != frame.width || lastFrameHeight != frame.height;

    std::vector<uint8_t> rgbaData;
    const uint8_t* sourceData = frame.rgbData.data();
    const size_t pixelCount = static_cast<size_t>(frame.width) * static_cast<size_t>(frame.height);
    const bool needsAlpha = frame.hasAlpha && !frame.alphaData.empty() && frame.alphaData.size() == pixelCount;
    // The upload format is part of the texture allocation. Submitting RGBA
    // pixels to an existing RGB allocation (or vice versa) is invalid OpenGL
    // and was the reason alpha sometimes appeared as black after clip changes.
    const bool formatChanged = lastFrameHasAlpha != needsAlpha;

    if (needsAlpha) {
        rgbaData.resize(pixelCount * 4);
        for (size_t i = 0; i < pixelCount; ++i) {
            const size_t srcIndex = i * 3;
            const size_t dstIndex = i * 4;
            rgbaData[dstIndex + 0] = frame.rgbData[srcIndex + 0];
            rgbaData[dstIndex + 1] = frame.rgbData[srcIndex + 1];
            rgbaData[dstIndex + 2] = frame.rgbData[srcIndex + 2];
            rgbaData[dstIndex + 3] = frame.alphaData[i];
        }
        sourceData = rgbaData.data();
    }
    const GLsizeiptr byteCount = static_cast<GLsizeiptr>(needsAlpha ? rgbaData.size() : frame.rgbData.size());
    bool uploadedWithPbo = false;

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, videoTexture);

    if (m_asyncTextureUploads && uploadPbos[0] != 0) {
        const GLuint pbo = uploadPbos[nextUploadPbo++ % uploadPbos.size()];
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
        glBufferData(GL_PIXEL_UNPACK_BUFFER, byteCount, nullptr, GL_STREAM_DRAW);
        void* destination = glMapBufferRange(
            GL_PIXEL_UNPACK_BUFFER, 0, byteCount,
            GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
        if (destination) {
            std::memcpy(destination, sourceData, static_cast<size_t>(byteCount));
            if (glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER) == GL_TRUE) {
                if (resized || formatChanged) {
                    glTexImage2D(GL_TEXTURE_2D, 0, needsAlpha ? GL_RGBA : GL_RGB, frame.width, frame.height, 0, needsAlpha ? GL_RGBA : GL_RGB, GL_UNSIGNED_BYTE, nullptr);
                } else {
                    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, frame.width, frame.height, needsAlpha ? GL_RGBA : GL_RGB, GL_UNSIGNED_BYTE, nullptr);
                }
                uploadedWithPbo = true;
            }
        }
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    }

    if (!uploadedWithPbo) {
        if (resized || formatChanged) {
            glTexImage2D(GL_TEXTURE_2D, 0, needsAlpha ? GL_RGBA : GL_RGB, frame.width, frame.height, 0, needsAlpha ? GL_RGBA : GL_RGB, GL_UNSIGNED_BYTE, sourceData);
        } else {
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, frame.width, frame.height, needsAlpha ? GL_RGBA : GL_RGB, GL_UNSIGNED_BYTE, sourceData);
        }
    }
    lastFrameWidth = frame.width;
    lastFrameHeight = frame.height;
    lastFrameHasAlpha = needsAlpha;
}

void GLWidget::setDetections(const std::vector<DetectionBox>& boxes) {
    detections = boxes;
    update();
}

void GLWidget::setShowDetections(bool show) {
    m_showDetections = show;
    update();
}

void GLWidget::setDetectionOverlayOptions(const DetectionOverlayOptions& options) {
    m_detectionOverlayOptions = options;
    m_detectionOverlayOptions.lineWidth = std::clamp(m_detectionOverlayOptions.lineWidth, 1, 12);
    m_detectionOverlayOptions.labelPointSize = std::clamp(m_detectionOverlayOptions.labelPointSize, 8, 48);
    m_detectionOverlayOptions.maxDetections = std::clamp(m_detectionOverlayOptions.maxDetections, 1, 128);
    m_detectionOverlayOptions.fillOpacity = std::clamp(m_detectionOverlayOptions.fillOpacity, 0, 255);
    m_detectionOverlayOptions.trailLength = std::clamp(m_detectionOverlayOptions.trailLength, 2, 120);
    m_detectionOverlayOptions.trailWidth = std::clamp(m_detectionOverlayOptions.trailWidth, 1, 12);
    m_detectionOverlayOptions.trailOpacity = std::clamp(m_detectionOverlayOptions.trailOpacity, 0, 255);
    m_detectionOverlayOptions.linkDistance = std::clamp(m_detectionOverlayOptions.linkDistance, 0.02f, 1.0f);
    update();
}

void GLWidget::setDetectionShape(DetectionShape shape) {
    m_detectionShape = shape;
    update();
}

void GLWidget::setGuideOverlay(GuideOverlay guide) {
    m_guideOverlay = guide;
    update();
}

void GLWidget::setMaskEnabled(bool enabled) {
    m_maskEnabled = enabled;
    update();
}

void GLWidget::setMaskInverted(bool inverted) {
    m_maskInverted = inverted;
    update();
}

void GLWidget::setDetectionMaskEffectIds(std::vector<std::string> effectIds) {
    m_detectionMaskEffectIds = std::move(effectIds);
    update();
}

void GLWidget::setMaskData(int width, int height, const std::vector<uint8_t>& maskR) {
    pendingMaskW = width;
    pendingMaskH = height;
    pendingMask = maskR;
    maskDirty = true;
    const bool hasCoverage = std::any_of(maskR.begin(), maskR.end(), [](uint8_t v) {
        return v != 0;
    });
    // An all-zero mask makes every effect appear "broken" (fully bypassed)
    // while still paying an extra full-screen composite pass per effect.
    // Treat it as no mask until detections produce real coverage.
    hasMaskTexture = (width > 0 && height > 0 && !maskR.empty() && hasCoverage);
    m_detectionMaskHasCoverage = hasMaskTexture;
    update();
}

void GLWidget::setMaskData(int width, int height, std::vector<uint8_t>&& maskR) {
    pendingMaskW = width;
    pendingMaskH = height;
    const bool hasCoverage = std::any_of(maskR.begin(), maskR.end(), [](uint8_t value) {
        return value != 0;
    });
    pendingMask = std::move(maskR);
    maskDirty = true;
    hasMaskTexture = (width > 0 && height > 0 && !pendingMask.empty() && hasCoverage);
    m_detectionMaskHasCoverage = hasMaskTexture;
    update();
}

void GLWidget::setClipMasks(const std::vector<ClipMask>& masks, double clipLocalTime) {
    m_clipMasks = masks;
    m_clipMaskLocalTime = clipLocalTime;
    update();
}

void GLWidget::uploadMaskPixels(const std::vector<uint8_t>& pixels, int width, int height, bool flipVertical) {
    m_uploadedMaskSignature = 0;
    if (width <= 0 || height <= 0 || pixels.size() != static_cast<size_t>(width) * static_cast<size_t>(height)) {
        hasMaskTexture = false;
        return;
    }

    std::vector<uint8_t> flippedPixels;
    const uint8_t* pixelData = pixels.data();
    if (flipVertical) {
        const size_t rowBytes = static_cast<size_t>(width);
        flippedPixels.resize(pixels.size());
        for (int y = 0; y < height; ++y) {
            std::copy_n(pixels.data() + static_cast<size_t>(height - 1 - y) * rowBytes, rowBytes,
                flippedPixels.data() + static_cast<size_t>(y) * rowBytes);
        }
        pixelData = flippedPixels.data();
    }

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, maskTexture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    if (lastMaskWidth != width || lastMaskHeight != height) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, width, height, 0, GL_RED, GL_UNSIGNED_BYTE, pixelData);
        lastMaskWidth = width;
        lastMaskHeight = height;
    } else {
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RED, GL_UNSIGNED_BYTE, pixelData);
    }
    hasMaskTexture = true;
}

void GLWidget::uploadMaskIfNeeded() {
    if (!maskDirty || !maskTexture) return;
    maskDirty = false;

    if (pendingMaskW <= 0 || pendingMaskH <= 0 || pendingMask.empty() || !hasMaskTexture) {
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, maskTexture);
        unsigned char whitePixel = 255;
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, 1, 1, 0, GL_RED, GL_UNSIGNED_BYTE, &whitePixel);
        lastMaskWidth = 1;
        lastMaskHeight = 1;
        m_uploadedMaskSignature = 0;
        hasMaskTexture = false;
        return;
    }

    uploadMaskPixels(pendingMask, pendingMaskW, pendingMaskH);
}

void GLWidget::paintEvent(QPaintEvent* event) {
    QOpenGLWidget::paintEvent(event);

    if (m_guideOverlay == GuideOverlay::None && (!m_showDetections || detections.empty()) &&
        !m_activeEditMask && !m_maskDrawing) return;

    QPainter painter(this);
    // The renderer leaves custom programs, textures and framebuffer state
    // behind. Reset every texture unit and pixel-unpack binding at Qt's
    // native-painting boundary; stale GL state can corrupt Qt's glyph atlas.
    painter.beginNativePainting();
    glUseProgram(0);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    for (int textureUnit = 0; textureUnit < 3; ++textureUnit) {
        glActiveTexture(GL_TEXTURE0 + textureUnit);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    glActiveTexture(GL_TEXTURE0);
    glBindFramebuffer(GL_FRAMEBUFFER, defaultFramebufferObject());
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    painter.endNativePainting();
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);

    // Editor-only overlays are painted after OpenGL presentation. Export reads
    // renderedTexture directly, so guides and detection labels never render to
    // the output video.
    const QColor guideColor(110, 231, 183, 185);
    QPen guidePen(guideColor, 1.0, Qt::DashLine);
    painter.setPen(guidePen);
    if (m_guideOverlay == GuideOverlay::Center) {
        painter.drawLine(width() / 2, 0, width() / 2, height());
        painter.drawLine(0, height() / 2, width(), height() / 2);
    } else if (m_guideOverlay == GuideOverlay::RuleOfThirds) {
        for (int i : {1, 2}) {
            painter.drawLine(width() * i / 3, 0, width() * i / 3, height());
            painter.drawLine(0, height() * i / 3, width(), height() * i / 3);
        }
    } else if (m_guideOverlay == GuideOverlay::SafeAreas) {
        painter.drawRect(QRectF(width() * 0.05, height() * 0.05, width() * 0.90, height() * 0.90));
        painter.drawRect(QRectF(width() * 0.10, height() * 0.10, width() * 0.80, height() * 0.80));
    } else if (m_guideOverlay == GuideOverlay::Grid) {
        for (int i = 1; i < 8; ++i) {
            painter.drawLine(width() * i / 8, 0, width() * i / 8, height());
            painter.drawLine(0, height() * i / 8, width(), height() * i / 8);
        }
    }

    if (m_showDetections && !detections.empty()) {

    const auto stableColor = [](const std::string& key, int fallback) {
        uint32_t hash = 2166136261u;
        for (const unsigned char ch : key) {
            hash = (hash ^ ch) * 16777619u;
        }
        hash ^= static_cast<uint32_t>(fallback) * 2654435761u;
        return QColor::fromHsv(static_cast<int>(hash % 360u), 185 + static_cast<int>((hash >> 9) % 56u), 255);
    };
    const auto colorFor = [&](const DetectionBox& box, int fallback) {
        switch (m_detectionOverlayOptions.colorMode) {
        case DetectionColorMode::ByClass:
            return stableColor(box.label, fallback);
        case DetectionColorMode::Fixed:
            return QColor(245, 158, 248);
        case DetectionColorMode::ByTrack:
        default:
            return stableColor("track_" + std::to_string(std::max(0, box.trackId)), fallback);
        }
    };
    const auto centerOf = [](const DetectionBox& box) {
        return QPointF(box.x + box.w * 0.5, box.y + box.h * 0.5);
    };
    // Decoded RGB rows are flipped for OpenGL texture upload: detector space
    // has y=0 at the texture's bottom while QPainter has y=0 at the widget's
    // top. Keep this conversion in one place so boxes, labels, links and
    // trails stay registered to the preview image.
    const auto previewPoint = [&](float x, float y) {
        return QPointF(x * width(), (1.0f - y) * height());
    };
    const auto previewRect = [&](const DetectionBox& box) {
        return QRectF(
            box.x * width(),
            (1.0f - box.y - box.h) * height(),
            box.w * width(),
            box.h * height());
    };

    const size_t visibleCount = std::min(
        detections.size(), static_cast<size_t>(m_detectionOverlayOptions.maxDetections));

    // Badges already on screen; overlapping detections must not print their
    // labels on top of each other.
    std::vector<QRect> placedBadges;
    placedBadges.reserve(visibleCount);

    if (m_detectionOverlayOptions.showLinks && visibleCount > 1) {
        painter.save();
        for (size_t i = 0; i < visibleCount; ++i) {
            const QPointF from = centerOf(detections[i]);
            for (size_t j = i + 1; j < visibleCount; ++j) {
                const QPointF to = centerOf(detections[j]);
                const qreal distance = std::hypot(from.x() - to.x(), from.y() - to.y());
                if (distance > m_detectionOverlayOptions.linkDistance) continue;
                QColor color = colorFor(detections[i], static_cast<int>(i));
                color.setAlpha(130);
                QPen pen(color, std::max(1, m_detectionOverlayOptions.lineWidth - 1), Qt::DashLine);
                painter.setPen(pen);
                painter.drawLine(previewPoint(static_cast<float>(from.x()), static_cast<float>(from.y())),
                    previewPoint(static_cast<float>(to.x()), static_cast<float>(to.y())));
            }
        }
        painter.restore();
    }

    if (m_detectionOverlayOptions.showTrails) {
        painter.save();
        for (size_t boxIndex = 0; boxIndex < visibleCount; ++boxIndex) {
            const auto& box = detections[boxIndex];
            const auto& trail = box.trail;
            if (trail.size() < 2) continue;
            const size_t first = trail.size() > static_cast<size_t>(m_detectionOverlayOptions.trailLength)
                ? trail.size() - static_cast<size_t>(m_detectionOverlayOptions.trailLength) : 0;
            const QColor baseColor = colorFor(box, static_cast<int>(boxIndex));
            QColor color = baseColor;
            color.setAlpha(m_detectionOverlayOptions.trailOpacity);
            painter.setPen(QPen(color, m_detectionOverlayOptions.trailWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            QPainterPath trailPath(previewPoint(trail[first].x, trail[first].y));
            for (size_t pointIndex = first + 1; pointIndex < trail.size(); ++pointIndex) {
                trailPath.lineTo(previewPoint(trail[pointIndex].x, trail[pointIndex].y));
            }
            painter.drawPath(trailPath);
        }
        painter.restore();
    }

    for (size_t i = 0; i < visibleCount; ++i) {
        const auto& box = detections[i];
        const QColor color = colorFor(box, static_cast<int>(i));
        const QRectF rect = previewRect(box);

        const bool hasPersonOutline = m_detectionOverlayOptions.showPersonOutline &&
            box.label == "person" && box.outline.size() >= 3;
        const bool drawBox = !hasPersonOutline || !m_detectionOverlayOptions.replacePersonBoxesWithOutline;

        if (drawBox && m_detectionOverlayOptions.fillOpacity > 0) {
            QColor fill = color;
            fill.setAlpha(m_detectionOverlayOptions.fillOpacity);
            painter.setPen(Qt::NoPen);
            painter.setBrush(fill);
            if (m_detectionOverlayOptions.style == DetectionOverlayStyle::Ellipse) {
                painter.drawEllipse(rect);
            } else if (m_detectionOverlayOptions.style == DetectionOverlayStyle::RoundedRectangle) {
                painter.drawRoundedRect(rect, 6.0, 6.0);
            } else {
                painter.drawRect(rect);
            }
        }

        QPen pen(color, m_detectionOverlayOptions.lineWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
        if (drawBox) {
            if (m_detectionOverlayOptions.style == DetectionOverlayStyle::Ellipse) {
                painter.drawEllipse(rect);
            } else if (m_detectionOverlayOptions.style == DetectionOverlayStyle::RoundedRectangle) {
                painter.drawRoundedRect(rect, 6.0, 6.0);
            } else if (m_detectionOverlayOptions.style == DetectionOverlayStyle::CornerBrackets) {
                const qreal arm = std::max<qreal>(8.0, std::min(rect.width(), rect.height()) * 0.24);
                painter.drawLine(rect.left(), rect.top(), rect.left() + arm, rect.top());
                painter.drawLine(rect.left(), rect.top(), rect.left(), rect.top() + arm);
                painter.drawLine(rect.right(), rect.top(), rect.right() - arm, rect.top());
                painter.drawLine(rect.right(), rect.top(), rect.right(), rect.top() + arm);
                painter.drawLine(rect.left(), rect.bottom(), rect.left() + arm, rect.bottom());
                painter.drawLine(rect.left(), rect.bottom(), rect.left(), rect.bottom() - arm);
                painter.drawLine(rect.right(), rect.bottom(), rect.right() - arm, rect.bottom());
                painter.drawLine(rect.right(), rect.bottom(), rect.right(), rect.bottom() - arm);
            } else {
                painter.drawRect(rect);
            }
        }
        if (hasPersonOutline) {
            QPainterPath outlinePath(previewPoint(box.outline.front().x, box.outline.front().y));
            for (size_t pointIndex = 1; pointIndex < box.outline.size(); ++pointIndex) {
                outlinePath.lineTo(previewPoint(box.outline[pointIndex].x, box.outline[pointIndex].y));
            }
            outlinePath.closeSubpath();
            painter.drawPath(outlinePath);
        }

        if (m_detectionOverlayOptions.showCenters) {
            painter.setBrush(color);
            painter.setPen(Qt::NoPen);
            const QPointF center(rect.center());
            const qreal radius = std::max<qreal>(2.0, m_detectionOverlayOptions.lineWidth + 0.5);
            painter.drawEllipse(center, radius, radius);
        }

        QStringList labelParts;
        if (m_detectionOverlayOptions.showTrackIds && box.trackId > 0) {
            labelParts << QString("#%1").arg(box.trackId);
        }
        if (m_detectionOverlayOptions.showLabels) {
            labelParts << QString::fromStdString(box.label.empty() ? "object" : box.label);
        }
        if (m_detectionOverlayOptions.showConfidence) {
            labelParts << QString("%1%").arg(box.confidence * 100.0f, 0, 'f', 0);
        }
        if (labelParts.isEmpty()) continue;
        const QString label = labelParts.join("  ");

        QFont font = painter.font();
        font.setBold(true);
        font.setPointSize(m_detectionOverlayOptions.labelPointSize);
        font.setStyleStrategy(QFont::StyleStrategy(QFont::PreferAntialias | QFont::PreferQuality));
        painter.setFont(font);

        const QFontMetrics fm(font);
        const int paddingX = 7;
        const int paddingY = 3;
        const int accentWidth = 3;
        const int badgeW = fm.horizontalAdvance(label) + paddingX * 2 + accentWidth;
        const int badgeH = fm.height() + paddingY * 2;

        // Prefer a badge just above the box; when the box touches the top of
        // the frame, tuck the badge inside it so text never leaves the view
        // or gets struck through by the box's own edge.
        int badgeX = static_cast<int>(std::lround(rect.left()));
        int badgeY = static_cast<int>(std::lround(rect.top())) - badgeH - 2;
        if (badgeY < 0) {
            badgeY = static_cast<int>(std::lround(rect.top())) + 2;
        }
        badgeX = std::clamp(badgeX, 0, std::max(0, width() - badgeW));
        badgeY = std::clamp(badgeY, 0, std::max(0, height() - badgeH));
        QRect badge(badgeX, badgeY, badgeW, badgeH);

        // Nudge colliding badges down until they find free space instead of
        // double-printing text on top of an existing label.
        for (int attempt = 0; attempt < 16; ++attempt) {
            const bool collides = std::any_of(placedBadges.begin(), placedBadges.end(),
                [&badge](const QRect& placed) { return badge.intersects(placed); });
            if (!collides) break;
            badge.translate(0, badgeH + 2);
            if (badge.bottom() > height() - 1) {
                badge.moveBottom(height() - 1);
                break;
            }
        }
        placedBadges.push_back(badge);

        // Dark plate with a colored accent strip: readable over bright and
        // dark footage alike, unlike saturated fill with near-black text.
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(12, 10, 18, 200));
        painter.drawRoundedRect(badge, 3.0, 3.0);
        QColor accent = color;
        accent.setAlpha(255);
        painter.setBrush(accent);
        painter.drawRect(QRect(badge.left(), badge.top() + 2, accentWidth, badge.height() - 4));

        painter.setBrush(Qt::NoBrush);
        const QRect textArea = badge.adjusted(accentWidth + paddingX, 0, -paddingX, 0);
        // Rasterize glyphs with Qt's CPU paint engine, then composite the
        // finished image. This avoids the OpenGL glyph-atlas path that can
        // inherit renderer texture/PBO state and produce mirrored/noisy text.
        QImage textImage(textArea.size(), QImage::Format_ARGB32_Premultiplied);
        textImage.fill(Qt::transparent);
        {
            QPainter textPainter(&textImage);
            textPainter.setRenderHint(QPainter::TextAntialiasing, true);
            textPainter.setFont(font);
            textPainter.setPen(Qt::white);
            textPainter.drawText(textImage.rect(), Qt::AlignVCenter | Qt::AlignLeft, label);
        }
        painter.drawImage(textArea.topLeft(), textImage);
    }

    }

    if (m_activeEditMask) {
        renderMaskDirectOverlay(painter);
    }

    if (m_maskDrawing && m_maskDrawInProgress) {
        const QPointF current = m_maskDrawCurrent;
        const QRectF drawRect(m_maskDrawStart, current);
        painter.save();
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setPen(QPen(QColor("#e855f4"), 2.0, Qt::DashLine));
        painter.setBrush(QColor(232, 85, 244, 28));
        if (m_maskDrawingShape == MaskShapeType::Ellipse) painter.drawEllipse(drawRect.normalized());
        else painter.drawRect(drawRect.normalized());
        painter.restore();
    }
}

void GLWidget::setActiveEditMask(const ClipMask* mask) {
    m_activeEditMask = mask;
    m_maskDrawing = false;
    m_maskDrawInProgress = false;
    unsetCursor();
    m_hoveredPointIndex = -1;
    m_draggedPointIndex = -1;
    m_draggingMaskCenter = false;
    m_draggingMaskRotate = false;
    m_draggingMaskScale = false;
    update();
}

void GLWidget::beginMaskDrawing(MaskShapeType shape) {
    setActiveEditMask(nullptr);
    m_maskDrawingShape = shape;
    m_maskDrawing = true;
    m_maskDrawInProgress = false;
    setCursor(Qt::CrossCursor);
    update();
}

void GLWidget::renderMaskDirectOverlay(QPainter& painter) {
    if (!m_activeEditMask || width() <= 0 || height() <= 0) return;

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);

    const int w = width();
    const int h = height();
    const double px = m_activeEditMask->evalProp(m_activeEditMask->posXCurve, m_activeEditMask->posX, m_editMaskClipTime);
    const double py = m_activeEditMask->evalProp(m_activeEditMask->posYCurve, m_activeEditMask->posY, m_editMaskClipTime);
    const double sx = m_activeEditMask->evalProp(m_activeEditMask->scaleXCurve, m_activeEditMask->scaleX, m_editMaskClipTime);
    const double sy = m_activeEditMask->evalProp(m_activeEditMask->scaleYCurve, m_activeEditMask->scaleY, m_editMaskClipTime);
    const double rot = m_activeEditMask->evalProp(m_activeEditMask->rotationCurve, m_activeEditMask->rotation, m_editMaskClipTime);
    const double feather = std::max(0.0, m_activeEditMask->evalProp(m_activeEditMask->featherCurve, m_activeEditMask->feather, m_editMaskClipTime));
    const double expPx = m_activeEditMask->evalProp(m_activeEditMask->expansionCurve, m_activeEditMask->expansion, m_editMaskClipTime);

    const float cx = static_cast<float>(px * w);
    const float cy = static_cast<float>(py * h);

    QTransform transform;
    transform.translate(cx, cy);
    transform.rotate(rot);
    transform.scale(sx, sy);

    QPen outlinePen(QColor("#e855f4"), 2.0);
    outlinePen.setStyle(Qt::SolidLine);
    painter.setPen(outlinePen);
    painter.setBrush(Qt::NoBrush);

    QPainterPath path;
    if (m_activeEditMask->shapeType == MaskShapeType::Rectangle) {
        float halfW = 0.25f * w;
        float halfH = 0.25f * h;
        path.addRect(-halfW, -halfH, halfW * 2.0f, halfH * 2.0f);
    } else if (m_activeEditMask->shapeType == MaskShapeType::Ellipse) {
        float rx = 0.25f * w;
        float ry = 0.25f * h;
        path.addEllipse(QPointF(0.0, 0.0), rx, ry);
    } else if (!m_activeEditMask->points.empty()) {
        path.moveTo((m_activeEditMask->points[0].x - 0.5f) * w, (m_activeEditMask->points[0].y - 0.5f) * h);
        for (size_t i = 1; i < m_activeEditMask->points.size(); ++i) {
            const auto& prev = m_activeEditMask->points[i - 1];
            const auto& curr = m_activeEditMask->points[i];
            if (m_activeEditMask->shapeType == MaskShapeType::Bezier &&
                (prev.outHandleX != 0 || prev.outHandleY != 0 || curr.inHandleX != 0 || curr.inHandleY != 0)) {
                QPointF c1((prev.x - 0.5f + prev.outHandleX) * w, (prev.y - 0.5f + prev.outHandleY) * h);
                QPointF c2((curr.x - 0.5f + curr.inHandleX) * w, (curr.y - 0.5f + curr.inHandleY) * h);
                path.cubicTo(c1, c2, QPointF((curr.x - 0.5f) * w, (curr.y - 0.5f) * h));
            } else {
                path.lineTo((curr.x - 0.5f) * w, (curr.y - 0.5f) * h);
            }
        }
        if (m_activeEditMask->closed) path.closeSubpath();
    }

    QPainterPath transformedPath = transform.map(path);
    if (std::abs(expPx) > 0.01) {
        QPainterPathStroker stroker;
        stroker.setWidth(std::abs(expPx) * 2.0);
        const QPainterPath border = stroker.createStroke(transformedPath);
        transformedPath = expPx > 0.0 ? transformedPath.united(border) : transformedPath.subtracted(border);
    }
    painter.setBrush(QColor(232, 85, 244, 38));
    painter.drawPath(transformedPath);
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(transformedPath);

    if (feather > 1.0) {
        QPen featherPen(QColor(232, 85, 244, 90), 1.0, Qt::DashLine);
        painter.setPen(featherPen);
        painter.drawPath(transformedPath);
    }

    QPen vertexPen(QColor("#ffffff"), 1.5);
    QBrush vertexBrush(QColor("#e855f4"));
    QBrush hoverBrush(QColor("#38bdf8"));

    if (m_activeEditMask->shapeType == MaskShapeType::Polygon || m_activeEditMask->shapeType == MaskShapeType::Bezier) {
        for (size_t i = 0; i < m_activeEditMask->points.size(); ++i) {
            const auto& pt = m_activeEditMask->points[i];
            QPointF rawPt((pt.x - 0.5) * w, (pt.y - 0.5) * h);
            QPointF screenPt = transform.map(rawPt);

            bool isHovered = (static_cast<int>(i) == m_hoveredPointIndex || static_cast<int>(i) == m_draggedPointIndex);
            painter.setPen(vertexPen);
            painter.setBrush(isHovered ? hoverBrush : vertexBrush);
            const float r = isHovered ? 6.0f : 4.5f;
            painter.drawEllipse(screenPt, r, r);

            if (m_activeEditMask->shapeType == MaskShapeType::Bezier) {
                if (pt.inHandleX != 0 || pt.inHandleY != 0) {
                    QPointF inPt = transform.map(QPointF((pt.x - 0.5 + pt.inHandleX) * w, (pt.y - 0.5 + pt.inHandleY) * h));
                    painter.setPen(QPen(QColor(255, 255, 255, 140), 1.0));
                    painter.drawLine(screenPt, inPt);
                    painter.setBrush(QColor("#38bdf8"));
                    painter.drawEllipse(inPt, 3.0f, 3.0f);
                }
                if (pt.outHandleX != 0 || pt.outHandleY != 0) {
                    QPointF outPt = transform.map(QPointF((pt.x - 0.5 + pt.outHandleX) * w, (pt.y - 0.5 + pt.outHandleY) * h));
                    painter.setPen(QPen(QColor(255, 255, 255, 140), 1.0));
                    painter.drawLine(screenPt, outPt);
                    painter.setBrush(QColor("#38bdf8"));
                    painter.drawEllipse(outPt, 3.0f, 3.0f);
                }
            }
        }
    }

    painter.setPen(QPen(QColor("#ffffff"), 1.5));
    painter.setBrush(QColor(232, 85, 244, 180));
    painter.drawEllipse(QPointF(cx, cy), 5.0, 5.0);
    painter.drawLine(QPointF(cx - 8, cy), QPointF(cx + 8, cy));
    painter.drawLine(QPointF(cx, cy - 8), QPointF(cx, cy + 8));

    QPointF rotHandle(0.0, -35.0f);
    QPointF transformedRot = transform.map(rotHandle);
    painter.setPen(QPen(QColor("#e855f4"), 1.0, Qt::DashLine));
    painter.drawLine(QPointF(cx, cy), transformedRot);
    painter.setPen(vertexPen);
    painter.setBrush(m_draggingMaskRotate ? hoverBrush : QColor("#a855f7"));
    painter.drawEllipse(transformedRot, 5.0, 5.0);

    const QPointF transformedScale = transform.map(QPointF(0.25 * w, 0.25 * h));
    painter.setPen(QPen(QColor("#ffffff"), 1.5));
    painter.setBrush(m_draggingMaskScale ? hoverBrush : QColor("#e855f4"));
    painter.drawRect(QRectF(transformedScale.x() - 5.0, transformedScale.y() - 5.0, 10.0, 10.0));

    painter.restore();
}

void GLWidget::drawDetectionExportOverlay(QPainter& painter, int targetW, int targetH) const {
    if (targetW <= 0 || targetH <= 0 || width() <= 0 || height() <= 0 || detections.empty()) return;
    const qreal scaleX = static_cast<qreal>(targetW) / width();
    const qreal scaleY = static_cast<qreal>(targetH) / height();
    const auto stableColor = [](const std::string& key, int fallback) {
        uint32_t hash = 2166136261u;
        for (const unsigned char ch : key) hash = (hash ^ ch) * 16777619u;
        hash ^= static_cast<uint32_t>(fallback) * 2654435761u;
        return QColor::fromHsv(static_cast<int>(hash % 360u), 185 + static_cast<int>((hash >> 9) % 56u), 255);
    };
    const auto previewPoint = [targetW, targetH](float x, float y) {
        return QPointF(x * targetW, (1.0f - y) * targetH);
    };

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    std::vector<QRect> placedBadges;
    const int lineWidth = std::max(1, static_cast<int>(std::lround(m_detectionOverlayOptions.lineWidth * std::max(scaleX, scaleY))));
    for (size_t index = 0; index < detections.size(); ++index) {
        const auto& box = detections[index];
        const QRectF rect(box.x * targetW, (1.0f - box.y - box.h) * targetH,
            box.w * targetW, box.h * targetH);
        QColor color;
        if (m_detectionOverlayOptions.colorMode == DetectionColorMode::Fixed) {
            color = QColor(245, 158, 248);
        } else if (m_detectionOverlayOptions.colorMode == DetectionColorMode::ByClass) {
            color = stableColor(box.label, static_cast<int>(index));
        } else {
            color = stableColor("track_" + std::to_string(std::max(0, box.trackId)), static_cast<int>(index));
        }

        if (m_detectionOverlayOptions.fillOpacity > 0) {
            QColor fill = color;
            fill.setAlpha(std::clamp(m_detectionOverlayOptions.fillOpacity, 0, 255));
            painter.setPen(Qt::NoPen);
            painter.setBrush(fill);
            painter.drawRect(rect);
        }
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(color, lineWidth));
        if (m_detectionOverlayOptions.style == DetectionOverlayStyle::Ellipse) {
            painter.drawEllipse(rect);
        } else if (m_detectionOverlayOptions.style == DetectionOverlayStyle::RoundedRectangle) {
            painter.drawRoundedRect(rect, 6.0 * scaleX, 6.0 * scaleY);
        } else if (m_detectionOverlayOptions.style == DetectionOverlayStyle::CornerBrackets) {
            const qreal arm = std::max<qreal>(8.0 * scaleX, std::min(rect.width(), rect.height()) * 0.24);
            painter.drawLine(rect.topLeft(), QPointF(rect.left() + arm, rect.top()));
            painter.drawLine(rect.topLeft(), QPointF(rect.left(), rect.top() + arm));
            painter.drawLine(rect.topRight(), QPointF(rect.right() - arm, rect.top()));
            painter.drawLine(rect.topRight(), QPointF(rect.right(), rect.top() + arm));
            painter.drawLine(rect.bottomLeft(), QPointF(rect.left() + arm, rect.bottom()));
            painter.drawLine(rect.bottomLeft(), QPointF(rect.left(), rect.bottom() - arm));
            painter.drawLine(rect.bottomRight(), QPointF(rect.right() - arm, rect.bottom()));
            painter.drawLine(rect.bottomRight(), QPointF(rect.right(), rect.bottom() - arm));
        } else {
            painter.drawRect(rect);
        }

        if (m_detectionOverlayOptions.showPersonOutline && box.outline.size() >= 3) {
            QPainterPath outline(previewPoint(box.outline.front().x, box.outline.front().y));
            for (size_t point = 1; point < box.outline.size(); ++point) {
                outline.lineTo(previewPoint(box.outline[point].x, box.outline[point].y));
            }
            outline.closeSubpath();
            painter.setPen(QPen(color, lineWidth));
            painter.drawPath(outline);
        }

        QStringList labelParts;
        if (m_detectionOverlayOptions.showTrackIds && box.trackId > 0) labelParts << QString("#%1").arg(box.trackId);
        if (m_detectionOverlayOptions.showLabels) labelParts << QString::fromStdString(box.label.empty() ? "object" : box.label);
        if (m_detectionOverlayOptions.showConfidence) labelParts << QString("%1%").arg(box.confidence * 100.0f, 0, 'f', 0);
        if (labelParts.isEmpty()) continue;

        const QString label = labelParts.join("  ");
        QFont font = painter.font();
        font.setBold(true);
        font.setPointSizeF(std::max(1.0, m_detectionOverlayOptions.labelPointSize * scaleY));
        painter.setFont(font);
        const QFontMetrics metrics(font);
        const int paddingX = std::max(2, static_cast<int>(std::lround(7 * scaleX)));
        const int paddingY = std::max(1, static_cast<int>(std::lround(3 * scaleY)));
        const int accentWidth = std::max(1, static_cast<int>(std::lround(3 * scaleX)));
        const int badgeW = metrics.horizontalAdvance(label) + paddingX * 2 + accentWidth;
        const int badgeH = metrics.height() + paddingY * 2;
        int badgeX = std::clamp(static_cast<int>(std::lround(rect.left())), 0, std::max(0, targetW - badgeW));
        int badgeY = static_cast<int>(std::lround(rect.top())) - badgeH - 2;
        if (badgeY < 0) badgeY = static_cast<int>(std::lround(rect.top())) + 2;
        badgeY = std::clamp(badgeY, 0, std::max(0, targetH - badgeH));
        QRect badge(badgeX, badgeY, badgeW, badgeH);
        for (int attempt = 0; attempt < 16; ++attempt) {
            if (std::none_of(placedBadges.begin(), placedBadges.end(), [&badge](const QRect& placed) { return badge.intersects(placed); })) break;
            badge.translate(0, badgeH + 2);
            if (badge.bottom() > targetH - 1) { badge.moveBottom(targetH - 1); break; }
        }
        placedBadges.push_back(badge);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(12, 10, 18, 200));
        painter.drawRoundedRect(badge, 3.0 * scaleX, 3.0 * scaleY);
        color.setAlpha(255);
        painter.setBrush(color);
        painter.drawRect(QRect(badge.left(), badge.top() + 2, accentWidth, badge.height() - 4));
        painter.setBrush(Qt::NoBrush);
        painter.setPen(Qt::white);
        painter.drawText(badge.adjusted(accentWidth + paddingX, 0, -paddingX, 0), Qt::AlignVCenter | Qt::AlignLeft, label);
    }
    painter.restore();
}

void GLWidget::mousePressEvent(QMouseEvent* event) {
    if (m_maskDrawing && event->button() == Qt::LeftButton) {
        m_maskDrawStart = QPointF(
            std::clamp(event->position().x(), 0.0, static_cast<double>(width())),
            std::clamp(event->position().y(), 0.0, static_cast<double>(height())));
        m_maskDrawCurrent = m_maskDrawStart;
        m_maskDrawInProgress = true;
        update();
        event->accept();
        return;
    }

    if (m_activeEditMask && event->button() == Qt::LeftButton) {
        const int w = width();
        const int h = height();
        if (w > 0 && h > 0) {
            const double px = m_activeEditMask->evalProp(m_activeEditMask->posXCurve, m_activeEditMask->posX, m_editMaskClipTime);
            const double py = m_activeEditMask->evalProp(m_activeEditMask->posYCurve, m_activeEditMask->posY, m_editMaskClipTime);
            const double sx = m_activeEditMask->evalProp(m_activeEditMask->scaleXCurve, m_activeEditMask->scaleX, m_editMaskClipTime);
            const double sy = m_activeEditMask->evalProp(m_activeEditMask->scaleYCurve, m_activeEditMask->scaleY, m_editMaskClipTime);
            const double rot = m_activeEditMask->evalProp(m_activeEditMask->rotationCurve, m_activeEditMask->rotation, m_editMaskClipTime);

            const float cx = static_cast<float>(px * w);
            const float cy = static_cast<float>(py * h);

            QTransform transform;
            transform.translate(cx, cy);
            transform.rotate(rot);
            transform.scale(sx, sy);

            m_dragStartPos = event->position();
            m_dragStartPosX = px;
            m_dragStartPosY = py;
            m_dragStartScaleX = sx;
            m_dragStartScaleY = sy;
            m_dragStartRotation = rot;

            QPointF rotHandle = transform.map(QPointF(0.0, -35.0f));
            if (QLineF(event->position(), rotHandle).length() <= 10.0) {
                m_draggingMaskRotate = true;
                event->accept();
                return;
            }

            const QPointF scaleHandle = transform.map(QPointF(0.25 * w, 0.25 * h));
            if (QLineF(event->position(), scaleHandle).length() <= 10.0) {
                m_draggingMaskScale = true;
                event->accept();
                return;
            }

            const bool editablePath = m_activeEditMask->shapeType == MaskShapeType::Polygon ||
                m_activeEditMask->shapeType == MaskShapeType::Bezier;
            if (editablePath) {
                for (size_t i = 0; i < m_activeEditMask->points.size(); ++i) {
                    QPointF pt = transform.map(QPointF(
                        (m_activeEditMask->points[i].x - 0.5) * w,
                        (m_activeEditMask->points[i].y - 0.5) * h));
                    if (QLineF(event->position(), pt).length() <= 8.0) {
                        m_draggedPointIndex = static_cast<int>(i);
                        event->accept();
                        return;
                    }
                }
            }

            QPainterPath hitPath;
            if (m_activeEditMask->shapeType == MaskShapeType::Ellipse) {
                hitPath.addEllipse(QRectF(-0.25 * w, -0.25 * h, 0.5 * w, 0.5 * h));
            } else if (editablePath && !m_activeEditMask->points.empty()) {
                hitPath.moveTo((m_activeEditMask->points.front().x - 0.5) * w,
                    (m_activeEditMask->points.front().y - 0.5) * h);
                for (size_t i = 1; i < m_activeEditMask->points.size(); ++i) {
                    hitPath.lineTo((m_activeEditMask->points[i].x - 0.5) * w,
                        (m_activeEditMask->points[i].y - 0.5) * h);
                }
                if (m_activeEditMask->closed) hitPath.closeSubpath();
            } else {
                hitPath.addRect(QRectF(-0.25 * w, -0.25 * h, 0.5 * w, 0.5 * h));
            }
            if (transform.map(hitPath).contains(event->position())) {
                m_draggingMaskCenter = true;
                event->accept();
                return;
            }
        }
    }
    QOpenGLWidget::mousePressEvent(event);
}

void GLWidget::mouseMoveEvent(QMouseEvent* event) {
    if (m_maskDrawing && m_maskDrawInProgress) {
        m_maskDrawCurrent = QPointF(
            std::clamp(event->position().x(), 0.0, static_cast<double>(width())),
            std::clamp(event->position().y(), 0.0, static_cast<double>(height())));
        update();
        event->accept();
        return;
    }

    if (m_activeEditMask) {
        const int w = width();
        const int h = height();
        if (w > 0 && h > 0) {
            if (m_draggedPointIndex >= 0 && m_draggedPointIndex < static_cast<int>(m_activeEditMask->points.size())) {
                const double px = m_activeEditMask->evalProp(m_activeEditMask->posXCurve, m_activeEditMask->posX, m_editMaskClipTime);
                const double py = m_activeEditMask->evalProp(m_activeEditMask->posYCurve, m_activeEditMask->posY, m_editMaskClipTime);
                const double sx = m_activeEditMask->evalProp(m_activeEditMask->scaleXCurve, m_activeEditMask->scaleX, m_editMaskClipTime);
                const double sy = m_activeEditMask->evalProp(m_activeEditMask->scaleYCurve, m_activeEditMask->scaleY, m_editMaskClipTime);
                const double rotation = m_activeEditMask->evalProp(m_activeEditMask->rotationCurve, m_activeEditMask->rotation, m_editMaskClipTime);
                QTransform transform;
                transform.translate(px * w, py * h);
                transform.rotate(rotation);
                transform.scale(sx, sy);
                bool invertible = false;
                const QTransform inverse = transform.inverted(&invertible);
                if (!invertible) return;
                const QPointF localPoint = inverse.map(event->position());
                float nx = std::clamp(static_cast<float>(localPoint.x() / w + 0.5), 0.0f, 1.0f);
                float ny = std::clamp(static_cast<float>(localPoint.y() / h + 0.5), 0.0f, 1.0f);
                emit maskPointMoved(m_draggedPointIndex, nx, ny);
                update();
                event->accept();
                return;
            } else if (m_draggingMaskCenter) {
                double dx = (event->position().x() - m_dragStartPos.x()) / w;
                double dy = (event->position().y() - m_dragStartPos.y()) / h;
                emit maskTransformChanged(m_dragStartPosX + dx, m_dragStartPosY + dy, m_dragStartScaleX, m_dragStartScaleY, m_dragStartRotation);
                update();
                event->accept();
                return;
            } else if (m_draggingMaskScale) {
                QTransform startTransform;
                startTransform.translate(m_dragStartPosX * w, m_dragStartPosY * h);
                startTransform.rotate(m_dragStartRotation);
                startTransform.scale(m_dragStartScaleX, m_dragStartScaleY);
                bool invertible = false;
                const QTransform inverse = startTransform.inverted(&invertible);
                if (!invertible) return;
                const QPointF localPoint = inverse.map(event->position());
                const double scaleX = std::clamp(m_dragStartScaleX * std::abs(localPoint.x()) / (0.25 * w), 0.01, 4.0);
                const double scaleY = std::clamp(m_dragStartScaleY * std::abs(localPoint.y()) / (0.25 * h), 0.01, 4.0);
                emit maskTransformChanged(m_dragStartPosX, m_dragStartPosY, scaleX, scaleY, m_dragStartRotation);
                update();
                event->accept();
                return;
            } else if (m_draggingMaskRotate) {
                float cx = static_cast<float>(m_dragStartPosX * w);
                float cy = static_cast<float>(m_dragStartPosY * h);
                double angleNow = std::atan2(event->position().y() - cy, event->position().x() - cx) * 180.0 / M_PI;
                double angleStart = std::atan2(m_dragStartPos.y() - cy, m_dragStartPos.x() - cx) * 180.0 / M_PI;
                double dAngle = angleNow - angleStart;
                emit maskTransformChanged(m_dragStartPosX, m_dragStartPosY, m_dragStartScaleX, m_dragStartScaleY, m_dragStartRotation + dAngle);
                update();
                event->accept();
                return;
            }

            int oldHover = m_hoveredPointIndex;
            m_hoveredPointIndex = -1;
            const double px = m_activeEditMask->evalProp(m_activeEditMask->posXCurve, m_activeEditMask->posX, m_editMaskClipTime);
            const double py = m_activeEditMask->evalProp(m_activeEditMask->posYCurve, m_activeEditMask->posY, m_editMaskClipTime);
            const double sx = m_activeEditMask->evalProp(m_activeEditMask->scaleXCurve, m_activeEditMask->scaleX, m_editMaskClipTime);
            const double sy = m_activeEditMask->evalProp(m_activeEditMask->scaleYCurve, m_activeEditMask->scaleY, m_editMaskClipTime);
            const double rotation = m_activeEditMask->evalProp(m_activeEditMask->rotationCurve, m_activeEditMask->rotation, m_editMaskClipTime);
            QTransform transform;
            transform.translate(px * w, py * h);
            transform.rotate(rotation);
            transform.scale(sx, sy);
            for (size_t i = 0; i < m_activeEditMask->points.size(); ++i) {
                QPointF pt = transform.map(QPointF(
                    (m_activeEditMask->points[i].x - 0.5) * w,
                    (m_activeEditMask->points[i].y - 0.5) * h));
                if (QLineF(event->position(), pt).length() <= 8.0) {
                    m_hoveredPointIndex = static_cast<int>(i);
                    break;
                }
            }
            if (oldHover != m_hoveredPointIndex) update();
        }
    }
    QOpenGLWidget::mouseMoveEvent(event);
}

void GLWidget::mouseReleaseEvent(QMouseEvent* event) {
    if (m_maskDrawing && m_maskDrawInProgress && event->button() == Qt::LeftButton) {
        m_maskDrawCurrent = QPointF(
            std::clamp(event->position().x(), 0.0, static_cast<double>(width())),
            std::clamp(event->position().y(), 0.0, static_cast<double>(height())));
        const QRectF rect(m_maskDrawStart, m_maskDrawCurrent);
        const QRectF normalizedRect = rect.normalized();
        const bool hasMinimumSize = normalizedRect.width() >= 4.0 && normalizedRect.height() >= 4.0 &&
            width() > 0 && height() > 0;
        m_maskDrawing = false;
        m_maskDrawInProgress = false;
        unsetCursor();
        if (hasMinimumSize) {
            const double posX = normalizedRect.center().x() / width();
            const double posY = normalizedRect.center().y() / height();
            const double scaleX = normalizedRect.width() / (0.5 * width());
            const double scaleY = normalizedRect.height() / (0.5 * height());
            emit maskDrawn(m_maskDrawingShape, posX, posY, scaleX, scaleY);
        }
        update();
        event->accept();
        return;
    }

    if (m_draggedPointIndex >= 0 || m_draggingMaskCenter || m_draggingMaskRotate || m_draggingMaskScale) {
        m_draggedPointIndex = -1;
        m_draggingMaskCenter = false;
        m_draggingMaskRotate = false;
        m_draggingMaskScale = false;
        update();
        event->accept();
        return;
    }
    QOpenGLWidget::mouseReleaseEvent(event);
}

void GLWidget::compileCustomPluginShader(ShaderPlugin& plugin) {
    if (plugin.compileAttempted) return;
    plugin.compileAttempted = true;

    auto program = std::make_unique<QOpenGLShaderProgram>();
    if (!program->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShaderSource)) {
        qWarning() << "Failed to compile vertex shader for plugin:" << QString::fromStdString(plugin.name)
                   << program->log();
        return;
    }
    QFile file(QString::fromStdString(plugin.fragmentShaderPath));
    if (file.open(QIODevice::ReadOnly)) {
        QString fragSource = file.readAll();
        if (!program->addShaderFromSourceCode(QOpenGLShader::Fragment, fragSource)) {
            qWarning() << "Failed to compile fragment shader for plugin:" << QString::fromStdString(plugin.name)
                       << program->log();
            return;
        }
    } else {
        qWarning() << "Failed to open developer plugin fragment shader:" << QString::fromStdString(plugin.fragmentShaderPath);
        return;
    }
    if (program->link()) {
        plugin.shaderProgram = program->programId();
        uniformLocations.erase(plugin.shaderProgram);
        plugin.isCompiled = true;
        program.release()->setParent(this);
    } else {
        qWarning() << "Failed to link developer plugin shader:" << QString::fromStdString(plugin.name)
                   << program->log();
    }
}

GLint GLWidget::uniformLocation(GLuint program, const char* name) {
    auto& locations = uniformLocations[program];
    const auto found = locations.find(name);
    if (found != locations.end()) return found->second;
    const GLint location = glGetUniformLocation(program, name);
    locations.emplace(name, location);
    return location;
}

void GLWidget::paintGL() {
    renderPipeline(width(), height(), true, false);
}

void GLWidget::renderPipeline(int w, int h, bool toScreen, bool isExport) {
    if (!passthroughShader || !passthroughShader->isLinked()) return;

    QElapsedTimer renderTimer;
    renderTimer.start();
    Profiler::instance().mark("frame_start");

    if (w <= 0) w = width();
    if (h <= 0) h = height();
    allocateFBOs(w, h);
    uploadMaskIfNeeded();
    const bool hasDetectionMask = m_maskEnabled && m_detectionMaskHasCoverage &&
        pendingMaskW > 0 && pendingMaskH > 0 && !pendingMask.empty();
    glDisable(GL_BLEND);

    if (hasNewFrame) {
        std::lock_guard<std::mutex> lock(m_frameMutex);
        if (!isTransitioning) {
            const DecodedVideoFrame& uploaded = sharedCurrentFrame ? *sharedCurrentFrame : currentFrame;
            uploadPrimaryVideoTexture(uploaded);
            const size_t rgbBytes = static_cast<size_t>(uploaded.width) * static_cast<size_t>(uploaded.height) * 3;
            hasBaseTexture = rgbBytes > 0 && uploaded.baseRgb.size() == rgbBytes;
            if (hasBaseTexture) {
                glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, baseTexture);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, uploaded.width, uploaded.height, 0, GL_RGB, GL_UNSIGNED_BYTE, uploaded.baseRgb.data());
            }
        } else {
            hasBaseTexture = false;
            if (!transitionFrame1.rgbData.empty()) {
                uploadPrimaryVideoTexture(transitionFrame1);
            }
            if (!transitionFrame2.rgbData.empty()) {
                glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
                glActiveTexture(GL_TEXTURE1);
                glBindTexture(GL_TEXTURE_2D, videoTexture2);
                const bool hasAlpha = transitionFrame2.hasAlpha && !transitionFrame2.alphaData.empty() && transitionFrame2.alphaData.size() == static_cast<size_t>(transitionFrame2.width) * static_cast<size_t>(transitionFrame2.height);
                if (hasAlpha) {
                    std::vector<uint8_t> rgbaData(static_cast<size_t>(transitionFrame2.width) * static_cast<size_t>(transitionFrame2.height) * 4);
                    for (size_t i = 0; i < rgbaData.size() / 4; ++i) {
                        const size_t srcIndex = i * 3;
                        const size_t dstIndex = i * 4;
                        rgbaData[dstIndex + 0] = transitionFrame2.rgbData[srcIndex + 0];
                        rgbaData[dstIndex + 1] = transitionFrame2.rgbData[srcIndex + 1];
                        rgbaData[dstIndex + 2] = transitionFrame2.rgbData[srcIndex + 2];
                        rgbaData[dstIndex + 3] = transitionFrame2.alphaData[i];
                    }
                    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, transitionFrame2.width, transitionFrame2.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgbaData.data());
                } else {
                    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, transitionFrame2.width, transitionFrame2.height, 0, GL_RGB, GL_UNSIGNED_BYTE, transitionFrame2.rgbData.data());
                }
            }
        }
        hasNewFrame = false;
    }

    auto bindRichUniforms = [&](GLuint prog) {
        GLint timeLoc = uniformLocation(prog, "time");
        if (timeLoc != -1) glUniform1f(timeLoc, (float)m_time);

        GLint resLoc = uniformLocation(prog, "resolution");
        if (resLoc != -1) glUniform2f(resLoc, (float)w, (float)h);

        GLint aspectLoc = uniformLocation(prog, "aspect");
        if (aspectLoc != -1) glUniform1f(aspectLoc, (float)w / std::max(1.0f, (float)h));

        GLint frameLoc = uniformLocation(prog, "frameIndex");
        if (frameLoc != -1) glUniform1i(frameLoc, (int)(m_time * 30.0));

        GLint fpsLoc = uniformLocation(prog, "fps");
        if (fpsLoc != -1) glUniform1f(fpsLoc, 30.0f);

        QPoint pt = mapFromGlobal(QCursor::pos());
        float normMouseX = (float)pt.x() / std::max(1.0f, (float)w);
        float normMouseY = (float)(h - pt.y()) / std::max(1.0f, (float)h);
        bool isMouseDown = (QApplication::mouseButtons() & Qt::LeftButton);

        GLint mouseLoc = uniformLocation(prog, "mouse");
        if (mouseLoc != -1) glUniform2f(mouseLoc, normMouseX, normMouseY);

        GLint mouseXLoc = uniformLocation(prog, "mouseX");
        if (mouseXLoc != -1) glUniform1f(mouseXLoc, normMouseX);

        GLint mouseYLoc = uniformLocation(prog, "mouseY");
        if (mouseYLoc != -1) glUniform1f(mouseYLoc, normMouseY);

        GLint mousePressLoc = uniformLocation(prog, "mousePressed");
        if (mousePressLoc != -1) glUniform1i(mousePressLoc, isMouseDown ? 1 : 0);

        float b = AudioEngine::instance().getBass();
        float m = AudioEngine::instance().getMid();
        float t = AudioEngine::instance().getHigh();

        GLint bassLoc = uniformLocation(prog, "audioBass");
        if (bassLoc != -1) glUniform1f(bassLoc, b);
        GLint midLoc = uniformLocation(prog, "audioMid");
        if (midLoc != -1) glUniform1f(midLoc, m);
        GLint trebleLoc = uniformLocation(prog, "audioTreble");
        if (trebleLoc != -1) glUniform1f(trebleLoc, t);
        GLint volLoc = uniformLocation(prog, "audioVolume");
        if (volLoc != -1) glUniform1f(volLoc, (b + m + t) / 3.0f);
    };

    GLuint currentTex = videoTexture;
    QOpenGLFramebufferObject* readFbo = fboPing;
    QOpenGLFramebufferObject* writeFbo = fboPong;

    if (isTransitioning) {
        ShaderPlugin* plugin = PluginManager::instance().findPlugin(currentTransitionPlugin);
        if (plugin) {
            compileCustomPluginShader(*plugin);
            if (plugin->isCompiled && plugin->shaderProgram > 0) {
                writeFbo->bind();
                glViewport(0, 0, w, h);
                glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
                glClear(GL_COLOR_BUFFER_BIT);

                glUseProgram(plugin->shaderProgram);

                GLint videoTexLoc = uniformLocation(plugin->shaderProgram, "videoTexture");
                glUniform1i(videoTexLoc, 0);
                GLint videoTex2Loc = uniformLocation(plugin->shaderProgram, "videoTexture2");
                glUniform1i(videoTex2Loc, 1);
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, videoTexture);
                glActiveTexture(GL_TEXTURE1);
                glBindTexture(GL_TEXTURE_2D, videoTexture2);

                GLint progressLoc = uniformLocation(plugin->shaderProgram, "progress");
                if (progressLoc != -1) glUniform1f(progressLoc, (float)transitionProgress);

                bindRichUniforms(plugin->shaderProgram);

                renderQuad();

                glUseProgram(0);
                writeFbo->release();

                currentTex = writeFbo->texture();
                std::swap(readFbo, writeFbo);
            }
        }
    }

    // Three scratch targets guarantee a pass never writes a texture it samples.
    auto pickScratchFbo = [&](GLuint avoidA, GLuint avoidB = 0) {
        for (QOpenGLFramebufferObject* candidate : { fboPing, fboPong, fboMask }) {
            if (candidate->texture() != avoidA && candidate->texture() != avoidB) return candidate;
        }
        return fboPing;
    };

    bool anyEffectApplied = false;
    if (hasBaseTexture && !isTransitioning && !m_decoderEffectIds.empty() &&
        maskCompositeShader && maskCompositeShader->isLinked()) {
        // Datamosh and CPU bitwise effects are baked into the decoded frame.
        // Re-blend the untouched frame through the clip mask so they honor it.
        for (const std::string& decoderEffectId : m_decoderEffectIds) {
            const uint64_t sig = MaskGenerator::signature(w, h, m_clipMasks, m_clipMaskLocalTime, decoderEffectId);
            if (sig == 0) continue;
            if (m_uploadedMaskSignature != sig) {
                auto cached = m_maskRasterCache.find(sig);
                if (cached == m_maskRasterCache.end()) {
                    if (m_maskRasterCache.size() > 16) m_maskRasterCache.clear();
                    cached = m_maskRasterCache.emplace(sig, MaskGenerator::renderMask(
                        w, h, m_clipMasks, m_clipMaskLocalTime, decoderEffectId)).first;
                }
                uploadMaskPixels(cached->second, w, h, true);
                m_uploadedMaskSignature = hasMaskTexture ? sig : 0;
            }
            if (!hasMaskTexture) break;
            QOpenGLFramebufferObject* gateFbo = pickScratchFbo(currentTex);
            gateFbo->bind();
            glViewport(0, 0, w, h);
            maskCompositeShader->bind();
            glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, baseTexture);
            maskCompositeShader->setUniformValue("sourceTexture", 0);
            glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, currentTex);
            maskCompositeShader->setUniformValue("effectedTexture", 1);
            glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, maskTexture);
            maskCompositeShader->setUniformValue("maskTexture", 2);
            maskCompositeShader->setUniformValue("invertMask", 0.0f);
            renderQuad();
            maskCompositeShader->release();
            gateFbo->release();
            currentTex = gateFbo->texture();
            break;
        }
    }
    for (const AppliedEffect& eff : activeEffects) {
        Profiler::instance().mark("effect_" + eff.pluginId);
        ShaderPlugin* plugin = PluginManager::instance().findPlugin(eff.pluginId);
        if (plugin) {
            compileCustomPluginShader(*plugin);
            if (plugin->isCompiled && plugin->shaderProgram > 0) {
                // Clip masks are rasterized for the current output resolution
                // and effect ID, then applied by the same composite pass in
                // preview and export. The detector mask remains a fallback.
                const uint64_t maskSignature = MaskGenerator::signature(
                    w, h, m_clipMasks, m_clipMaskLocalTime, eff.pluginId);
                bool hasClipMask = maskSignature != 0;
                if (hasClipMask) {
                    if (m_uploadedMaskSignature != maskSignature) {
                        auto cached = m_maskRasterCache.find(maskSignature);
                        if (cached == m_maskRasterCache.end()) {
                            if (m_maskRasterCache.size() > 16) m_maskRasterCache.clear();
                            std::vector<uint8_t> pixels = MaskGenerator::renderMask(
                                w, h, m_clipMasks, m_clipMaskLocalTime, eff.pluginId);
                            cached = m_maskRasterCache.emplace(maskSignature, std::move(pixels)).first;
                        }
                        // MaskGenerator rasterizes with QPainter's top-left origin;
                        // video textures are uploaded bottom-up for the GL preview.
                        uploadMaskPixels(cached->second, w, h, true);
                        m_uploadedMaskSignature = hasMaskTexture ? maskSignature : 0;
                    }
                    hasClipMask = hasMaskTexture;
                }
                const bool detectionMaskTargetsEffect = m_detectionMaskEffectIds.empty() ||
                    std::find(m_detectionMaskEffectIds.begin(), m_detectionMaskEffectIds.end(), eff.pluginId) !=
                        m_detectionMaskEffectIds.end();
                const bool effectHasDetectionMask = hasDetectionMask && detectionMaskTargetsEffect;
                if (!hasClipMask && effectHasDetectionMask) {
                    uploadMaskPixels(pendingMask, pendingMaskW, pendingMaskH);
                    m_uploadedMaskSignature = 0;
                } else if (!hasClipMask) {
                    hasMaskTexture = false;
                    m_uploadedMaskSignature = 0;
                }
                const bool effectMaskEnabled = hasClipMask || effectHasDetectionMask;
                const float effectMaskInverted = hasClipMask ? 0.0f : (m_maskInverted ? 1.0f : 0.0f);

                const GLuint sourceTex = currentTex;
                writeFbo = pickScratchFbo(sourceTex);
                writeFbo->bind();
                glViewport(0, 0, w, h);
                glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
                glClear(GL_COLOR_BUFFER_BIT);

                glUseProgram(plugin->shaderProgram);

                bindRichUniforms(plugin->shaderProgram);

                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, currentTex);
                GLint videoTexLoc = uniformLocation(plugin->shaderProgram, "videoTexture");
                if (videoTexLoc != -1) glUniform1i(videoTexLoc, 0);
                GLint currentTexLoc = uniformLocation(plugin->shaderProgram, "currentTexture");
                if (currentTexLoc != -1) glUniform1i(currentTexLoc, 0);

                glActiveTexture(GL_TEXTURE1);
                glBindTexture(GL_TEXTURE_2D, fboFeedback->texture());
                GLint feedbackTexLoc = uniformLocation(plugin->shaderProgram, "feedbackTexture");
                if (feedbackTexLoc != -1) glUniform1i(feedbackTexLoc, 1);
                GLint blendTexLoc = uniformLocation(plugin->shaderProgram, "blendTexture");
                if (blendTexLoc != -1) glUniform1i(blendTexLoc, 1);

                glActiveTexture(GL_TEXTURE2);
                glBindTexture(GL_TEXTURE_2D, maskTexture);
                GLint maskTexLoc = uniformLocation(plugin->shaderProgram, "maskTexture");
                if (maskTexLoc != -1) glUniform1i(maskTexLoc, 2);
                GLint hasMaskLoc = uniformLocation(plugin->shaderProgram, "hasMask");
                if (hasMaskLoc != -1) glUniform1i(hasMaskLoc, effectMaskEnabled ? 1 : 0);

                for (const auto& param : eff.parameters) {
                    GLint paramLoc = uniformLocation(plugin->shaderProgram, param.name.c_str());
                    if (paramLoc != -1) {
                        double paramVal = param.curve.getKeyframes().empty() ? param.currentVal : param.curve.evaluate(m_time);
                        glUniform1f(paramLoc, (float)paramVal);
                    }
                }

                renderQuad();

                glUseProgram(0);
                writeFbo->release();

                Profiler::instance().sample("effect_" + eff.pluginId, Profiler::instance().elapsed("effect_" + eff.pluginId));

                currentTex = writeFbo->texture();
                anyEffectApplied = true;

                if (effectMaskEnabled && hasMaskTexture &&
                    maskCompositeShader && maskCompositeShader->isLinked()) {
                    QOpenGLFramebufferObject* compositeFbo = pickScratchFbo(sourceTex, currentTex);
                    compositeFbo->bind();
                    glViewport(0, 0, w, h);
                    maskCompositeShader->bind();
                    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, sourceTex);
                    maskCompositeShader->setUniformValue("sourceTexture", 0);
                    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, currentTex);
                    maskCompositeShader->setUniformValue("effectedTexture", 1);
                    glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, maskTexture);
                    maskCompositeShader->setUniformValue("maskTexture", 2);
                    maskCompositeShader->setUniformValue("invertMask", effectMaskInverted);
                    renderQuad();
                    maskCompositeShader->release();
                    compositeFbo->release();
                    currentTex = compositeFbo->texture();
                }
            }
        }
    }
    // When the source video contains alpha (e.g. transparent ProRes 4444),
    // restore the source alpha channel once after the effect chain so custom
    // shaders that write opaque RGB do not erase transparent backgrounds.
    if (anyEffectApplied && lastFrameHasAlpha && alphaGuardShader && alphaGuardShader->isLinked()) {
        writeFbo = pickScratchFbo(currentTex);
        writeFbo->bind();
        glViewport(0, 0, w, h);
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        alphaGuardShader->bind();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, videoTexture);
        alphaGuardShader->setUniformValue("sourceTexture", 0);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, currentTex);
        alphaGuardShader->setUniformValue("effectedTexture", 1);
        renderQuad();
        alphaGuardShader->release();
        writeFbo->release();
        currentTex = writeFbo->texture();
    }

    if (fboFeedback && passthroughShader && passthroughShader->isLinked()) {
        fboFeedback->bind();
        glViewport(0, 0, w, h);
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        passthroughShader->bind();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, currentTex);
        passthroughShader->setUniformValue("videoTexture", 0);
        renderQuad();
        passthroughShader->release();
        fboFeedback->release();
    }

    if (toScreen) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glBindFramebuffer(GL_FRAMEBUFFER, defaultFramebufferObject());
        glViewport(0, 0, w, h);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        if (transparencyGridShader && transparencyGridShader->isLinked()) {
            glDisable(GL_BLEND);
            transparencyGridShader->bind();
            renderQuad();
            transparencyGridShader->release();
            glEnable(GL_BLEND);
        }

        passthroughShader->bind();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, currentTex);
        passthroughShader->setUniformValue("videoTexture", 0);
        renderQuad();
        passthroughShader->release();

        double frameRenderMs = renderTimer.nsecsElapsed() / 1.0e6;
        m_lastRenderTimeMs = frameRenderMs;
        m_renderTimeHistory.push_back(frameRenderMs);
        if (m_renderTimeHistory.size() > 30) {
            m_renderTimeHistory.pop_front();
        }
        double sum = 0.0;
        for (double v : m_renderTimeHistory) sum += v;
        m_avgRenderTimeMs = m_renderTimeHistory.empty() ? 0.0 : (sum / m_renderTimeHistory.size());

        Profiler::instance().sample("frame_render", frameRenderMs);

        frameCount++;
        if (fpsTimer.elapsed() >= 500) {
            currentFps = frameCount / (fpsTimer.elapsed() / 1000.0);
            frameCount = 0;
            fpsTimer.restart();
        }
        if (showOverlay && overlayLabel) {
            size_t cacheCount = VideoEngine::instance().getCacheFrameCount();
            size_t cacheBytes = VideoEngine::instance().getCacheByteSize();
            overlayLabel->setText(QString("FPS: %1\nRender: %2 ms (Avg: %3 ms)\nRes: %4x%5\nCache: %6 f (%7 MB)")
                .arg(currentFps, 0, 'f', 1)
                .arg(m_lastRenderTimeMs, 0, 'f', 1)
                .arg(m_avgRenderTimeMs, 0, 'f', 1)
                .arg(lastFrameWidth)
                .arg(lastFrameHeight)
                .arg(cacheCount)
                .arg(cacheBytes / (1024 * 1024)));
            overlayLabel->adjustSize();
        }
    }

    if (isExport && exportFbo) {
        exportFbo->bind();
        glViewport(0, 0, w, h);
        glDisable(GL_BLEND);
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        auto* blitShader = (exportBlitShader && exportBlitShader->isLinked()) ? exportBlitShader : passthroughShader;
        blitShader->bind();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, currentTex);
        blitShader->setUniformValue("videoTexture", 0);
        renderQuad();
        blitShader->release();
        exportFbo->release();
    }

    renderedTexture = currentTex;
}

void GLWidget::renderQuad() {
    quadVao.bind();
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    quadVao.release();
}
