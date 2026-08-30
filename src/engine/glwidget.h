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
#include <QImage>
#include <mutex>
#include <vector>
#include <unordered_map>
#include <array>
#include "engine/pluginmanager.h"
#include "engine/videoengine.h"
#include "engine/detector.h"
#include "engine/renderbackend.h"
#include "core/project.h"

class GLWidget : public QOpenGLWidget, protected QOpenGLExtraFunctions {
    Q_OBJECT
public:
    explicit GLWidget(QWidget* parent = nullptr);
    ~GLWidget();

    void updateFrame(const DecodedVideoFrame& frame);
    void updateFrame(DecodedVideoFrame&& frame);
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

    void setMaskEnabled(bool enabled);
    bool maskEnabled() const { return m_maskEnabled; }
    void setMaskData(int width, int height, const std::vector<uint8_t>& maskR);
    QImage grabRenderedFrame();

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;
    void paintEvent(QPaintEvent* event) override;

private:
    double m_time = 0.0;
    bool hasNewFrame = false;
    DecodedVideoFrame currentFrame;
    QElapsedTimer fpsTimer;
    int frameCount = 0;
    double currentFps = 0.0;
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
    bool m_maskEnabled = false;
    bool maskDirty = false;
    std::vector<uint8_t> pendingMask;
    int pendingMaskW = 0;
    int pendingMaskH = 0;

    bool m_showDetections = true;
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
