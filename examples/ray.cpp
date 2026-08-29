/* raylib : fournit DEUX contrats depuis la meme dll. */
#include "graphic.hpp"

namespace {
struct Graphic final : IGraphicModule { const char *name() const override { return "ray"; } };
struct Window final : IWindowModule { const char *name() const override { return "ray"; } };
Graphic graphic;
Window window;
} // namespace

extern "C" IGraphicModule *getGraphicModule() { return &graphic; }
extern "C" IWindowModule *getWindowModule() { return &window; }
