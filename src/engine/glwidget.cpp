#include "glwidget.h"
#include <QOpenGLFramebufferObjectFormat>
#include <cmath>
#include <QDebug>
#include <QFile>
#include <QIODevice>
#include "engine/audioengine.h"

static const char* vertexShaderSource = R"(#version 330 core
layout (location = 0) in vec2 position;
out vec2 TexCoord;
void main() {
    TexCoord = position * 0.5 + 0.5;
    gl_Position = vec4(position, 0.0, 1.0);
}
)";

static const char* bentShaderSource = R"(#version 330 core
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform float time;
uniform float syncDrift;
uniform float clockCorruption;
uniform float railInstability;
uniform float addressScramble;

float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123);
}

void main() {
    vec2 uv = TexCoord;
    // Sync Drift (horizontal shifting / tearing of rows)
    if (syncDrift > 0.0) {
        float drift = sin(uv.y * 10.0 + time * 5.0) * 0.02 * syncDrift;
        // Add random sync breaks
        if (hash(vec2(floor(uv.y * 30.0), time)) > 0.98 - (syncDrift * 0.05)) {
            drift += hash(vec2(time)) * 0.1 * syncDrift;
        }
        uv.x = fract(uv.x + drift);
    }
    // Clock Corruption (repeated columns, stretched pixels)
    if (clockCorruption > 0.0) {
        float colSteps = 128.0 - (clockCorruption * 100.0);
        float stepX = floor(uv.x * colSteps) / colSteps;
        if (hash(vec2(stepX, time)) > 0.95 - (clockCorruption * 0.1)) {
            uv.x = stepX; // lock pixel clock column
        }
    }

    // Address Scrambling (corrupted RAM addressing)
    if (addressScramble > 0.0) {
        float grid = 8.0;
        vec2 cell = floor(uv * grid) / grid;
        if (hash(cell + time) < addressScramble * 0.2) {
            // swap texture blocks
            uv = fract(uv + hash(cell) * 0.5);
        }
    }

    vec4 color = texture(videoTexture, uv);

    // Power Rail Instability (brightness oscillations and resets)
    if (railInstability > 0.0) {
        float noiseValue = sin(time * 20.0) * cos(time * 35.0);
        color.rgb *= 1.0 + noiseValue * 0.4 * railInstability;
        // color washouts
        if (noiseValue > 0.8) {
            color.rgb += vec3(0.3 * railInstability);
        }
    }

    FragColor = color;
}
)";

static const char* vhsShaderSource = R"(#version 330 core
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform float time;
uniform float tracking;
uniform float noise;
uniform float wear;
uniform float chromaDelay;

float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123);
}

void main() {
    vec2 uv = TexCoord;

    // Head Switching Noise (tearing at the bottom 2% of the screen)
    if (uv.y < 0.02) {
        float switchOffset = hash(vec2(uv.y, time)) * 0.05;
        uv.x = fract(uv.x + switchOffset);
    }

    // Tracking Jitter/Lines
    float trackLine = fract(time * 0.2);
    float distToTrack = abs(uv.y - trackLine);
    if (distToTrack < 0.05 * tracking) {
        uv.x += sin(uv.y * 50.0 + time * 10.0) * 0.015 * tracking;
    }

    // Chroma Delay (VHS color bleeding: shift red and blue channels)
    float r = texture(videoTexture, uv - vec2(chromaDelay, 0.0)).r;
    float g = texture(videoTexture, uv).g;
    float b = texture(videoTexture, uv + vec2(chromaDelay, 0.0)).b;

    vec4 color = vec4(r, g, b, 1.0);

    // Add Tape Noise / Generation Loss Wear
    if (noise > 0.0 || wear > 0.0) {
        float n = hash(uv + time);
        color.rgb += vec3(n * 0.15 * noise);
        // Horizontal scan dropouts
        if (hash(vec2(uv.y * 200.0, time)) > 0.99 - (wear * 0.03)) {
            color.rgb = vec3(0.8); // White dropout line
        }
    }

    FragColor = color;
}
)";

static const char* crtShaderSource = R"(#version 330 core
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform float curvature;
uniform float bloom;
uniform float persistence;
uniform float rgbOffset;

