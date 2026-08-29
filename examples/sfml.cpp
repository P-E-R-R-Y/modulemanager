/* SFML : fournit graphic SEULEMENT, pas window. Couverture partielle
 * volontaire. */
#include "graphic.hpp"

namespace {
struct Graphic final : IGraphicModule { const char *name() const override { return "sfml"; } };
Graphic graphic;
} // namespace

extern "C" IGraphicModule *getGraphicModule() { return &graphic; }
