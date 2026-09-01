#include "glwidget.h"
#include <QOpenGLFramebufferObjectFormat>
#include <QApplication>
#include <QCursor>
#include <QPainter>
#include <QPaintEvent>
#include <QPainterPath>
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
    overlayLabel = new QLabel(this);
    overlayLabel->setStyleSheet(
        "QLabel {"
        "  color: #f59ef8;"
        "  background-color: rgba(14, 10, 18, 200);"
        "  border: 1px solid #4a1d5e;"
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
    if (maskTexture) glDeleteTextures(1, &maskTexture);
    glDeleteBuffers(static_cast<GLsizei>(uploadPbos.size()), uploadPbos.data());
    quadVao.destroy();
    quadVbo.destroy();
    delete passthroughShader;
    delete transparencyGridShader;
    delete maskCompositeShader;
    delete alphaGuardShader;
    delete fboPing;
    delete fboPong;
    delete fboFeedback;
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

}

void GLWidget::allocateFBOs(int w, int h) {
    if (w <= 0 || h <= 0) return;

    if (fboPing && fboPing->width() == w && fboPing->height() == h) {
        return;
    }

    delete fboPing;
    delete fboPong;
    delete fboFeedback;
    delete exportFbo;

    QOpenGLFramebufferObjectFormat format;
    format.setAttachment(QOpenGLFramebufferObject::NoAttachment);
    format.setInternalTextureFormat(GL_RGBA);

    fboPing = new QOpenGLFramebufferObject(w, h, format);
    fboPong = new QOpenGLFramebufferObject(w, h, format);
    fboFeedback = new QOpenGLFramebufferObject(w, h, format);
    exportFbo = new QOpenGLFramebufferObject(w, h, format);

    for (QOpenGLFramebufferObject* fbo : { fboPing, fboPong, fboFeedback, exportFbo }) {
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
    update();
}

void GLWidget::uploadMaskIfNeeded() {
    if (!maskDirty || !maskTexture) return;
    maskDirty = false;

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, maskTexture);
    if (pendingMaskW <= 0 || pendingMaskH <= 0 || pendingMask.empty() || !hasMaskTexture) {
        unsigned char whitePixel = 255;
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, 1, 1, 0, GL_RED, GL_UNSIGNED_BYTE, &whitePixel);
        lastMaskWidth = 1;
        lastMaskHeight = 1;
        hasMaskTexture = false;
        return;
    }

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    if (lastMaskWidth != pendingMaskW || lastMaskHeight != pendingMaskH) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, pendingMaskW, pendingMaskH, 0, GL_RED, GL_UNSIGNED_BYTE, pendingMask.data());
        lastMaskWidth = pendingMaskW;
        lastMaskHeight = pendingMaskH;
    } else {
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, pendingMaskW, pendingMaskH, GL_RED, GL_UNSIGNED_BYTE, pendingMask.data());
    }
    hasMaskTexture = true;
}

