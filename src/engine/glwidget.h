#ifndef GLWIDGET_H
#define GLWIDGET_H

#include <QOpenGLWidget>
#include <QOpenGLExtraFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLFramebufferObject>
#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>
#include <QElapsedTimer>
#include <QLabel>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QImage>
#include <mutex>
#include <vector>
#include <deque>
#include <unordered_map>
#include <array>
#include <memory>
#include "engine/pluginmanager.h"
#include "engine/videoengine.h"
#include "engine/detector.h"
#include "engine/renderbackend.h"
#include "core/project.h"

enum class DetectionOverlayStyle {
    Rectangle,
    CornerBrackets,
    RoundedRectangle,
    Ellipse
};

enum class DetectionColorMode {
    ByTrack,
    ByClass,
    Fixed
};

// Whole-clip detections are sampled sparsely. This controls how the preview
// travels between those samples without changing the underlying tracks.
enum class DetectionTrackMotion {
    Stepped,
    Smooth,
    NearestSample
};

struct DetectionOverlayOptions {
    DetectionOverlayStyle style = DetectionOverlayStyle::CornerBrackets;
    DetectionColorMode colorMode = DetectionColorMode::ByTrack;
    bool showLabels = true;
    bool showConfidence = true;
    bool showTrackIds = false;
    bool showTrails = true;
    bool showLinks = false;
    bool showCenters = false;
    bool showPersonOutline = true;
    bool replacePersonBoxesWithOutline = true;
    int lineWidth = 2;
    int labelPointSize = 15;
    int maxDetections = 64;
    int fillOpacity = 0;
    int trailLength = 30;
    int trailWidth = 2;
    int trailOpacity = 180;
    float linkDistance = 0.25f;
};

class GLWidget : public QOpenGLWidget, protected QOpenGLExtraFunctions {
    Q_OBJECT
public:
    enum class GuideOverlay {
        None,
        Center,
        RuleOfThirds,
        SafeAreas,
        Grid
    };

    explicit GLWidget(QWidget* parent = nullptr);
    ~GLWidget();

    void updateFrame(const DecodedVideoFrame& frame);
    void updateFrame(DecodedVideoFrame&& frame);
    void updateFrame(std::shared_ptr<const DecodedVideoFrame> frame);
    void updateTransitionFrames(const DecodedVideoFrame& frame1, const DecodedVideoFrame& frame2, double progress, const std::string& transitionPluginId);
    void clearFrame();

    void setPlaybackTime(double time);

    void setActiveEffects(const std::vector<AppliedEffect>& effects);
    void setShowOverlay(bool show);
    void setAsyncTextureUploads(bool enabled);
    bool asyncTextureUploads() const { return m_asyncTextureUploads; }
    void setRendererBackend(RenderBackendKind backend);
    RenderBackendKind rendererBackend() const { return m_rendererBackend; }

    void setDetections(const std::vector<DetectionBox>& boxes);
    void setShowDetections(bool show);
    bool showDetections() const { return m_showDetections; }
    void setDetectionOverlayOptions(const DetectionOverlayOptions& options);
    void setDetectionShape(DetectionShape shape);
    void setGuideOverlay(GuideOverlay guide);

    void setMaskEnabled(bool enabled);
    bool maskEnabled() const { return m_maskEnabled; }
    void setMaskInverted(bool inverted);
    void setDetectionMaskEffectIds(std::vector<std::string> effectIds);
    bool maskInverted() const { return m_maskInverted; }
    void setMaskData(int width, int height, const std::vector<uint8_t>& maskR);
    void setMaskData(int width, int height, std::vector<uint8_t>&& maskR);
    void setClipMasks(const std::vector<ClipMask>& masks, double clipLocalTime);
    // Draw the current timeline/effect state immediately. Export uses this to
    // avoid capturing a queued, stale QOpenGLWidget paint.
    void renderFrameNow();
    QImage grabRenderedFrame();
    bool renderExportFrame(int targetW, int targetH, std::vector<uint8_t>& outRgbaBuffer);

    // Interactive Mask Editing
    void setActiveEditMask(const ClipMask* mask);
    void beginMaskDrawing(MaskShapeType shape);
    const ClipMask* activeEditMask() const { return m_activeEditMask; }
    void setEditMaskClipTime(double localTime) { m_editMaskClipTime = localTime; update(); }

signals:
    void maskPointMoved(int pointIndex, float newX, float newY);
    void maskTransformChanged(double posX, double posY, double scaleX, double scaleY, double rotation);
    void maskDrawn(MaskShapeType shape, double posX, double posY, double scaleX, double scaleY);

public:

