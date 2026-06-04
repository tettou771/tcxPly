# tcxPly Licenses

## tcxPly

MIT License

Copyright (c) 2026 tettou771

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

---

## fragment.ply — example-load/bin/data/fragment.ply

An RGB-colored point cloud from the **Redwood "Living Room" indoor scan
dataset**, which its authors irrevocably placed in the **public domain**. The
original 196,133-point cloud is redistributed by the Open3D sample-data set
(Open3D itself is MIT-licensed); the scan data is public domain and requires no
attribution.

- Redwood dataset: <http://redwood-data.org/indoor/> (public domain)
- Open3D (sample-data redistribution, MIT): <https://github.com/isl-org/Open3D>
- Original file: <https://github.com/isl-org/open3d_downloads/releases/download/20220201-data/fragment.ply>

The bundled copy is **derived** from that public-domain data: downsampled to
1/4 (49,034 points), flipped upright (Y negated), and re-saved as a
binary-little-endian PLY by tcxPly itself — all vertex properties (RGB, normals,
`curvature`) and the `camera` element are preserved. Public-domain data stays
public domain, so the derived file carries no additional restrictions.
