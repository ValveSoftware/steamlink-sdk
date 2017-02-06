/*
 * Authors: kaen, raptor, sam686, watusimoto
 *
 * Originally from the bitfighter source code
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2014 Bitfighter developers
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef CLIP2TRI_H_
#define CLIP2TRI_H_

#include <vector>
#include "../clipper/clipper.h"

using namespace std;
using namespace ClipperLib;

namespace c2t
{

typedef signed int       S32;
typedef signed long long S64;
typedef unsigned int     U32;
typedef float            F32;
typedef double           F64;


struct Point
{
   F32 x;
   F32 y;

   Point();
   Point(const Point &pt);

   template<class T, class U>
   Point(T in_x, U in_y) { x = static_cast<F32>(in_x); y = static_cast<F32>(in_y); }
};


class clip2tri
{
private:
   //
   Path upscaleClipperPoints(const vector<Point> &inputPolygon);

   // These operate on a vector of polygons
   Paths upscaleClipperPoints(const vector<vector<Point> > &inputPolygons);
   vector<vector<Point> > downscaleClipperPoints(const Paths &inputPolygons);

   bool mergePolysToPolyTree(const vector<vector<Point> > &inputPolygons, PolyTree &solution);

   bool triangulateComplex(vector<Point> &outputTriangles, const Path &outline,
         const PolyTree &polyTree, bool ignoreFills = true, bool ignoreHoles = false);

public:
   clip2tri();
   virtual ~clip2tri();

   void triangulate(const vector<vector<Point> > &inputPolygons, vector<Point> &outputTriangles,
         const vector<Point> &boundingPolygon);
};

} /* namespace c2t */

#endif /* CLIP2TRI_H_ */
