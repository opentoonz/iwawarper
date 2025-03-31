
//-----------------------------------------------------------------------------------------
// Based on tantialias.cpp in OpenToonz
//-----------------------------------------------------------------------------------------

#include "iwrenderinstance.h"
#include "iwproject.h"
#include "trastercm.h"
#include "half.h"

/*
See Alexander Reshetov's "Morphological Antialiasing" paper on Intel Labs site.

Basically, this antialiasing algorithm is based on the following ideas:

 - Suppose that our image is just made up of flat colors. Then, a simple
antialiasing
   approach is that of assuming that the 'actual' line separating two distinct
colors
   is the polyline that passes through the midpoint of each edge of its original
jaggy
   counterpart.
   As pixels around the border are cut through by the polyline, the area of the
pixel
   that is filled of a certain color is its weight in the output filtered pixel.

 - The above method can be applied on each single uniform piece of a scanline,
considering
   the lines originated by the vertical extensions of its left and right edges.

 - Of these lines, only those which lie completely on pixels adjacent to the
edge are
   considered - so that the antialiasing effect is kept only around the
contours.

This algorithm would yield a good result at what may be considered 50% softness.
Implementing
a generalized softness simply requires that the line slopes used above are
modified
accordingly (divide by 2 * softFactor).
*/

//-----------------------------------------------------------------------------------------

namespace {

template <typename PIX>
inline bool areEqual(const PIX &a, const PIX &b) {
  return a == b;
}

//-----------------------------------------------------------------------------------------

template <typename PIX>
inline void weightPix(PIX *out, const PIX *a, const PIX *b, double weightA,
                      double weightB) {
  out->r = a->r * weightA + b->r * weightB;
  out->g = a->g * weightA + b->g * weightB;
  out->b = a->b * weightA + b->b * weightB;
  out->m = a->m * weightA + b->m * weightB;
}

//-----------------------------------------------------------------------------------------

// Returns 0 if pixels to connect are on the 00-11 diagonal, 1 on the 01-10 one.
template <typename PIX>
inline bool checkNeighbourHood(int x, int y, PIX *pix, int lx, int ly, int dx,
                               int dy) {
  int count1 = 0, count2 = 0;
  int dx2 = 2 * dx, dy2 = 2 * dy;
  if (y > 1) {
    // Lower edge
    count1 += (int)areEqual(*(pix - dx), *(pix - dy2)) +
              (int)areEqual(*(pix - dx), *(pix - dy2 - dx));
    count2 += (int)areEqual(*pix, *(pix - dy2)) +
              (int)areEqual(*pix, *(pix - dy2 - dx));
  }
  if (y < ly - 1) {
    // Upper edge
    count1 += (int)areEqual(*(pix - dx), *(pix + dy)) +
              (int)areEqual(*(pix - dx), *(pix + dy - dx));
    count2 += (int)areEqual(*pix, *(pix + dy)) +
              (int)areEqual(*pix, *(pix + dy - dx));
  }
  if (x > 1) {
    // Left edge
    count1 += (int)areEqual(*(pix - dx), *(pix - dx2)) +
              (int)areEqual(*(pix - dx), *(pix - dx2 - dy));
    count2 += (int)areEqual(*pix, *(pix - dx2)) +
              (int)areEqual(*pix, *(pix - dx2 - dy));
  }
  if (x < lx - 1) {
    // Left edge
    count1 += (int)areEqual(*(pix - dx), *(pix + dx)) +
              (int)areEqual(*(pix - dx), *(pix + dx - dy));
    count2 += (int)areEqual(*pix, *(pix + dx)) +
              (int)areEqual(*pix, *(pix + dx - dy));
  }

  // Connect by minority: if there are more pixels like those on the 00-11
  // diagonal, connect the other,
  // and vice versa.
  return count1 > count2;
}
}  // namespace

//========================================================================================