void GLWidget::paintEvent(QPaintEvent* event) {
    QOpenGLWidget::paintEvent(event);

    if (m_guideOverlay == GuideOverlay::None && (!m_showDetections || detections.empty())) return;

    QPainter painter(this);
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

    if (!m_showDetections || detections.empty()) return;

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

    const size_t visibleCount = std::min(
        detections.size(), static_cast<size_t>(m_detectionOverlayOptions.maxDetections));

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
                painter.drawLine(QPointF(from.x() * width(), from.y() * height()),
                    QPointF(to.x() * width(), to.y() * height()));
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
            QPainterPath trailPath(QPointF(trail[first].x * width(), trail[first].y * height()));
            for (size_t pointIndex = first + 1; pointIndex < trail.size(); ++pointIndex) {
                trailPath.lineTo(QPointF(trail[pointIndex].x * width(), trail[pointIndex].y * height()));
            }
            painter.drawPath(trailPath);
        }
        painter.restore();
    }

    for (size_t i = 0; i < visibleCount; ++i) {
        const auto& box = detections[i];
        const QColor color = colorFor(box, static_cast<int>(i));
        const QRectF rect(
            box.x * width(),
            box.y * height(),
            box.w * width(),
            box.h * height()
        );

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
            QPainterPath outlinePath(QPointF(box.outline.front().x * width(), box.outline.front().y * height()));
            for (size_t pointIndex = 1; pointIndex < box.outline.size(); ++pointIndex) {
                outlinePath.lineTo(QPointF(box.outline[pointIndex].x * width(), box.outline[pointIndex].y * height()));
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
        painter.setFont(font);

        QFontMetrics fm(font);
        const QRect textRect = fm.boundingRect(label).adjusted(-4, -2, 4, 2);
        QRect badge(
            std::clamp(static_cast<int>(rect.left()), 0, std::max(0, width() - textRect.width())),
            std::max(0, static_cast<int>(rect.top()) - textRect.height()),
            textRect.width(),
            textRect.height()
        );

        painter.fillRect(badge, QColor(color.red(), color.green(), color.blue(), 210));
        painter.setPen(QColor(20, 16, 28));
        painter.drawText(badge, Qt::AlignCenter, label);
    }
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
    if (!passthroughShader || !passthroughShader->isLinked()) return;

    Profiler::instance().mark("frame_start");

    int w = width();
    int h = height();
    allocateFBOs(w, h);
    uploadMaskIfNeeded();
    glDisable(GL_BLEND);

    if (hasNewFrame) {
        std::lock_guard<std::mutex> lock(m_frameMutex);
        if (!isTransitioning) {
            if (sharedCurrentFrame) {
                uploadPrimaryVideoTexture(*sharedCurrentFrame);
            } else {
                uploadPrimaryVideoTexture(currentFrame);
            }
        } else {
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

    bool anyEffectApplied = false;
    for (const AppliedEffect& eff : activeEffects) {
        Profiler::instance().mark("effect_" + eff.pluginId);
        ShaderPlugin* plugin = PluginManager::instance().findPlugin(eff.pluginId);
        if (plugin) {
            compileCustomPluginShader(*plugin);
            if (plugin->isCompiled && plugin->shaderProgram > 0) {
                const GLuint sourceTex = currentTex;
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
                if (hasMaskLoc != -1) glUniform1i(hasMaskLoc, (m_maskEnabled && hasMaskTexture) ? 1 : 0);

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
                std::swap(readFbo, writeFbo);
                anyEffectApplied = true;

                if (m_maskEnabled && hasMaskTexture &&
                    maskCompositeShader && maskCompositeShader->isLinked()) {
                    writeFbo->bind();
                    glViewport(0, 0, w, h);
                    maskCompositeShader->bind();
                    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, sourceTex);
                    maskCompositeShader->setUniformValue("sourceTexture", 0);
                    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, currentTex);
                    maskCompositeShader->setUniformValue("effectedTexture", 1);
                    glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, maskTexture);
                    maskCompositeShader->setUniformValue("maskTexture", 2);
                    maskCompositeShader->setUniformValue("invertMask", m_maskInverted ? 1.0f : 0.0f);
                    renderQuad();
                    maskCompositeShader->release();
                    writeFbo->release();
                    currentTex = writeFbo->texture();
                    std::swap(readFbo, writeFbo);
                }
            }
        }
    }

    // When the source video contains alpha (e.g. transparent ProRes 4444),
    // restore the source alpha channel once after the effect chain so custom
    // shaders that write opaque RGB do not erase transparent backgrounds.
    if (anyEffectApplied && lastFrameHasAlpha && alphaGuardShader && alphaGuardShader->isLinked()) {
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
        std::swap(readFbo, writeFbo);
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

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBindFramebuffer(GL_FRAMEBUFFER, defaultFramebufferObject());
    glViewport(0, 0, w, h);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Transparency has no visible color on a native window surface and is
    // commonly shown as black by Windows. Draw an editor-only checkerboard so
    // alpha is unmistakable; grabRenderedFrame() never reads this framebuffer.
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
    renderedTexture = currentTex;
    frameCount++;
    if (fpsTimer.elapsed() > 1000) {
        currentFps = frameCount / (fpsTimer.elapsed() / 1000.0);
        frameCount = 0;
        fpsTimer.restart();
    }
    if (showOverlay && overlayLabel) {
        overlayLabel->setText(QString("FPS: %1\nRes: %2x%3")
            .arg(currentFps, 0, 'f', 1)
            .arg(lastFrameWidth)
            .arg(lastFrameHeight));
        overlayLabel->adjustSize();
    }
}

void GLWidget::renderQuad() {
    quadVao.bind();
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    quadVao.release();
}
