// Aseprite Render Library
// Copyright (c) 2019 Igara Studio S.A
// Copyright (c) 2017 David Capello
//
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "render/error_diffusion.h"

#include "gfx/hsl.h"
#include "gfx/rgb.h"

#include <algorithm>

namespace render {

ErrorDiffusionDither::ErrorDiffusionDither(int transparentIndex)
  : m_transparentIndex(transparentIndex)
{
}

void ErrorDiffusionDither::start(
  const doc::Image* srcImage,
  doc::Image* dstImage)
{
  m_srcImage = srcImage;
  m_width = 2+srcImage->width();
  for (int i=0; i<kChannels; ++i)
    m_err[i].resize(m_width*2, 0);
  m_lastY = -1;
}

void ErrorDiffusionDither::finish()
{
}

doc::color_t ErrorDiffusionDither::ditherRgbToIndex2D(
  const int x, const int y,
  const doc::RgbMap* rgbmap,
  const doc::Palette* palette)
{
  if (y != m_lastY) {
    for (int i=0; i<kChannels; ++i) {
      int* row0 = &m_err[i][0];
      int* row1 = row0 + m_width;
      int* end1 = row1 + m_width;
      std::copy(row1, end1, row0);
      std::fill(row1, end1, 0);
    }
    m_lastY = y;
  }

  doc::color_t color =
    doc::get_pixel_fast<doc::RgbTraits>(m_srcImage, x, y);

  // Get RGB values + quatization error
  int v[kChannels] = {
    doc::rgba_getr(color),
    doc::rgba_getg(color),
    doc::rgba_getb(color),
    doc::rgba_geta(color)
  };
  int u[kChannels];
  for (int i=0; i<kChannels; ++i) {
    v[i] += m_err[i][x+1] / 16;
    u[i] = MID(0, v[i], 255);
  }

  const doc::color_t index =
    (rgbmap ? rgbmap->mapColor(u[0], u[1], u[2], u[3]):
              palette->findBestfit(u[0], u[1], u[2], u[3], m_transparentIndex));

  doc::color_t palColor = palette->getEntry(index);
  if (m_transparentIndex == index || doc::rgba_geta(palColor) == 0) {
    // "color" without alpha
    palColor = (color & doc::rgba_rgb_mask);
  }

  const int quantError[kChannels] = {
    v[0] - doc::rgba_getr(palColor),
    v[1] - doc::rgba_getg(palColor),
    v[2] - doc::rgba_getb(palColor),
    v[3] - doc::rgba_geta(palColor)
  };

  // TODO using Floyd-Steinberg matrix here but it should be configurable
  for (int i=0; i<kChannels; ++i) {
    int* err = &m_err[i][x];
    err[       +2] += quantError[i] * 7;
    err[m_width  ] += quantError[i] * 3;
    err[m_width+1] += quantError[i] * 5;
    err[m_width+2] += quantError[i] * 1;
  }

  return index;
}

} // namespace render