#pragma once

// Deliberately small seam for the renderer rewrite. New backends (D3D11/Metal/
// Vulkan) can implement this choice without changing the editor's timeline or
// effect APIs. For now both modes use the mature OpenGL effect renderer.
enum class RenderBackendKind {
    OpenGLCompatibility,
    OpenGLPipelined,
};

inline const char* renderBackendName(RenderBackendKind backend) {
    switch (backend) {
        case RenderBackendKind::OpenGLPipelined: return "Pipelined OpenGL";
        case RenderBackendKind::OpenGLCompatibility:
        default: return "Compatibility OpenGL";
    }
}

inline bool renderBackendUsesPbo(RenderBackendKind backend) {
    return backend == RenderBackendKind::OpenGLPipelined;
}
