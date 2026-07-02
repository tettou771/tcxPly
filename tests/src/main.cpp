// =============================================================================
// tcxPly tests - headless behavioral test (no window).
//
// Built and run by CI (TrussC-org/ci-actions build-addon.yml): exit 0 = all
// pass, non-zero = failure -> CI fails. Console only, so it runs on headless
// macOS / Windows / Linux runners. Files are written to the OS temp dir via
// absolute paths (tc::getDataPath passes absolute paths through).
// =============================================================================

#include <tcxPly.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace std;
using namespace tc;
using namespace tcx;

static int g_pass = 0, g_fail = 0;
static void check(const char* name, bool ok) {
    printf("%-54s %s\n", name, ok ? "PASS" : "FAIL");
    fflush(stdout);
    ok ? ++g_pass : ++g_fail;
}
static bool approx(float a, float b, float eps = 1e-3f) { return fabs(a - b) <= eps; }

static string tmp(const char* name) {
    return (filesystem::temp_directory_path() / name).string();
}

// 8-corner cube, edge 100, centered at origin. Vertex color encodes position;
// a per-vertex "quality" float and a per-face "face_id" exercise non-standard
// scalar columns alongside the standard attributes and the face list.
static const float CUBE[8][3] = {
    {-50,-50,-50},{50,-50,-50},{50,50,-50},{-50,50,-50},
    {-50,-50, 50},{50,-50, 50},{50,50, 50},{-50,50, 50}};
static const int FACES[6][4] = {
    {0,1,2,3},{7,6,5,4},{0,3,7,4},{1,5,6,2},{3,2,6,7},{0,4,5,1}};

static string makeAsciiCube() {
    string s =
        "ply\nformat ascii 1.0\n"
        "comment hello tcxPly\n"
        "obj_info scanner test\n"
        "element vertex 8\n"
        "property float x\nproperty float y\nproperty float z\n"
        "property uchar red\nproperty uchar green\nproperty uchar blue\n"
        "property float quality\n"
        "element face 6\n"
        "property list uchar int vertex_indices\n"
        "property uchar face_id\n"
        "end_header\n";
    char buf[256];
    for (int i = 0; i < 8; ++i) {
        int r = (int)((CUBE[i][0] + 50) / 100 * 255);
        int g = (int)((CUBE[i][1] + 50) / 100 * 255);
        int b = (int)((CUBE[i][2] + 50) / 100 * 255);
        snprintf(buf, sizeof(buf), "%g %g %g %d %d %d %g\n",
                 CUBE[i][0], CUBE[i][1], CUBE[i][2], r, g, b, i * 0.5f);
        s += buf;
    }
    for (int f = 0; f < 6; ++f) {
        snprintf(buf, sizeof(buf), "4 %d %d %d %d %d\n",
                 FACES[f][0], FACES[f][1], FACES[f][2], FACES[f][3], 100 + f);
        s += buf;
    }
    return s;
}

// Hand-write a binary_big_endian vertex-only PLY (3 points) so we can prove the
// big-endian read path without depending on any external tool.
static void writeBigEndianPoints(const string& path) {
    ofstream o(path, ios::binary);
    o << "ply\nformat binary_big_endian 1.0\n"
         "element vertex 3\n"
         "property float x\nproperty float y\nproperty float z\n"
         "end_header\n";
    float pts[3][3] = {{1.f, 2.f, 3.f}, {-4.f, 5.f, -6.f}, {7.5f, -8.25f, 9.f}};
    for (auto& p : pts) {
        for (float v : p) {
            uint8_t b[4];
            memcpy(b, &v, 4);
            // host is almost always little-endian on CI runners -> reverse to BE
            uint16_t one = 1;
            if (*reinterpret_cast<uint8_t*>(&one) == 1) { swap(b[0], b[3]); swap(b[1], b[2]); }
            o.write(reinterpret_cast<char*>(b), 4);
        }
    }
}