// RGBA各チャンネルにそれぞれ下記の値をhalf-floatで入れる
// R:水平メタ境界線の左の切片 G:水平メタ境界線の右の切片
// B:垂直メタ境界線の下の切片 A:垂直メタ境界線の上の切片
template <typename PIX>
inline void filterLine(PIX *inLPix, PIX *inUPix, TPixel64 *outLPix,
                       TPixel64 *outUPix, int ll, int inDl, int outLDl,
                       int outUDl, double hStart, double slope,
                       bool filterLower, bool isHorizontal, bool isInverted) {
  assert(hStart >= 0.0 && slope > 0.0);

  double h0   = hStart, h1, area;
  double base = hStart / slope;

  int i, end = std::min(tceil(base), ll);
  // int i, end = std::min(tfloor(base), ll);
  if (filterLower) {
    // Filter lower line
    for (i = 0; i < end;
         ++i, h0 = h1, inLPix += inDl, inUPix += inDl, outLPix += outLDl) {
      h1 = h0 - slope;

      if (isHorizontal) {
        if (isInverted) {
          outLPix->r = FLOAT16::ToFloat16(1.f - h1).m_uiFormat;
          outLPix->g = FLOAT16::ToFloat16(1.f - h0).m_uiFormat;
        } else {
          outLPix->r = FLOAT16::ToFloat16(1.f - h0).m_uiFormat;
          outLPix->g = FLOAT16::ToFloat16(1.f - h1).m_uiFormat;
        }
      } else {  // vertical case
        if (isInverted) {
          outLPix->b = FLOAT16::ToFloat16(1.f - h1).m_uiFormat;
          outLPix->m = FLOAT16::ToFloat16(1.f - h0).m_uiFormat;
        } else {
          outLPix->b = FLOAT16::ToFloat16(1.f - h0).m_uiFormat;
          outLPix->m = FLOAT16::ToFloat16(1.f - h1).m_uiFormat;
        }
      }
    }
  } else {
    // Filter upper line
    for (i = 0; i < end;
         ++i, h0 = h1, inLPix += inDl, inUPix += inDl, outUPix += outUDl) {
      h1 = h0 - slope;

      if (isHorizontal) {
        if (isInverted) {
          outUPix->r = FLOAT16::ToFloat16(h1).m_uiFormat;
          outUPix->g = FLOAT16::ToFloat16(h0).m_uiFormat;
        } else {
          outUPix->r = FLOAT16::ToFloat16(h0).m_uiFormat;
          outUPix->g = FLOAT16::ToFloat16(h1).m_uiFormat;
        }
      } else {  // vertical case
        if (isInverted) {
          outUPix->b = FLOAT16::ToFloat16(h1).m_uiFormat;
          outUPix->m = FLOAT16::ToFloat16(h0).m_uiFormat;
        } else {
          outUPix->b = FLOAT16::ToFloat16(h0).m_uiFormat;
          outUPix->m = FLOAT16::ToFloat16(h1).m_uiFormat;
        }
      }
    }
  }
}

//---------------------------------------------------------------------------------------

template <typename PIX>
inline bool checkLength(int lLine, int y, int ly, int dy, PIX *pixL1,
                        PIX *pixU1, PIX *pixL2, PIX *pixU2, bool uniteU,
                        bool do1Line) {
  // 1-length edges must be processed (as primary edges) only if explicitly
  // required,
  // and only when its associated secondary edge is of the same length.

  return (lLine > 1) ||
         (do1Line &&
          ((uniteU && (y > 1 && !(areEqual(*pixL1, *(pixL1 - dy)) &&
                                  areEqual(*pixL2, *(pixL2 - dy))))) ||
           (!uniteU && (y < ly - 1 && !(areEqual(*pixU1, *(pixU1 + dy)) &&
                                        areEqual(*pixU2, *(pixU2 + dy)))))));
}

//---------------------------------------------------------------------------------------

