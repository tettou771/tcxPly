#include "tcApp.h"
#include <sstream>

void tcApp::setup() {
    setWindowTitle("tcxPly - example-save");

    cam.setDistance(300);
    cam.enableMouseInput();

    // 1. Generate a mesh (sphere has positions, normals and texcoords).
    Mesh sphere = createSphere(80, 24);

    // 2. Write it both ways, with a comment for good measure.
    Ply out;
    out.setMesh(sphere);
    out.addComment("written by tcxPly example-save");
    bool aOk = out.save("sphere_ascii.ply", PlyFormat::Ascii);
    bool bOk = out.save("sphere_bin.ply",  PlyFormat::BinaryLittleEndian);

    // 3. Read the binary one back and draw THAT (proves the round trip).
    Ply in;
    bool rOk = in.load("sphere_bin.ply");
    reloaded = in.toMesh();

    std::stringstream ss;
    ss << "save ascii: " << (aOk ? "ok" : "FAIL")
       << "  save bin: " << (bOk ? "ok" : "FAIL")
       << "  reload: "   << (rOk ? "ok" : "FAIL") << "\n";
    ss << "orig verts: " << sphere.getNumVertices()
       << "  reloaded verts: " << reloaded.getNumVertices() << "\n";
    BoundingBox bb = in.getBoundingBox();
    ss << "reloaded bbox size: " << (int)bb.size().x << " x "
       << (int)bb.size().y << " x " << (int)bb.size().z;
    status = ss.str();
    logNotice() << status;

    mat = Material::plastic(Color(0.3f, 0.7f, 0.9f));
}

void tcApp::update() {}

void tcApp::draw() {
    clear(0.08f);

    cam.begin();
    Light light;
    light.setDirectional(Vec3(-0.7f, -1.0f, -0.4f));
    light.setAmbient(0.7f, 0.7f, 0.75f);
    light.setDiffuse(0.6f, 0.6f, 0.6f);
    addLight(light);
    setCameraPosition(cam.getPosition());
    setColor(1.0f);
    setMaterial(mat);
    reloaded.draw();
    clearMaterial();
    clearLights();
    cam.end();

    setColor(1.0f);
    drawBitmapString(status, 20, 20);
}