int main() {
    // ----- 1. ASCII parse: standard attrs + non-standard scalar columns -----
    {
        string path = tmp("tcxply_cube.ply");
        { ofstream o(path, ios::binary); o << makeAsciiCube(); }

        Ply ply;
        bool ok = ply.load(path);
        check("ascii: load succeeds", ok);
        check("ascii: 8 vertices", ply.getNumVertices() == 8);
        check("ascii: 6 faces", ply.getNumFaces() == 6);

        BoundingBox bb = ply.getBoundingBox();
        check("ascii: bbox not empty", !bb.empty);
        check("ascii: bbox size 100^3",
              approx(bb.size().x, 100) && approx(bb.size().y, 100) && approx(bb.size().z, 100));
        check("ascii: bbox center origin",
              approx(bb.center().x, 0) && approx(bb.center().y, 0) && approx(bb.center().z, 0));

        // quality is a `float` property -> getVertexProperty<float> (the default)
        vector<float> q = ply.getVertexProperty<float>("quality");
        check("typed: quality<float> len 8", q.size() == 8);
        check("typed: quality[3] == 1.5", q.size() == 8 && approx(q[3], 1.5f));

        // face_id is a `uchar` property -> must request it as uint8_t
        vector<uint8_t> fid = ply.getFaceProperty<uint8_t>("face_id");
        check("typed: face_id<uint8_t> len 6", fid.size() == 6);
        check("typed: face_id[2] == 102", fid.size() == 6 && fid[2] == 102);

        // strictness: no implicit conversion. Wrong type -> empty (+warning).
        check("typed: quality<double> mismatch -> empty",
              ply.getVertexProperty<double>("quality").empty());
        check("typed: face_id<float> mismatch -> empty",
              ply.getFaceProperty<float>("face_id").empty());
        check("typed: vertex_indices is list -> empty",
              ply.getFaceProperty<int32_t>("vertex_indices").empty());
        check("typed: missing property -> empty",
              ply.getVertexProperty<float>("nope").empty());

        // standard fields are fetched the same way: red is `uchar`
        vector<uint8_t> red = ply.getVertexProperty<uint8_t>("red");
        check("typed: red<uint8_t> len 8", red.size() == 8);
        check("typed: red<float> mismatch -> empty",
              ply.getVertexProperty<float>("red").empty());

        // property list carries name + type
        auto vprops = ply.getVertexProperties();
        bool xIsFloat = false, redIsUchar = false;
        for (auto& p : vprops) {
            if (p.name == "x")   xIsFloat   = (p.type == PlyType::Float32 && !p.isList);
            if (p.name == "red") redIsUchar = (p.type == PlyType::Uint8   && !p.isList);
        }
        check("properties: x is float, red is uchar", xIsFloat && redIsUchar);
        auto fprops = ply.getFaceProperties();
        bool viIsList = false;
        for (auto& p : fprops) if (p.name == "vertex_indices") viIsList = p.isList;
        check("properties: vertex_indices is a list", viIsList);

        // toMesh: quads fan-triangulate to 2 tris each -> 36 indices, colors present
        Mesh m = ply.toMesh();
        check("toMesh: 8 verts", m.getNumVertices() == 8);
        check("toMesh: 36 indices (6 quads x2 tris)", m.getNumIndices() == 36);
        check("toMesh: has colors", m.hasColors());
        check("toMesh: mode Triangles", m.getMode() == tc::PrimitiveMode::Triangles);
        // vertex 6 is (+50,+50,+50) -> color ~ (1,1,1)
        if (m.hasColors() && m.getNumVertices() == 8) {
            const auto& c = m.getColors()[6];
            check("toMesh: color uchar->0..1 (v6 ~white)",
                  approx(c.r, 1.f, 0.01f) && approx(c.g, 1.f, 0.01f) && approx(c.b, 1.f, 0.01f));
        }

        // metadata preserved on the Ply object
        check("ascii: 1 comment kept", ply.getComments().size() == 1 &&
                                       ply.getComments()[0] == "hello tcxPly");
        check("ascii: 1 obj_info kept", ply.getObjInfo().size() == 1 &&
                                        ply.getObjInfo()[0] == "scanner test");
    }

    // ----- 2. Round trip via Mesh: ASCII and binary little-endian -----
    for (PlyFormat fmt : {PlyFormat::Ascii, PlyFormat::BinaryLittleEndian}) {
        const char* tag = (fmt == PlyFormat::Ascii) ? "ascii" : "binaryLE";

        Mesh src;
        src.setMode(tc::PrimitiveMode::Triangles);
        for (int i = 0; i < 8; ++i) {
            src.addVertex(CUBE[i][0], CUBE[i][1], CUBE[i][2]);
            src.addColor(tc::Color((CUBE[i][0] + 50) / 100, (CUBE[i][1] + 50) / 100,
                                   (CUBE[i][2] + 50) / 100, 1.0f));
        }
        for (auto& f : FACES) {  // each quad as two triangles
            src.addTriangle(f[0], f[1], f[2]);
            src.addTriangle(f[0], f[2], f[3]);
        }

        string path = tmp(fmt == PlyFormat::Ascii ? "tcxply_rt_a.ply" : "tcxply_rt_b.ply");
        bool sOk = savePly(path, src, fmt);
        Ply in;
        bool lOk = in.load(path);
        Mesh back = in.toMesh();

        string n;
        n = string("roundtrip ") + tag + ": save+load ok";
        check(n.c_str(), sOk && lOk);
        n = string("roundtrip ") + tag + ": vertex count matches";
        check(n.c_str(), back.getNumVertices() == src.getNumVertices());
        n = string("roundtrip ") + tag + ": index count matches";
        check(n.c_str(), back.getNumIndices() == src.getNumIndices());

        bool posOk = back.getNumVertices() == src.getNumVertices();
        for (int i = 0; posOk && i < back.getNumVertices(); ++i) {
            const auto& a = src.getVertices()[i];
            const auto& b = back.getVertices()[i];
            posOk = approx(a.x, b.x) && approx(a.y, b.y) && approx(a.z, b.z);
        }
        n = string("roundtrip ") + tag + ": positions preserved";
        check(n.c_str(), posOk);

        bool colOk = back.hasColors() && back.getNumColors() == src.getNumColors();
        for (int i = 0; colOk && i < back.getNumColors(); ++i) {
            const auto& a = src.getColors()[i];
            const auto& b = back.getColors()[i];
            colOk = approx(a.r, b.r, 0.01f) && approx(a.g, b.g, 0.01f) && approx(a.b, b.b, 0.01f);
        }
        n = string("roundtrip ") + tag + ": colors preserved (uchar quantized)";
        check(n.c_str(), colOk);
    }

    // ----- 3. Point cloud: vertices only -> Points mode, no faces -----
    {
        Mesh cloud;
        cloud.setMode(tc::PrimitiveMode::Points);
        for (int i = 0; i < 50; ++i) cloud.addVertex((float)i, (float)(i * 2), (float)(-i));
        string path = tmp("tcxply_cloud.ply");
        savePly(path, cloud, PlyFormat::BinaryLittleEndian);

        Ply in;
        in.load(path);
        check("cloud: 50 verts", in.getNumVertices() == 50);
        check("cloud: 0 faces", in.getNumFaces() == 0);
        Mesh back = in.toMesh();
        check("cloud: toMesh mode Points", back.getMode() == tc::PrimitiveMode::Points);
        check("cloud: no indices", !back.hasIndices());
    }

    // ----- 4. Metadata round trip -----
    {
        Mesh m;
        for (int i = 0; i < 3; ++i) m.addVertex((float)i, 0, 0);
        Ply out;
        out.setMesh(m);
        out.addComment("created by testApp");
        out.addObjInfo("unit test");
        string path = tmp("tcxply_meta.ply");
        out.save(path, PlyFormat::Ascii);

        Ply in;
        in.load(path);
        check("meta: comment survives round trip",
              in.getComments().size() == 1 && in.getComments()[0] == "created by testApp");
        check("meta: obj_info survives round trip",
              in.getObjInfo().size() == 1 && in.getObjInfo()[0] == "unit test");
    }

    // ----- 5. Binary big-endian read -----
    {
        string path = tmp("tcxply_be.ply");
        writeBigEndianPoints(path);
        Ply in;
        bool ok = in.load(path);
        check("big-endian: load ok", ok);
        check("big-endian: 3 verts", in.getNumVertices() == 3);
        vector<float> xs = in.getVertexProperty("x");
        vector<float> ys = in.getVertexProperty("y");
        vector<float> zs = in.getVertexProperty("z");
        bool vals = xs.size() == 3 &&
                    approx(xs[0], 1.f) && approx(ys[0], 2.f) && approx(zs[0], 3.f) &&
                    approx(xs[2], 7.5f) && approx(ys[2], -8.25f) && approx(zs[2], 9.f);
        check("big-endian: values decoded correctly", vals);
    }

    // ----- 6. Error handling: not a PLY file -----
    {
        string path = tmp("tcxply_bad.ply");
        { ofstream o(path, ios::binary); o << "this is not a ply\n12345\n"; }
        Ply in;
        bool ok = in.load(path);
        check("error: non-PLY rejected (returns false)", !ok);
        check("error: empty after failed load", in.getNumVertices() == 0);
    }

    printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