template <typename PIX>
void processLine(int r, int lx, int ly, PIX *inLRow, PIX *inURow,
                 TPixel64 *outLRow, TPixel64 *outURow, int inDx, int inDy,
                 int outLDx, int outUDx, bool do1Line, double hStart,
                 double slope) {
  // Using a 'horizontal' notation here - but the same applies in vertical too
  ++r;

  bool isHorizontal = do1Line;

  // As long as we don't reach row end, process uninterrupted separation lines
  // between colors

  PIX *inLL = inLRow, *inLR, *inUL = inURow, *inUR;
  PIX *inLL_1, *inUL_1, *inLR_1, *inUR_1;
  PIX *inLEnd = inLRow + lx * inDx;
  int x, lLine;
  bool uniteLL, uniteUL, uniteLR, uniteUR;

  // Special case: a line at row start has different weights
  if (!areEqual(*inLL, *inUL)) {
    // Look for line ends
    for (inLR = inLL + inDx, inUR = inUL + inDx;
         inLR != inLEnd && areEqual(*inLL, *inLR) && areEqual(*inUL, *inUR);
         inLR += inDx, inUR += inDx);

    if (inLR != inLEnd) {
      // Found a line to process
      lLine = (inLR - inLL) / inDx;

      inLR_1 = inLR - inDx, inUR_1 = inUR - inDx;
      x = (inLR_1 - inLRow) / inDx;

      uniteUR = areEqual(*inUR_1, *inLR);
      uniteLR = areEqual(*inLR_1, *inUR);
      if (uniteUR || uniteLR) {
        if (uniteUR && uniteLR)
          // Ambiguous case. Check neighborhood to find out which one must be
          // actually united.
          uniteUR = !checkNeighbourHood(x + 1, r, inUR, lx, ly, inDx, inDy);

        if (checkLength(lLine, r, ly, inDy, inLR_1, inUR_1, inLR, inUR, uniteUR,
                        do1Line))
          filterLine(inLR_1, inUR_1, outLRow + x * outLDx, outURow + x * outUDx,
                     lLine, -inDx, -outLDx, -outUDx, hStart,
                     slope / (lLine << 1), uniteUR, isHorizontal, true);
      }
    }

    // Update lefts
    inLL = inLR, inUL = inUR;
  }

  // Search for a line start
  for (; inLL != inLEnd && areEqual(*inLL, *inUL); inLL += inDx, inUL += inDx);

  while (inLL != inLEnd) {
    // Look for line ends
    for (inLR = inLL + inDx, inUR = inUL + inDx;
         inLR != inLEnd && areEqual(*inLL, *inLR) && areEqual(*inUL, *inUR);
         inLR += inDx, inUR += inDx);

    if (inLR == inLEnd) break;  // Dealt with later

    // Found a line to process
    lLine = (inLR - inLL) / inDx;

    // First, filter left to right
    inLL_1 = inLL - inDx, inUL_1 = inUL - inDx;
    x = (inLL - inLRow) / inDx;

    uniteUL = areEqual(*inUL, *inLL_1);
    uniteLL = areEqual(*inLL, *inUL_1);
    if (uniteUL || uniteLL) {
      if (uniteUL && uniteLL)
        uniteUL = checkNeighbourHood(x, r, inUL, lx, ly, inDx, inDy);

      if (checkLength(lLine, r, ly, inDy, inLL_1, inUL_1, inLL, inUL, uniteUL,
                      do1Line))
        filterLine(inLL, inUL, outLRow + x * outLDx, outURow + x * outUDx,
                   lLine, inDx, outLDx, outUDx, hStart, slope / lLine, uniteUL,
                   isHorizontal, false);
    }

    // Then, filter right to left
    inLR_1 = inLR - inDx, inUR_1 = inUR - inDx;
    x = (inLR_1 - inLRow) / inDx;

    uniteUR = areEqual(*inUR_1, *inLR);
    uniteLR = areEqual(*inLR_1, *inUR);
    if (uniteUR || uniteLR) {
      if (uniteUR && uniteLR)
        uniteUR = !checkNeighbourHood(x + 1, r, inUR, lx, ly, inDx, inDy);

      if (checkLength(lLine, r, ly, inDy, inLR_1, inUR_1, inLR, inUR, uniteUR,
                      do1Line))
        filterLine(inLR_1, inUR_1, outLRow + x * outLDx, outURow + x * outUDx,
                   lLine, -inDx, -outLDx, -outUDx, hStart, slope / lLine,
                   uniteUR, isHorizontal, true);
    }

    // Update lefts - search for a new line start
    inLL = inLR, inUL = inUR;
    for (; inLL != inLEnd && areEqual(*inLL, *inUL);
         inLL += inDx, inUL += inDx);
  }

  // Special case: filter the last line in the row
  if (inLL != inLEnd) {
    // Found a line to process
    lLine = (inLR - inLL) / inDx;

    inLL_1 = inLL - inDx, inUL_1 = inUL - inDx;
    x = (inLL - inLRow) / inDx;

    uniteUL = areEqual(*inUL, *inLL_1);
    uniteLL = areEqual(*inLL, *inUL_1);
    if (uniteUL || uniteLL) {
      if (uniteUL && uniteLL)
        uniteUL = checkNeighbourHood(x, r, inUL, lx, ly, inDx, inDy);

      if (checkLength(lLine, r, ly, inDy, inLL_1, inUL_1, inLL, inUL, uniteUL,
                      do1Line))
        filterLine(inLL, inUL, outLRow + x * outLDx, outURow + x * outUDx,
                   lLine, inDx, outLDx, outUDx, hStart, slope / (lLine << 1),
                   uniteUL, isHorizontal, false);
    }
  }
}

//---------------------------------------------------------------------------------------

template <typename PIX>
void makeMLSSRefRas(const TRasterPT<PIX> &src, TRasterPT<TPixel64> &dst) {
  // dst->copy(src);
  double slope  = 1.;
  double hStart = 0.5;  // fixed for now

  src->lock();
  dst->lock();

  // First, filter by rows
  int x, y, lx = src->getLx(), ly = src->getLy(), lx_1 = lx - 1, ly_1 = ly - 1;
  for (y = 0; y < ly_1; ++y) {
    processLine(y, lx, ly, src->pixels(y), src->pixels(y + 1), dst->pixels(y),
                dst->pixels(y + 1), 1, src->getWrap(), 1, 1, true, hStart,
                slope);
  }

  // Then, go by columns
  for (x = 0; x < lx_1; ++x) {
    processLine(x, ly, lx, src->pixels(0) + x, src->pixels(0) + x + 1,
                dst->pixels(0) + x, dst->pixels(0) + x + 1, src->getWrap(), 1,
                dst->getWrap(), dst->getWrap(), false, hStart, slope);
  }

  dst->unlock();
  src->unlock();
}

//---------------------------------------------------
// Morphological Supersamplingを行う場合、サンプル用の境界線マップ画像を生成する
// RGBA各チャンネルにそれぞれ下記の値をhalf-floatで入れる
// R:水平メタ境界線の左の切片 G:水平メタ境界線の右の切片
// B:垂直メタ境界線の下の切片 A:垂直メタ境界線の上の切片
TRaster64P IwRenderInstance::createMLSSRefRas(TRaster64P inRas) {
  if (m_project->getRenderSettings()->getResampleMode() !=
      MorphologicalSupersampling)
    return TRaster64P();

  TRaster64P ret(inRas->getSize());
  ret->fill(TPixel64(0, 0, 0, 0));
  makeMLSSRefRas<TPixel64>(inRas, ret);
  return ret;
}
