#pragma once

#include <Arduino.h>

enum class Face {
    Sleep,
    Happy,
    Think,
    Disgust,
    Amaze,
    Fist,
};

const char* faceName(Face f);
Face faceFromName(const char* name);
void faceBegin();
void faceSet(Face f);
Face faceCurrent();
void faceSetCaption(const char* text);
void faceTick();