    double getCurrentFps() const { return currentFps; }
    double getLastRenderTimeMs() const { return m_lastRenderTimeMs; }
    double getAvgRenderTimeMs() const { return m_avgRenderTimeMs; }
    int getLastFrameWidth() const { return lastFrameWidth; }
    int getLastFrameHeight() const { return lastFrameHeight; }

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    double m_time = 0.0;
    bool hasNewFrame = false;
    DecodedVideoFrame currentFrame;
    std::shared_ptr<const DecodedVideoFrame> sharedCurrentFrame;
    QElapsedTimer fpsTimer;
    int frameCount = 0;
    double currentFps = 0.0;
    double m_lastRenderTimeMs = 0.0;
    double m_avgRenderTimeMs = 0.0;
    std::deque<double> m_renderTimeHistory;
    bool showOverlay = true;
    QLabel* overlayLabel = nullptr;

    std::mutex m_frameMutex;
    bool isTransitioning = false;
    DecodedVideoFrame transitionFrame1;
    DecodedVideoFrame transitionFrame2;
    double transitionProgress = 0.0;
    std::string currentTransitionPlugin;

    GLuint videoTexture = 0;
    GLuint videoTexture2 = 0;
    GLuint maskTexture = 0;
    std::array<GLuint, 3> uploadPbos{};
    unsigned int nextUploadPbo = 0;
    bool m_asyncTextureUploads = true;
    RenderBackendKind m_rendererBackend = RenderBackendKind::OpenGLPipelined;
    int lastFrameWidth = 0;
    int lastFrameHeight = 0;
    bool lastFrameHasAlpha = false;
    int lastMaskWidth = 0;
    int lastMaskHeight = 0;
    bool hasMaskTexture = false;
    bool m_detectionMaskHasCoverage = false;
    bool m_maskEnabled = false;
    bool m_maskInverted = false;
    bool maskDirty = false;
    std::vector<uint8_t> pendingMask;
    int pendingMaskW = 0;
    int pendingMaskH = 0;
    std::vector<std::string> m_detectionMaskEffectIds;
    std::vector<ClipMask> m_clipMasks;
    double m_clipMaskLocalTime = 0.0;

    bool m_showDetections = true;
    DetectionShape m_detectionShape = DetectionShape::Rectangle;
    DetectionOverlayOptions m_detectionOverlayOptions;
    GuideOverlay m_guideOverlay = GuideOverlay::None;
    std::vector<DetectionBox> detections;

    QOpenGLFramebufferObject* fboPing = nullptr;
    QOpenGLFramebufferObject* fboPong = nullptr;
    QOpenGLFramebufferObject* fboFeedback = nullptr;
    QOpenGLFramebufferObject* exportFbo = nullptr;
    GLuint renderedTexture = 0;

    QOpenGLShaderProgram* passthroughShader = nullptr;
    QOpenGLShaderProgram* transparencyGridShader = nullptr;
    QOpenGLShaderProgram* maskCompositeShader = nullptr;
    QOpenGLShaderProgram* alphaGuardShader = nullptr;
    QOpenGLShaderProgram* exportBlitShader = nullptr;
    void renderPipeline(int w, int h, bool toScreen, bool isExport);
    void uploadMaskPixels(const std::vector<uint8_t>& pixels, int width, int height, bool flipVertical = false);
    void drawDetectionExportOverlay(QPainter& painter, int targetW, int targetH) const;

    // Mask editing state
    const ClipMask* m_activeEditMask = nullptr;
    double m_editMaskClipTime = 0.0;
    int m_hoveredPointIndex = -1;
    int m_draggedPointIndex = -1;
    bool m_draggingMaskCenter = false;
    bool m_draggingMaskRotate = false;
    bool m_draggingMaskScale = false;
    bool m_maskDrawing = false;
    bool m_maskDrawInProgress = false;
    MaskShapeType m_maskDrawingShape = MaskShapeType::Rectangle;
    QPointF m_maskDrawStart;
    QPointF m_maskDrawCurrent;
    QPointF m_dragStartPos;
    double m_dragStartPosX = 0.5;
    double m_dragStartPosY = 0.5;
    double m_dragStartScaleX = 1.0;
    double m_dragStartScaleY = 1.0;
    double m_dragStartRotation = 0.0;
    void renderMaskDirectOverlay(QPainter& painter);


    std::vector<AppliedEffect> activeEffects;

    QOpenGLBuffer quadVbo;
    QOpenGLVertexArrayObject quadVao;
    void initShaders();
    void allocateFBOs(int w, int h);
    void renderQuad();
    void compileCustomPluginShader(ShaderPlugin& plugin);
    void uploadMaskIfNeeded();
    void uploadPrimaryVideoTexture(const DecodedVideoFrame& frame);
    GLint uniformLocation(GLuint program, const char* name);
    std::unordered_map<GLuint, std::unordered_map<std::string, GLint>> uniformLocations;
};

#endif
