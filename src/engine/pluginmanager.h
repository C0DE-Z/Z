#ifndef PLUGINMANAGER_H
#define PLUGINMANAGER_H

#include <string>
#include <vector>
#include <map>
#include "core/keyframe.h"

struct ShaderParameter {
    std::string name;
    std::string label;
    double minVal;
    double maxVal;
    double defaultVal;
    double currentVal;
    bool isBool = false;
    AnimationCurve curve; 
};

struct ShaderPlugin {
    std::string id; 
    std::string name;
    std::string description;
    std::string fragmentShaderPath;
    std::vector<ShaderParameter> parameters;
    bool isCompiled = false;
    unsigned int shaderProgram = 0;
};

class PluginManager {
public:
    static PluginManager& instance() {
        static PluginManager inst;
        return inst;
    }

    void scanPluginsDir(const std::string& path);
    const std::vector<ShaderPlugin>& getPlugins() const { return plugins; }
    std::vector<ShaderPlugin>& getPlugins() { return plugins; }

    ShaderPlugin* findPlugin(const std::string& id);

    void createDefaultPlugins(const std::string& path);

private:
    PluginManager() = default;
    std::vector<ShaderPlugin> plugins;
    bool loadPluginFromDir(const std::string& dirPath, const std::string& filename);
};

#endif 
