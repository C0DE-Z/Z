#include "glwidget.h"
#include <QOpenGLFramebufferObjectFormat>
#include <QApplication>
#include <QCursor>
#include <cmath>
#include <QDebug>
#include <QFile>
#include <QIODevice>
#include "engine/audioengine.h"

#include "engine/shaders.h"

GLWidget::GLWidget(QWidget* parent) : QOpenGLWidget(parent) {
    QSurfaceFormat format;
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::NoProfile);
    setFormat(format);
    fpsTimer.start();
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
    quadVao.destroy();
    quadVbo.destroy();
    delete passthroughShader;
    delete fboPing;
    delete fboPong;
    delete fboFeedback;
    doneCurrent();
}

void GLWidget::initializeGL() {
    initializeOpenGLFunctions();
    glClearColor(0.04f, 0.04f, 0.06f, 1.0f);

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

    glGenTextures(1, &videoTexture2);
    glBindTexture(GL_TEXTURE_2D, videoTexture2);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    unsigned char blackPixel[3] = { 0, 0, 0 };
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, blackPixel);
}

void GLWidget::initShaders() {
    passthroughShader = new QOpenGLShaderProgram(this);
    passthroughShader->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShaderSource);
    passthroughShader->addShaderFromSourceCode(QOpenGLShader::Fragment, passthroughShaderSource);
    if (!passthroughShader->link()) {
        qWarning() << "Failed to link passthrough shader:" << passthroughShader->log();
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

    QOpenGLFramebufferObjectFormat format;
    format.setAttachment(QOpenGLFramebufferObject::NoAttachment);
    format.setInternalTextureFormat(GL_RGBA);

    fboPing = new QOpenGLFramebufferObject(w, h, format);
    fboPong = new QOpenGLFramebufferObject(w, h, format);
    fboFeedback = new QOpenGLFramebufferObject(w, h, format);

    fboFeedback->bind();
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    fboFeedback->release();
}

void GLWidget::resizeGL(int w, int h) {
    glViewport(0, 0, w, h);
    allocateFBOs(w, h);
}

void GLWidget::updateFrame(const DecodedVideoFrame& frame) {
    std::lock_guard<std::mutex> lock(m_frameMutex);
    currentFrame = frame;
    hasNewFrame = true;
    isTransitioning = false;
    update();
}

void GLWidget::updateTransitionFrames(const DecodedVideoFrame& f1, const DecodedVideoFrame& f2, double progress, const std::string& pluginId) {
    std::lock_guard<std::mutex> lock(m_frameMutex);
    transitionFrame1 = f1;
    transitionFrame2 = f2;
    transitionProgress = progress;
    currentTransitionPlugin = pluginId;
    isTransitioning = true;
    hasNewFrame = true;
    update();
}
void GLWidget::clearFrame() {
    int w = lastFrameWidth > 0 ? lastFrameWidth : 1280;
    int h = lastFrameHeight > 0 ? lastFrameHeight : 720;
    currentFrame.rgbData.assign(w * h * 3, 0);
    currentFrame.width = w;
    currentFrame.height = h;
    hasNewFrame = true;
    update();
}

void GLWidget::setPlaybackTime(double time) {
    m_time = time;
}

void GLWidget::setActiveEffects(const std::vector<AppliedEffect>& effects) {
    activeEffects = effects;
    update();
}

void GLWidget::setShowOverlay(bool show) {
    showOverlay = show;
    if (overlayLabel) overlayLabel->setVisible(show);
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
        plugin.isCompiled = true;
        program.release()->setParent(this);
    } else {
        qWarning() << "Failed to link developer plugin shader:" << QString::fromStdString(plugin.name)
                   << program->log();
    }
}

