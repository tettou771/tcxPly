#pragma once

#include <TrussC.h>
#include <tcxPly.h>

using namespace std;
using namespace tc;
using namespace tcx;

// example-load - load a PLY file into a tc::Mesh and display it.
//   Starts on fragment.ply, a real colored point-cloud scan (Redwood, public
//   domain). Drag & drop any .ply to load it; meshes also honor:
//   [SPACE] cycles the render mode: face -> wireframe -> dots
class tcApp : public App {
public:
    void setup() override;
    void update() override;
    void draw() override;
    void keyPressed(int key) override;
    void filesDropped(const std::vector<std::string>& files) override;

    enum class RenderMode { Face, Wireframe, Dots };

private:
    EasyCam cam;
    Light   light;
    Material mat;
    RenderMode rmode = RenderMode::Face;

    Ply  heroPly;
    Mesh hero;          // fragment.ply - colored point cloud (no faces)
    std::string heroName = "fragment.ply (Redwood scan, public domain)";

    Mesh        dropped;        // last drag&dropped file (replaces hero once set)
    bool        hasDropped = false;
    std::string droppedName;
    BoundingBox droppedBbox;

    void drawMesh(const Mesh& m);          // honors rmode
    void loadFile(const std::string& path);
    void frameBox(const BoundingBox& bb);  // aim camera at a box
};