void main() {
    vec2 uv = TexCoord;

    // 1. CRT Screen Curvature (barrel distortion)
    if (curvature > 0.0) {
        uv = uv - 0.5;
        float dist = dot(uv, uv);
        uv = uv * (1.0 + curvature * dist + curvature * 0.5 * dist * dist);
        uv = uv + 0.5;
    }

    // Border cut off for curved tube edges
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) {
        FragColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    // 2. Phosphor Mask (Convergence Offset)
    float r = texture(videoTexture, uv - vec2(rgbOffset, 0.0)).r;
    float g = texture(videoTexture, uv).g;
    float b = texture(videoTexture, uv + vec2(rgbOffset, 0.0)).b;
    vec3 color = vec3(r, g, b);

    // 3. Scanline grid overlay
    float scanline = sin(uv.y * 800.0) * 0.15;
    color -= vec3(scanline);

    // 4. Subtle RGB slot mask pattern
    float mask = sin(uv.x * 1200.0) * 0.08;
    color += vec3(mask);

    // 5. Bloom/Glow emulation
    if (bloom > 0.0) {
        vec3 blurColor = texture(videoTexture, uv + vec2(0.005, 0.005)).rgb;
        blurColor += texture(videoTexture, uv - vec2(0.005, 0.005)).rgb;
        color += blurColor * 0.12 * bloom;
    }

    FragColor = vec4(color, 1.0);
}
)";

static const char* feedbackShaderSource = R"(#version 330 core
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D currentTexture;
uniform sampler2D feedbackTexture;
uniform float gain;
uniform float zoom;
uniform float rotation;
uniform vec2 shift;

void main() {
    vec2 uv = TexCoord;
    // Read clean, current frame
    vec4 current = texture(currentTexture, uv);

    // Apply zoom, rotation, offset mapping to feedback texture coordinates
    vec2 f_uv = uv - 0.5;
    // Zoom
    f_uv /= zoom;
    // Rotate
    float cosR = cos(rotation);
    float sinR = sin(rotation);
    f_uv = vec2(f_uv.x * cosR - f_uv.y * sinR, f_uv.x * sinR + f_uv.y * cosR);
    f_uv = f_uv + 0.5;
    // Shift/Offset
    f_uv -= shift;

    // Read historical feedback buffer
    vec4 feedback = texture(feedbackTexture, f_uv);

    // Blend current with historical feedback
    FragColor = mix(current, feedback, gain);
}
)";

static const char* passthroughShaderSource = R"(#version 330 core
in vec2 TexCoord;
out vec4 FragColor;
uniform sampler2D videoTexture;
void main() {
    FragColor = texture(videoTexture, TexCoord);
}
)";

static const char* milkdropShaderSource = R"(#version 330 core
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform float time;
uniform float bass;
uniform float mid;
uniform float treble;
uniform float zoom;
uniform float rotation;

void main() {
    vec2 uv = TexCoord - 0.5;
    // Apply audio-reactive rotation
    float angle = rotation * bass * 3.14159;
    float s = sin(angle);
    float c = cos(angle);
    mat2 rot = mat2(c, -s, s, c);
    uv = rot * uv;
    // Apply audio-reactive zoom and non-linear fractal warp
    float z = zoom - (treble * 0.1);
    float radius = length(uv);
    uv *= z - (sin(radius * 10.0 - time * 5.0) * mid * 0.05);
    uv += 0.5;
    // Chromatic aberration driven by bass
    float r = texture(videoTexture, uv + (uv - 0.5) * bass * 0.05).r;
    float g = texture(videoTexture, uv).g;
    float b = texture(videoTexture, uv - (uv - 0.5) * bass * 0.05).b;
    vec4 color = vec4(r, g, b, 1.0);
    // Brighten based on mid/treble hits
    color.rgb *= 1.0 + (mid * 0.2) + (treble * 0.2);
    FragColor = color;
}
)";

static const char* xorShaderSource = R"(#version 330 core
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D videoTexture;
uniform sampler2D blendTexture;
uniform float intensity;

void main() {
    vec4 c1 = texture(videoTexture, TexCoord);
    vec4 c2 = texture(blendTexture, TexCoord);
    // Scale to 255 integers to perform bitwise XOR
    uvec3 u1 = uvec3(c1.rgb * 255.0);
    uvec3 u2 = uvec3(c2.rgb * 255.0);
    uvec3 xorVal = u1 ^ u2;
    vec3 result = vec3(xorVal) / 255.0;
    FragColor = vec4(mix(c1.rgb, result, intensity), 1.0);
}
)";

GLWidget::GLWidget(QWidget* parent) : QOpenGLWidget(parent) {
    QSurfaceFormat format;
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::NoProfile);
    setFormat(format);
    fpsTimer.start();
    overlayLabel = new QLabel(this);
    overlayLabel->setStyleSheet(
        "QLabel {"
        "  color: #00ff00;"
        "  background-color: rgba(0, 0, 0, 160);"
        "  font-family: Consolas, monospace;"
        "  font-size: 13px;"
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
    delete passthroughShader;
    delete fboPing;
    delete fboPong;
    delete fboFeedback;
    doneCurrent();
}

