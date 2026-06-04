#include "tcxPly.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>

using namespace std;
using namespace tc;

namespace tcx {

// =============================================================================
// Endianness & low-level byte helpers
// =============================================================================

static bool hostIsLittleEndian() {
    uint16_t x = 1;
    return *reinterpret_cast<uint8_t*>(&x) == 1;
}

static void swapBytes(void* p, int size) {
    auto* b = static_cast<uint8_t*>(p);
    for (int i = 0; i < size / 2; ++i) std::swap(b[i], b[size - 1 - i]);
}

// =============================================================================
// PLY type name <-> enum
// =============================================================================

static PlyType parsePlyType(const string& s) {
    if (s == "char"   || s == "int8")    return PlyType::Int8;
    if (s == "uchar"  || s == "uint8")   return PlyType::Uint8;
    if (s == "short"  || s == "int16")   return PlyType::Int16;
    if (s == "ushort" || s == "uint16")  return PlyType::Uint16;
    if (s == "int"    || s == "int32")   return PlyType::Int32;
    if (s == "uint"   || s == "uint32")  return PlyType::Uint32;
    if (s == "float"  || s == "float32") return PlyType::Float32;
    if (s == "double" || s == "float64") return PlyType::Float64;
    return PlyType::Invalid;
}

// plyTypeName(PlyType) is defined inline in tcxPlyTypes.h.

// =============================================================================
// Binary scalar read / write (interpreting the raw bytes per type)
// =============================================================================

static double readBinaryScalar(istream& in, PlyType t, bool swap) {
    char raw[8] = {0};
    int sz = plyTypeSize(t);
    in.read(raw, sz);
    if (swap && sz > 1) swapBytes(raw, sz);
    switch (t) {
        case PlyType::Int8:    { int8_t   v; memcpy(&v, raw, 1); return (double)v; }
        case PlyType::Uint8:   { uint8_t  v; memcpy(&v, raw, 1); return (double)v; }
        case PlyType::Int16:   { int16_t  v; memcpy(&v, raw, 2); return (double)v; }
        case PlyType::Uint16:  { uint16_t v; memcpy(&v, raw, 2); return (double)v; }
        case PlyType::Int32:   { int32_t  v; memcpy(&v, raw, 4); return (double)v; }
        case PlyType::Uint32:  { uint32_t v; memcpy(&v, raw, 4); return (double)v; }
        case PlyType::Float32: { float    v; memcpy(&v, raw, 4); return (double)v; }
        case PlyType::Float64: { double   v; memcpy(&v, raw, 8); return v; }
        default:               return 0.0;
    }
}

// Always writes little-endian (the only binary format we emit).
static void writeBinaryScalarLE(ostream& out, PlyType t, double value) {
    char raw[8] = {0};
    int sz = plyTypeSize(t);
    switch (t) {
        case PlyType::Int8:    { int8_t   v = (int8_t)  llround(value); memcpy(raw, &v, 1); break; }
        case PlyType::Uint8:   { uint8_t  v = (uint8_t) llround(value); memcpy(raw, &v, 1); break; }
        case PlyType::Int16:   { int16_t  v = (int16_t) llround(value); memcpy(raw, &v, 2); break; }
        case PlyType::Uint16:  { uint16_t v = (uint16_t)llround(value); memcpy(raw, &v, 2); break; }
        case PlyType::Int32:   { int32_t  v = (int32_t) llround(value); memcpy(raw, &v, 4); break; }
        case PlyType::Uint32:  { uint32_t v = (uint32_t)llround(value); memcpy(raw, &v, 4); break; }
        case PlyType::Float32: { float    v = (float)value;             memcpy(raw, &v, 4); break; }
        case PlyType::Float64: { double   v = value;                    memcpy(raw, &v, 8); break; }
        default: return;
    }
    if (!hostIsLittleEndian() && sz > 1) swapBytes(raw, sz);
    out.write(raw, sz);
}

// Format a value for ASCII output: integers without a decimal point, floats
// with enough precision to round-trip a float32.
static void writeAsciiScalar(ostream& out, PlyType t, double value) {
    if (plyTypeIsFloat(t)) {
        out << setprecision(9) << (double)(float)value;
    } else {
        out << (long long)llround(value);
    }
}

// =============================================================================
// Header line tokenizer
// =============================================================================

static vector<string> tokenize(const string& line) {
    vector<string> out;
    istringstream ss(line);
    string tok;
    while (ss >> tok) out.push_back(tok);
    return out;
}

static void stripCR(string& s) {
    if (!s.empty() && s.back() == '\r') s.pop_back();
}

// Text after the first token + single space, with trailing CR stripped (used
// for comment / obj_info, which are free text and must be kept verbatim).
static string trailingText(const string& line, const string& keyword) {
    size_t pos = keyword.size();
    if (pos < line.size() && line[pos] == ' ') ++pos;
    string s = line.substr(min(pos, line.size()));
    stripCR(s);
    return s;
}

// =============================================================================
// Ply::load
// =============================================================================

bool Ply::load(const string& path) {
    comments_.clear();
    objInfo_.clear();
    elements_.clear();
    format_ = PlyFormat::Ascii;

    string full = getDataPath(path);
    ifstream in(full, ios::binary);
    if (!in) {
        logError() << "[tcxPly] cannot open: " << full;
        return false;
    }

    // ---- header ----
    string line;
    if (!getline(in, line)) { logError() << "[tcxPly] empty file: " << full; return false; }
    stripCR(line);
    if (line != "ply") {
        logError() << "[tcxPly] not a PLY file (missing 'ply' magic): " << full;
        return false;
    }

    bool sawFormat = false;
    bool sawEndHeader = false;
    while (getline(in, line)) {
        stripCR(line);
        if (line.rfind("comment", 0) == 0)  { comments_.push_back(trailingText(line, "comment")); continue; }
        if (line.rfind("obj_info", 0) == 0) { objInfo_.push_back(trailingText(line, "obj_info")); continue; }

        vector<string> t = tokenize(line);
        if (t.empty()) continue;

        if (t[0] == "format") {
            if (t.size() < 2) { logError() << "[tcxPly] malformed format line"; return false; }
            if      (t[1] == "ascii")                format_ = PlyFormat::Ascii;
            else if (t[1] == "binary_little_endian") format_ = PlyFormat::BinaryLittleEndian;
            else if (t[1] == "binary_big_endian")    format_ = PlyFormat::BinaryBigEndian;
            else { logError() << "[tcxPly] unknown format: " << t[1]; return false; }
            sawFormat = true;
        } else if (t[0] == "element") {
            if (t.size() < 3) { logError() << "[tcxPly] malformed element line"; return false; }
            PlyElement e;
            e.name = t[1];
            e.count = (size_t)strtoull(t[2].c_str(), nullptr, 10);
            elements_.push_back(std::move(e));
        } else if (t[0] == "property") {
            if (elements_.empty()) { logError() << "[tcxPly] property before any element"; return false; }
            PlyProperty p;
            if (t[1] == "list") {
                // property list <countType> <valueType> <name>
                if (t.size() < 5) { logError() << "[tcxPly] malformed list property"; return false; }
                p.isList    = true;
                p.countType = parsePlyType(t[2]);
                p.valueType = parsePlyType(t[3]);
                p.name      = t[4];
            } else {
                // property <type> <name>
                if (t.size() < 3) { logError() << "[tcxPly] malformed property"; return false; }
                p.isList    = false;
                p.valueType = parsePlyType(t[1]);
                p.name      = t[2];
            }
            if (p.valueType == PlyType::Invalid) {
                logError() << "[tcxPly] unknown property type in: " << line;
                return false;
            }
            elements_.back().properties.push_back(std::move(p));
        } else if (t[0] == "end_header") {
            sawEndHeader = true;
            break;
        }
        // unknown header lines are ignored (forward-compatible)
    }

    if (!sawFormat)    { logError() << "[tcxPly] header had no format line"; return false; }
    if (!sawEndHeader) { logError() << "[tcxPly] header had no end_header";   return false; }

    // ---- data ----
    const bool fileLE = (format_ != PlyFormat::BinaryBigEndian);
    const bool swap   = (format_ != PlyFormat::Ascii) && (fileLE != hostIsLittleEndian());

    for (PlyElement& e : elements_) {
        // pre-size columns
        for (PlyProperty& p : e.properties) {
            if (p.isList) p.list.assign(e.count, {});
            else          p.scalar.assign(e.count, 0.0);
        }

        if (format_ == PlyFormat::Ascii) {
            for (size_t row = 0; row < e.count; ++row) {
                if (!getline(in, line)) {
                    logError() << "[tcxPly] unexpected EOF in element '" << e.name
                               << "' at row " << row;
                    return false;
                }
                stripCR(line);
                istringstream ss(line);
                for (PlyProperty& p : e.properties) {
                    if (p.isList) {
                        double n = 0; ss >> n;
                        int cnt = (int)n;
                        auto& dst = p.list[row];
                        dst.resize(max(0, cnt));
                        for (int i = 0; i < cnt; ++i) ss >> dst[i];
                    } else {
                        ss >> p.scalar[row];
                    }
                }
                if (ss.fail()) {
                    logError() << "[tcxPly] parse error in element '" << e.name
                               << "' at row " << row;
                    return false;
                }
            }
        } else {
            for (size_t row = 0; row < e.count; ++row) {
                for (PlyProperty& p : e.properties) {
                    if (p.isList) {
                        double n = readBinaryScalar(in, p.countType, swap);
                        int cnt = (int)n;
                        auto& dst = p.list[row];
                        dst.resize(max(0, cnt));
                        for (int i = 0; i < cnt; ++i)
                            dst[i] = readBinaryScalar(in, p.valueType, swap);
                    } else {
                        p.scalar[row] = readBinaryScalar(in, p.valueType, swap);
                    }
                }
                if (!in) {
                    logError() << "[tcxPly] unexpected EOF in binary element '"
                               << e.name << "' at row " << row;
                    return false;
                }
            }
        }
    }

    logNotice() << "[tcxPly] loaded " << path << " (" << getNumVertices()
                << " verts, " << getNumFaces() << " faces)";
    return true;
}

// =============================================================================
// Element / property lookup
// =============================================================================

const PlyElement* Ply::findElement(const string& name) const {
    for (const auto& e : elements_) if (e.name == name) return &e;
    return nullptr;
}

// =============================================================================
// Ply::toMesh
// =============================================================================

Mesh Ply::toMesh() const {
    Mesh mesh;
    const PlyElement* v = findElement("vertex");
    if (!v) {
        logWarning() << "[tcxPly] no 'vertex' element; returning empty mesh";
        return mesh;
    }

    auto col = [&](const char* n) -> const vector<double>* {
        int idx = v->findProperty(n);
        if (idx < 0 || v->properties[idx].isList) return nullptr;
        return &v->properties[idx].scalar;
    };

    const vector<double>* px = col("x");
    const vector<double>* py = col("y");
    const vector<double>* pz = col("z");
    for (size_t i = 0; i < v->count; ++i) {
        float x = px ? (float)(*px)[i] : 0.0f;
        float y = py ? (float)(*py)[i] : 0.0f;
        float z = pz ? (float)(*pz)[i] : 0.0f;
        mesh.addVertex(x, y, z);
    }

    const vector<double>* nx = col("nx");
    const vector<double>* ny = col("ny");
    const vector<double>* nz = col("nz");
    if (nx && ny && nz) {
        for (size_t i = 0; i < v->count; ++i)
            mesh.addNormal((float)(*nx)[i], (float)(*ny)[i], (float)(*nz)[i]);
    }

    // Color: integer red/green/blue are 0-255; float variants are already 0-1.
    int rIdx = v->findProperty("red");
    int gIdx = v->findProperty("green");
    int bIdx = v->findProperty("blue");
    if (rIdx >= 0 && gIdx >= 0 && bIdx >= 0) {
        const auto& rp = v->properties[rIdx];
        const auto& gp = v->properties[gIdx];
        const auto& bp = v->properties[bIdx];
        int aIdx = v->findProperty("alpha");
        const vector<double>* ap = (aIdx >= 0 && !v->properties[aIdx].isList)
                                       ? &v->properties[aIdx].scalar : nullptr;
        float rs = plyTypeIsFloat(rp.valueType) ? 1.0f : (1.0f / 255.0f);
        float gs = plyTypeIsFloat(gp.valueType) ? 1.0f : (1.0f / 255.0f);
        float bs = plyTypeIsFloat(bp.valueType) ? 1.0f : (1.0f / 255.0f);
        float as = (ap && aIdx >= 0 && plyTypeIsFloat(v->properties[aIdx].valueType))
                       ? 1.0f : (1.0f / 255.0f);
        for (size_t i = 0; i < v->count; ++i) {
            float a = ap ? (float)(*ap)[i] * as : 1.0f;
            mesh.addColor(Color{(float)rp.scalar[i] * rs, (float)gp.scalar[i] * gs,
                                (float)bp.scalar[i] * bs, a});
        }
    }

    // Texcoords: accept the three common spellings.
    const vector<double>* tu = col("s"); const vector<double>* tv = col("t");
    if (!(tu && tv)) { tu = col("u");          tv = col("v"); }
    if (!(tu && tv)) { tu = col("texture_u");  tv = col("texture_v"); }
    if (tu && tv) {
        for (size_t i = 0; i < v->count; ++i)
            mesh.addTexCoord((float)(*tu)[i], (float)(*tv)[i]);
    }

    // Faces -> triangle indices (fan-triangulate polygons). Otherwise points.
    const PlyElement* f = findElement("face");
    const PlyProperty* faceList = nullptr;
    if (f) {
        int fi = f->findProperty("vertex_indices");
        if (fi < 0) fi = f->findProperty("vertex_index");
        if (fi < 0) {  // fall back to the first list property
            for (size_t i = 0; i < f->properties.size(); ++i)
                if (f->properties[i].isList) { fi = (int)i; break; }
        }
        if (fi >= 0) faceList = &f->properties[fi];
    }

    if (faceList) {
        mesh.setMode(PrimitiveMode::Triangles);
        for (const auto& poly : faceList->list) {
            if (poly.size() < 3) continue;
            for (size_t k = 1; k + 1 < poly.size(); ++k) {
                mesh.addTriangle((unsigned)poly[0], (unsigned)poly[k], (unsigned)poly[k + 1]);
            }
        }
    } else {
        mesh.setMode(PrimitiveMode::Points);
    }

    return mesh;
}

// =============================================================================
// Ply::setMesh
// =============================================================================

Ply& Ply::setMesh(const Mesh& mesh) {
    elements_.clear();

    const auto& verts   = mesh.getVertices();
    const auto& normals = mesh.getNormals();
    const auto& colors  = mesh.getColors();
    const auto& uvs     = mesh.getTexCoords();
    const size_t nv = verts.size();

    PlyElement v;
    v.name = "vertex";
    v.count = nv;

    // Build each column fully, then push. (Don't hold a reference into
    // v.properties across further push_backs — the vector can reallocate.)
    auto addScalar = [&](const char* name, PlyType type, auto valueAt) {
        PlyProperty p;
        p.name = name;
        p.valueType = type;
        p.scalar.resize(nv);
        for (size_t i = 0; i < nv; ++i) p.scalar[i] = valueAt(i);
        v.properties.push_back(std::move(p));
    };

    addScalar("x", PlyType::Float32, [&](size_t i) { return verts[i].x; });
    addScalar("y", PlyType::Float32, [&](size_t i) { return verts[i].y; });
    addScalar("z", PlyType::Float32, [&](size_t i) { return verts[i].z; });

    if (mesh.hasNormals() && normals.size() >= nv) {
        addScalar("nx", PlyType::Float32, [&](size_t i) { return normals[i].x; });
        addScalar("ny", PlyType::Float32, [&](size_t i) { return normals[i].y; });
        addScalar("nz", PlyType::Float32, [&](size_t i) { return normals[i].z; });
    }

    if (mesh.hasColors() && colors.size() >= nv) {
        auto to255 = [](float c) { return (double)(std::clamp(c, 0.0f, 1.0f) * 255.0f); };
        addScalar("red",   PlyType::Uint8, [&](size_t i) { return to255(colors[i].r); });
        addScalar("green", PlyType::Uint8, [&](size_t i) { return to255(colors[i].g); });
        addScalar("blue",  PlyType::Uint8, [&](size_t i) { return to255(colors[i].b); });
        addScalar("alpha", PlyType::Uint8, [&](size_t i) { return to255(colors[i].a); });
    }

    if (mesh.hasTexCoords() && uvs.size() >= nv) {
        addScalar("s", PlyType::Float32, [&](size_t i) { return uvs[i].x; });
        addScalar("t", PlyType::Float32, [&](size_t i) { return uvs[i].y; });
    }

    elements_.push_back(std::move(v));

    // Faces: from indices when present, else from a triangle soup. A point
    // cloud (Points mode, or non-triangle without indices) writes no faces.
    vector<vector<double>> faces;
    const auto& indices = mesh.getIndices();
    if (mesh.hasIndices()) {
        for (size_t i = 0; i + 2 < indices.size(); i += 3)
            faces.push_back({(double)indices[i], (double)indices[i + 1], (double)indices[i + 2]});
    } else if (mesh.getMode() == PrimitiveMode::Triangles && nv >= 3) {
        for (size_t i = 0; i + 2 < nv; i += 3)
            faces.push_back({(double)i, (double)(i + 1), (double)(i + 2)});
    }

    if (!faces.empty()) {
        PlyElement f;
        f.name = "face";
        f.count = faces.size();
        PlyProperty p;
        p.name = "vertex_indices";
        p.isList = true;
        p.countType = PlyType::Uint8;
        p.valueType = PlyType::Int32;
        p.list = std::move(faces);
        f.properties.push_back(std::move(p));
        elements_.push_back(std::move(f));
    }

    return *this;
}

// =============================================================================
// Ply::save
// =============================================================================

bool Ply::save(const string& path, PlyFormat format) const {
    if (format == PlyFormat::BinaryBigEndian) {
        logWarning() << "[tcxPly] big-endian write not supported; using little-endian";
        format = PlyFormat::BinaryLittleEndian;
    }

    string full = getDataPath(path);
    ofstream out(full, ios::binary);
    if (!out) {
        logError() << "[tcxPly] cannot write: " << full;
        return false;
    }

    // ---- header (always ASCII) ----
    out << "ply\n";
    out << "format " << (format == PlyFormat::Ascii ? "ascii" : "binary_little_endian")
        << " 1.0\n";
    for (const auto& c : comments_) out << "comment " << c << "\n";
    for (const auto& o : objInfo_)  out << "obj_info " << o << "\n";
    for (const auto& e : elements_) {
        out << "element " << e.name << " " << e.count << "\n";
        for (const auto& p : e.properties) {
            if (p.isList)
                out << "property list " << plyTypeName(p.countType) << " "
                    << plyTypeName(p.valueType) << " " << p.name << "\n";
            else
                out << "property " << plyTypeName(p.valueType) << " " << p.name << "\n";
        }
    }
    out << "end_header\n";

    // ---- data ----
    if (format == PlyFormat::Ascii) {
        for (const auto& e : elements_) {
            for (size_t row = 0; row < e.count; ++row) {
                bool first = true;
                for (const auto& p : e.properties) {
                    if (p.isList) {
                        const auto& l = p.list[row];
                        if (!first) out << ' ';
                        writeAsciiScalar(out, p.countType, (double)l.size());
                        for (double val : l) { out << ' '; writeAsciiScalar(out, p.valueType, val); }
                    } else {
                        if (!first) out << ' ';
                        writeAsciiScalar(out, p.valueType, p.scalar[row]);
                    }
                    first = false;
                }
                out << "\n";
            }
        }
    } else {
        for (const auto& e : elements_) {
            for (size_t row = 0; row < e.count; ++row) {
                for (const auto& p : e.properties) {
                    if (p.isList) {
                        const auto& l = p.list[row];
                        writeBinaryScalarLE(out, p.countType, (double)l.size());
                        for (double val : l) writeBinaryScalarLE(out, p.valueType, val);
                    } else {
                        writeBinaryScalarLE(out, p.valueType, p.scalar[row]);
                    }
                }
            }
        }
    }

    if (!out) { logError() << "[tcxPly] write error: " << full; return false; }
    logNotice() << "[tcxPly] saved " << path;
    return true;
}

// =============================================================================
// Property accessors & queries
// =============================================================================

// getVertexProperty<T>() / getFaceProperty<T>() are defined inline in the
// header (they are templates). Here we only provide the property-list query.

static vector<PlyPropertyInfo> propertyInfos(const PlyElement* e) {
    vector<PlyPropertyInfo> infos;
    if (e) for (const auto& p : e->properties) infos.push_back({p.name, p.valueType, p.isList});
    return infos;
}

vector<PlyPropertyInfo> Ply::getVertexProperties() const { return propertyInfos(findElement("vertex")); }
vector<PlyPropertyInfo> Ply::getFaceProperties()   const { return propertyInfos(findElement("face")); }

size_t Ply::getNumVertices() const {
    const PlyElement* v = findElement("vertex");
    return v ? v->count : 0;
}

size_t Ply::getNumFaces() const {
    const PlyElement* f = findElement("face");
    return f ? f->count : 0;
}

BoundingBox Ply::getBoundingBox() const {
    BoundingBox bb;
    const PlyElement* v = findElement("vertex");
    if (!v) return bb;
    int xi = v->findProperty("x");
    int yi = v->findProperty("y");
    int zi = v->findProperty("z");
    if (xi < 0 || yi < 0 || zi < 0 || v->count == 0) return bb;

    const auto& xs = v->properties[xi].scalar;
    const auto& ys = v->properties[yi].scalar;
    const auto& zs = v->properties[zi].scalar;
    float lo = std::numeric_limits<float>::max();
    float hi = -std::numeric_limits<float>::max();
    bb.min = Vec3{lo, lo, lo};
    bb.max = Vec3{hi, hi, hi};
    for (size_t i = 0; i < v->count; ++i) {
        Vec3 p{(float)xs[i], (float)ys[i], (float)zs[i]};
        bb.min.x = std::min(bb.min.x, p.x); bb.min.y = std::min(bb.min.y, p.y); bb.min.z = std::min(bb.min.z, p.z);
        bb.max.x = std::max(bb.max.x, p.x); bb.max.y = std::max(bb.max.y, p.y); bb.max.z = std::max(bb.max.z, p.z);
    }
    bb.empty = false;
    return bb;
}

} // namespace tcx
