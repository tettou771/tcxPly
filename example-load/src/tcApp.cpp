#include "tcApp.h"
#include <algorithm>
#include <sstream>

void tcApp::setup() {
    setWindowTitle("tcxPly - example-load");

    cam.enableMouseInput();

    light.setDirectional(Vec3(-0.7f, -1.0f, -0.4f));
    light.setAmbient(0.6f, 0.6f, 0.65f);
    light.setDiffuse(0.7f, 0.7f, 0.7f);
    mat = Material::plastic(Color(0.85f, 0.85f, 0.88f));

    // fragment.ply - a real RGB-colored point cloud scan from the Redwood
    // Living Room dataset (public domain). It also carries a non-standard
    // `curvature` vertex column and a `camera` element, which tcxPly's generic
    // parser reads (and toMesh ignores) without trouble.
    if (heroPly.load("fragment.ply")) {
        hero = heroPly.toMesh();   // RGB colors, no faces -> PrimitiveMode::Points
        frameBox(heroPly.getBoundingBox());
        logNotice() << "fragment.ply: " << heroPly.getNumVertices()
                    << " colored points, " << heroPly.getNumFaces() << " faces";
    }
}

void tcApp::frameBox(const BoundingBox& bb) {
    Vec3 c = bb.center();
    cam.setTarget(c.x, c.y, c.z);
    float diag = bb.size().length();
    cam.setDistance(diag > 0 ? diag * 1.3f : 3.0f);
}

void tcApp::drawMesh(const Mesh& m) {
    // Point clouds (no faces) can only be drawn as dots.
    RenderMode mode = m.hasIndices() ? rmode : RenderMode::Dots;

    switch (mode) {
        case RenderMode::Face:
            addLight(light);
            setCameraPosition(cam.getPosition());
            setMaterial(mat);
            setColor(1.0f);
            if (m.hasColors()) m.drawWithLighting();  // per-vertex color + lighting
            else               m.draw();              // material-shaded
            clearMaterial();
            clearLights();
            break;
        case RenderMode::Wireframe:
            setColor(0.8f, 0.85f, 0.9f);
            m.drawWireframe();
            break;
        case RenderMode::Dots:
            setColor(1.0f);
            if (m.getMode() == PrimitiveMode::Points) {
                m.draw();
            } else {
                Mesh pts = m;                          // draw vertices as points
                pts.setMode(PrimitiveMode::Points);
                pts.clearIndices();
                pts.draw();
            }
            break;
    }
}

void tcApp::loadFile(const std::string& path) {
    Ply ply;
    if (!ply.load(path)) return;

    Mesh m = ply.toMesh();

    // If the file has no explicit vertex color but carries 3D Gaussian Splat
    // spherical-harmonics DC terms (f_dc_0/1/2, as Blender/3DGS exports do),
    // derive a base color from them so the cloud isn't a flat white blob.
    if (!m.hasColors()) {
        auto r = ply.getVertexProperty("f_dc_0");
        auto g = ply.getVertexProperty("f_dc_1");
        auto b = ply.getVertexProperty("f_dc_2");
        const size_t n = m.getNumVertices();
        if (r.size() == n && g.size() == n && b.size() == n) {
            const float C0 = 0.28209479177387814f;  // SH band-0 constant
            auto sh = [&](float v) { return std::clamp(0.5f + C0 * v, 0.0f, 1.0f); };
            for (size_t i = 0; i < n; ++i)
                m.addColor(Color(sh(r[i]), sh(g[i]), sh(b[i]), 1.0f));
            logNotice() << "  colored from Gaussian-Splat f_dc_* SH terms";
        } else if (m.hasNormals()) {
            // geometry-only mesh: colorize by normal so it isn't flat gray
            for (size_t i = 0; i < n && i < m.getNormals().size(); ++i) {
                const Vec3& nv = m.getNormals()[i];
                m.addColor(Color(nv.x * 0.5f + 0.5f, nv.y * 0.5f + 0.5f, nv.z * 0.5f + 0.5f, 1.0f));
            }
        }
    }

    dropped     = std::move(m);
    droppedBbox = ply.getBoundingBox();
    droppedName = getFileName(path);
    hasDropped  = true;
    frameBox(droppedBbox);

    logNotice() << "dropped " << droppedName << ": " << dropped.getNumVertices()
                << " verts, " << ply.getNumFaces() << " faces, "
                << (dropped.getMode() == PrimitiveMode::Points ? "points" : "mesh");
}

void tcApp::filesDropped(const std::vector<std::string>& files) {
    for (const auto& f : files) {
        std::string ext = getFileExtension(f);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext == "ply") { loadFile(f); break; }
    }
}

void tcApp::update() {}

void tcApp::draw() {
    clear(0.08f);

    const Mesh& shown = hasDropped ? dropped : hero;
    const BoundingBox& bb = hasDropped ? droppedBbox : heroPly.getBoundingBox();

    cam.begin();
    drawMesh(shown);

    // bounding box wireframe of the shown object
    setColor(0.35f, 0.35f, 0.4f);
    Vec3 c = bb.center(), s = bb.size();
    pushMatrix();
    translate(c.x, c.y, c.z);
    Mesh box = createBox(s.x > 0 ? s.x : 1, s.y > 0 ? s.y : 1, s.z > 0 ? s.z : 1);
    box.drawWireframe();
    popMatrix();
    cam.end();

    const char* modeName = rmode == RenderMode::Face ? "face"
                         : rmode == RenderMode::Wireframe ? "wireframe" : "dots";

    setColor(1.0f);
    std::stringstream ss;
    ss << "tcxPly example-load\n\n";
    if (hasDropped)
        ss << "showing: " << droppedName << " (" << dropped.getNumVertices() << " verts)\n";
    else
        ss << "showing: " << heroName << " (" << hero.getNumVertices() << " pts)\n";
    ss << "render: " << modeName << "   [SPACE] cycle face / wireframe / dots\n";
    ss << "[drag & drop a .ply]   [drag] rotate   [scroll] zoom\n\n";
    ss << "bbox size: " << toString(s.x, 3) << " x " << toString(s.y, 3)
       << " x " << toString(s.z, 3) << "\n";
    drawBitmapString(ss.str(), 20, 20);
}

void tcApp::keyPressed(int key) {
    if (key == ' ') {
        rmode = static_cast<RenderMode>((static_cast<int>(rmode) + 1) % 3);
    }
}
