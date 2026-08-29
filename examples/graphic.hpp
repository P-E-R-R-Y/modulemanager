#include "IModule.hpp"

struct IGraphicModule : IModule {
    static constexpr const char *entry = "getGraphicModule";
    const char *type() const override { return "graphic"; }
};

struct IWindowModule : IModule {
    static constexpr const char *entry = "getWindowModule";
    const char *type() const override { return "window"; }
};
