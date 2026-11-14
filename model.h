#pragma once

#include "maths.h"
#include <string>

Mesh ObjLOAD(const std::string& file_name, uint32_t color = 0xFFFFFFFF);

Texture LoadTexture(const char* filename);

void NormaliserModele(Mesh& mesh);

