#pragma once
#include "imgui.h"

class ExampleFont;

namespace ExampleText
{
    void            Install();
    ExampleFont*    LoadFont(const char* path);
    void            DestroyFont(ExampleFont* font);
    ImFont*         ToImFont(ExampleFont* font);
}
