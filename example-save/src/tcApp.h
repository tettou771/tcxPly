#pragma once

#include <TrussC.h>
#include <tcxPly.h>

using namespace std;
using namespace tc;
using namespace tcx;

// example-save - generate a mesh, write it to PLY (ASCII + binary), read it
// back, and confirm the round trip. The reloaded mesh is what gets drawn, so
// what you see on screen came back off disk.
class tcApp : public App {
public:
    void setup() override;
    void update() override;
    void draw() override;

private:
    EasyCam cam;
    Mesh    reloaded;          // sphere, written then read back
    Material mat;
    std::string status;
};
