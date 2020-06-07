#pragma once

#include "Image.h"

BRWL_RENDERER_NS


// we expect the image to be null initialized
void makePreintegrationTable(Image& image, float* transferFunc, unsigned int lenFunc);

// we expect the image to be null initialized
void makeDiagram(Image& image, float* transferFunc, unsigned int lenFunc);
// we expect the image to be null initialized
void makeTestImage(Image& image, float* transferFunc, unsigned int lenFunc);


BRWL_RENDERER_NS_END