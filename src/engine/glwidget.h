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
#include <mutex>
#include "engine/pluginmanager.h"
#include "engine/videoengine.h"
#include "core/project.h"

class GLWidget : public QOpenGLWidget, protected QOpenGLExtraFunctions {
    Q_OBJECT
public:
    explicit GLWidget(QWidget* parent = nullptr);
    ~GLWidget();

    void updateFrame(const DecodedVideoFrame& frame);
    void updateTransitionFrames(const DecodedVideoFrame& frame1, const DecodedVideoFrame& frame2, double progress, const std::string& transitionPluginId);
    void clearFrame();

    void setPlaybackTime(double time);

    void setActiveEffects(const std::vector<AppliedEffect>& effects);
    void setShowOverlay(bool show);

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

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
    int lastFrameWidth = 0;
    int lastFrameHeight = 0;

    QOpenGLFramebufferObject* fboPing = nullptr;
    QOpenGLFramebufferObject* fboPong = nullptr;
    QOpenGLFramebufferObject* fboFeedback = nullptr; 

    QOpenGLShaderProgram* passthroughShader = nullptr;

    std::vector<AppliedEffect> activeEffects;

    QOpenGLBuffer quadVbo;
    QOpenGLVertexArrayObject quadVao;
    void initShaders();
    void allocateFBOs(int w, int h);
    void renderQuad();
    void compileCustomPluginShader(ShaderPlugin& plugin);
};

#endif 