void GLWidget::paintGL() {
    if (!passthroughShader || !passthroughShader->isLinked()) return;

    int w = width();
    int h = height();
    allocateFBOs(w, h);

    if (hasNewFrame) {
        std::lock_guard<std::mutex> lock(m_frameMutex);
        if (!isTransitioning) {
            if (!currentFrame.rgbData.empty()) {
                glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, videoTexture);

                if (lastFrameWidth != currentFrame.width || lastFrameHeight != currentFrame.height) {
                    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, currentFrame.width, currentFrame.height, 0, GL_RGB, GL_UNSIGNED_BYTE, currentFrame.rgbData.data());
                    lastFrameWidth = currentFrame.width;
                    lastFrameHeight = currentFrame.height;
                } else {
                    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, currentFrame.width, currentFrame.height, GL_RGB, GL_UNSIGNED_BYTE, currentFrame.rgbData.data());
                }
            }
        } else {
            if (!transitionFrame1.rgbData.empty()) {
                glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, videoTexture);
                if (lastFrameWidth != transitionFrame1.width || lastFrameHeight != transitionFrame1.height) {
                    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, transitionFrame1.width, transitionFrame1.height, 0, GL_RGB, GL_UNSIGNED_BYTE, transitionFrame1.rgbData.data());
                    lastFrameWidth = transitionFrame1.width;
                    lastFrameHeight = transitionFrame1.height;
                } else {
                    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, transitionFrame1.width, transitionFrame1.height, GL_RGB, GL_UNSIGNED_BYTE, transitionFrame1.rgbData.data());
                }
            }
            if (!transitionFrame2.rgbData.empty()) {
                glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
                glActiveTexture(GL_TEXTURE1);
                glBindTexture(GL_TEXTURE_2D, videoTexture2);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, transitionFrame2.width, transitionFrame2.height, 0, GL_RGB, GL_UNSIGNED_BYTE, transitionFrame2.rgbData.data());
            }
        }
        hasNewFrame = false;
    }

    auto bindRichUniforms = [&](GLuint prog) {
        GLint timeLoc = glGetUniformLocation(prog, "time");
        if (timeLoc != -1) glUniform1f(timeLoc, (float)m_time);

        GLint resLoc = glGetUniformLocation(prog, "resolution");
        if (resLoc != -1) glUniform2f(resLoc, (float)w, (float)h);

        GLint aspectLoc = glGetUniformLocation(prog, "aspect");
        if (aspectLoc != -1) glUniform1f(aspectLoc, (float)w / std::max(1.0f, (float)h));

        GLint frameLoc = glGetUniformLocation(prog, "frameIndex");
        if (frameLoc != -1) glUniform1i(frameLoc, (int)(m_time * 30.0));

        GLint fpsLoc = glGetUniformLocation(prog, "fps");
        if (fpsLoc != -1) glUniform1f(fpsLoc, 30.0f);

        QPoint pt = mapFromGlobal(QCursor::pos());
        float normMouseX = (float)pt.x() / std::max(1.0f, (float)w);
        float normMouseY = (float)(h - pt.y()) / std::max(1.0f, (float)h);
        bool isMouseDown = (QApplication::mouseButtons() & Qt::LeftButton);

        GLint mouseLoc = glGetUniformLocation(prog, "mouse");
        if (mouseLoc != -1) glUniform2f(mouseLoc, normMouseX, normMouseY);

        GLint mouseXLoc = glGetUniformLocation(prog, "mouseX");
        if (mouseXLoc != -1) glUniform1f(mouseXLoc, normMouseX);

        GLint mouseYLoc = glGetUniformLocation(prog, "mouseY");
        if (mouseYLoc != -1) glUniform1f(mouseYLoc, normMouseY);

        GLint mousePressLoc = glGetUniformLocation(prog, "mousePressed");
        if (mousePressLoc != -1) glUniform1i(mousePressLoc, isMouseDown ? 1 : 0);

        float b = AudioEngine::instance().getBass();
        float m = AudioEngine::instance().getMid();
        float t = AudioEngine::instance().getHigh();

        GLint bassLoc = glGetUniformLocation(prog, "audioBass");
        if (bassLoc != -1) glUniform1f(bassLoc, b);
        GLint midLoc = glGetUniformLocation(prog, "audioMid");
        if (midLoc != -1) glUniform1f(midLoc, m);
        GLint trebleLoc = glGetUniformLocation(prog, "audioTreble");
        if (trebleLoc != -1) glUniform1f(trebleLoc, t);
        GLint volLoc = glGetUniformLocation(prog, "audioVolume");
        if (volLoc != -1) glUniform1f(volLoc, (b + m + t) / 3.0f);
    };

    GLuint currentTex = videoTexture;
    if (isTransitioning) {
        ShaderPlugin* plugin = PluginManager::instance().findPlugin(currentTransitionPlugin);
        if (plugin) {
            compileCustomPluginShader(*plugin);
            if (plugin->isCompiled && plugin->shaderProgram > 0) {
                fboPong->bind();
                glViewport(0, 0, w, h);
                glClear(GL_COLOR_BUFFER_BIT);

                glUseProgram(plugin->shaderProgram);

                GLint videoTexLoc = glGetUniformLocation(plugin->shaderProgram, "videoTexture");
                glUniform1i(videoTexLoc, 0);
                GLint videoTex2Loc = glGetUniformLocation(plugin->shaderProgram, "videoTexture2");
                glUniform1i(videoTex2Loc, 1);
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, videoTexture);
                glActiveTexture(GL_TEXTURE1);
                glBindTexture(GL_TEXTURE_2D, videoTexture2);

                GLint progressLoc = glGetUniformLocation(plugin->shaderProgram, "progress");
                if (progressLoc != -1) glUniform1f(progressLoc, (float)transitionProgress);

                bindRichUniforms(plugin->shaderProgram);

                renderQuad();

                glUseProgram(0);
                fboPong->release();

                currentTex = fboPong->texture();
                std::swap(fboPing, fboPong);
            }
        }
    }

    for (const AppliedEffect& eff : activeEffects) {
        ShaderPlugin* plugin = PluginManager::instance().findPlugin(eff.pluginId);
        if (plugin) {
            compileCustomPluginShader(*plugin);
            if (plugin->isCompiled && plugin->shaderProgram > 0) {
                fboPong->bind();
                glViewport(0, 0, w, h);
                glClear(GL_COLOR_BUFFER_BIT);

                glUseProgram(plugin->shaderProgram);

                bindRichUniforms(plugin->shaderProgram);

                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, currentTex);
                GLint videoTexLoc = glGetUniformLocation(plugin->shaderProgram, "videoTexture");
                if (videoTexLoc != -1) glUniform1i(videoTexLoc, 0);
                GLint currentTexLoc = glGetUniformLocation(plugin->shaderProgram, "currentTexture");
                if (currentTexLoc != -1) glUniform1i(currentTexLoc, 0);

                glActiveTexture(GL_TEXTURE1);
                glBindTexture(GL_TEXTURE_2D, fboFeedback->texture());
                GLint feedbackTexLoc = glGetUniformLocation(plugin->shaderProgram, "feedbackTexture");
                if (feedbackTexLoc != -1) glUniform1i(feedbackTexLoc, 1);
                GLint blendTexLoc = glGetUniformLocation(plugin->shaderProgram, "blendTexture");
                if (blendTexLoc != -1) glUniform1i(blendTexLoc, 1);

                for (const auto& param : eff.parameters) {
                    GLint paramLoc = glGetUniformLocation(plugin->shaderProgram, param.name.c_str());
                    if (paramLoc != -1) {
                        double paramVal = param.curve.getKeyframes().empty() ? param.currentVal : param.curve.evaluate(m_time);
                        glUniform1f(paramLoc, (float)paramVal);
                    }
                }

                renderQuad();

                glUseProgram(0);
                fboPong->release();

                currentTex = fboPong->texture();
                std::swap(fboPing, fboPong);
            }
        }
    }

    if (fboFeedback && passthroughShader && passthroughShader->isLinked()) {
        fboFeedback->bind();
        glViewport(0, 0, w, h);
        passthroughShader->bind();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, currentTex);
        passthroughShader->setUniformValue("videoTexture", 0);
        renderQuad();
        passthroughShader->release();
        fboFeedback->release();
    }

    glBindFramebuffer(GL_FRAMEBUFFER, defaultFramebufferObject());
    glViewport(0, 0, w, h);
    glClear(GL_COLOR_BUFFER_BIT);

    passthroughShader->bind();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, currentTex);
    passthroughShader->setUniformValue("videoTexture", 0);

    renderQuad();

    passthroughShader->release();
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