void GLWidget::initializeGL() {
    initializeOpenGLFunctions();
    glClearColor(0.08f, 0.08f, 0.08f, 1.0f);

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
    if (plugin.isCompiled) return;

    QOpenGLShaderProgram* program = new QOpenGLShaderProgram(this);
    program->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShaderSource);
    QFile file(QString::fromStdString(plugin.fragmentShaderPath));
    if (file.open(QIODevice::ReadOnly)) {
        QString fragSource = file.readAll();
        program->addShaderFromSourceCode(QOpenGLShader::Fragment, fragSource);
    }
    if (program->link()) {
        plugin.shaderProgram = program->programId();
        plugin.isCompiled = true;
    } else {
        qWarning() << "Failed to compile developer plugin shader:" << QString::fromStdString(plugin.name)
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
                GLint resLoc = glGetUniformLocation(plugin->shaderProgram, "resolution");
                if (resLoc != -1) glUniform2f(resLoc, (float)w, (float)h);

                renderQuad();

                glUseProgram(0);
                fboPong->release();

                currentTex = fboPong->texture();
                std::swap(fboPing, fboPong);
            }
        }
    }

    auto pingPongPass = [&](QOpenGLShaderProgram* shader, auto setupUniforms) {
        fboPong->bind();
        glViewport(0, 0, w, h);
        glClear(GL_COLOR_BUFFER_BIT);

        shader->bind();
            glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, currentTex);
        shader->setUniformValue("videoTexture", 0);
        shader->setUniformValue("time", (float)m_time);
        shader->setUniformValue("resolution", QVector2D(w, h));

        setupUniforms(shader);

        renderQuad();

        shader->release();
        fboPong->release();

        currentTex = fboPong->texture();
        std::swap(fboPing, fboPong);
    };

    for (const AppliedEffect& eff : activeEffects) {
        ShaderPlugin* plugin = PluginManager::instance().findPlugin(eff.pluginId);
        if (plugin) {
            compileCustomPluginShader(*plugin);
            if (plugin->isCompiled && plugin->shaderProgram > 0) {
                fboPong->bind();
                glViewport(0, 0, w, h);
                glClear(GL_COLOR_BUFFER_BIT);

                glUseProgram(plugin->shaderProgram);

                GLint videoTexLoc = glGetUniformLocation(plugin->shaderProgram, "videoTexture");
                glUniform1i(videoTexLoc, 0);

                GLint timeLoc = glGetUniformLocation(plugin->shaderProgram, "time");
                glUniform1f(timeLoc, (float)m_time);

                GLint resLoc = glGetUniformLocation(plugin->shaderProgram, "resolution");
                glUniform2f(resLoc, (float)w, (float)h);
                GLint mouseLoc = glGetUniformLocation(plugin->shaderProgram, "mouse");
                if (mouseLoc != -1) {
                    QPoint pt = mapFromGlobal(QCursor::pos());
                    glUniform2f(mouseLoc, (float)pt.x() / w, (float)(h - pt.y()) / h);
                }
                GLint bassLoc = glGetUniformLocation(plugin->shaderProgram, "audioBass");
                if (bassLoc != -1) glUniform1f(bassLoc, AudioEngine::instance().getBass());
                GLint midLoc = glGetUniformLocation(plugin->shaderProgram, "audioMid");
                if (midLoc != -1) glUniform1f(midLoc, AudioEngine::instance().getMid());
                GLint trebleLoc = glGetUniformLocation(plugin->shaderProgram, "audioTreble");
                if (trebleLoc != -1) glUniform1f(trebleLoc, AudioEngine::instance().getHigh());

                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, currentTex);

                GLint currentTexLoc = glGetUniformLocation(plugin->shaderProgram, "currentTexture");
                if (currentTexLoc != -1) {
                    glUniform1i(currentTexLoc, 0);
                }

                glActiveTexture(GL_TEXTURE1);
                glBindTexture(GL_TEXTURE_2D, fboFeedback->texture());
                GLint feedbackTexLoc = glGetUniformLocation(plugin->shaderProgram, "feedbackTexture");
                if (feedbackTexLoc != -1) {
                    glUniform1i(feedbackTexLoc, 1);
                }

                GLint blendTexLoc = glGetUniformLocation(plugin->shaderProgram, "blendTexture");
                if (blendTexLoc != -1) {
                    glUniform1i(blendTexLoc, 1);
                }

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
