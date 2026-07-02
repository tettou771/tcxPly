#pragma once

// =============================================================================
// tcxPly - read & write PLY (Stanford Polygon File Format) as a tc::Mesh.
//
//   #include <tcxPly.h>
//   using namespace tcx;
//
//   // Quick path — load a PLY straight into a Mesh:
//   Mesh mesh = loadPly("bunny.ply");
//   mesh.draw();
//
//   // Quick path — write a Mesh out:
//   savePly("out.ply", mesh);                         // ASCII
//   savePly("out.ply", mesh, PlyFormat::BinaryLittleEndian);
//
//   // Full path — keep metadata and non-standard per-vertex/per-face fields:
//   Ply ply;
//   ply.load("scan.ply");
//   Mesh m   = ply.toMesh();
//   auto crv = ply.getVertexProperty<float>("curvature");    // typed: float col
//   auto rgb = ply.getVertexProperty<uint8_t>("red");        // typed: uchar col
//   for (auto& p : ply.getVertexProperties())                // name + type
//       logNotice() << p.name << " : " << plyTypeName(p.type);
//   BoundingBox bb = ply.getBoundingBox();
//   for (auto& c : ply.getComments()) logNotice() << c;
//
// PLY is more than a triangle container: it carries a format (ascii / binary,
// either endianness), free-text metadata (comment / obj_info), and arbitrary
// per-element properties. `toMesh()` extracts the standard attributes; the
// rest stays reachable on the Ply object and survives a load -> save round trip.
// =============================================================================

#include <TrussC.h>

#include "tcxPlyTypes.h"

#include <string>
#include <vector>

namespace tcx::ply {

class Ply {
public:
    Ply() = default;

    // ---- file I/O ----------------------------------------------------------

    // Load a PLY from `path` (resolved via tc::getDataPath). Auto-detects
    // ascii / binary_little_endian / binary_big_endian from the header.
    // Returns false (and logs) on a malformed file, leaving the Ply empty.
    bool load(const std::string& path);

    // Write to `path`. `format` defaults to ASCII (most portable);
    // BinaryLittleEndian is the compact option. Returns false on I/O error.
    bool save(const std::string& path, PlyFormat format = PlyFormat::Ascii) const;

    // ---- Mesh bridge -------------------------------------------------------

    // Build a tc::Mesh from the standard vertex/face attributes:
    //   position (x,y,z), normal (nx,ny,nz), color (red,green,blue[,alpha]),
    //   texcoord (s,t | u,v | texture_u,texture_v), faces (vertex_indices).
    // Polygons with >3 sides are fan-triangulated. With no face element the
    // mesh is returned as PrimitiveMode::Points (a point cloud).
    tc::Mesh toMesh() const;

    // Replace the contents from a Mesh (for saving). Only attributes the mesh
    // actually has are written. A mesh with indices (or a triangle soup) gets
    // a face element; otherwise it is saved as a point cloud.
    Ply& setMesh(const tc::Mesh& mesh);

    // ---- file-level metadata (preserved across load -> save) ---------------

    PlyFormat                       getFormat()   const { return format_; }
    Ply&                            setFormat(PlyFormat f) { format_ = f; return *this; }
    const std::vector<std::string>& getComments() const { return comments_; }
    const std::vector<std::string>& getObjInfo()  const { return objInfo_; }
    Ply& addComment(const std::string& line) { comments_.push_back(line); return *this; }
    Ply& addObjInfo(const std::string& line) { objInfo_.push_back(line); return *this; }

    // ---- property columns (standard & non-standard, handled identically) ---

    // Fetch a scalar property column by name, typed. T must match the property's
    // on-disk PLY type exactly (no implicit conversion) — e.g. a `float`
    // property needs getVertexProperty<float>, a `uchar` needs <uint8_t>. T
    // defaults to float. Returns an empty vector and logs a warning when:
    //   - the element/property does not exist          ("has no property ...")
    //   - the property exists but is a different type   ("is <X>, not <Y>")
    //   - the property is a list, not a scalar
    // Standard fields (x, red, ...) are fetched the same way as custom ones.
    template <typename T = float>
    std::vector<T> getVertexProperty(const std::string& name) const {
        return getTypedColumn<T>("vertex", name);
    }
    template <typename T = float>
    std::vector<T> getFaceProperty(const std::string& name) const {
        return getTypedColumn<T>("face", name);
    }

    // Discover what an element holds: each property's name, type and list-ness.
    std::vector<PlyPropertyInfo> getVertexProperties() const;
    std::vector<PlyPropertyInfo> getFaceProperties() const;

    // ---- queries -----------------------------------------------------------

    BoundingBox getBoundingBox() const;
    size_t      getNumVertices() const;
    size_t      getNumFaces()    const;

    // Low-level access to the parsed structure (advanced use).
    const std::vector<PlyElement>& getElements() const { return elements_; }
    std::vector<PlyElement>&       getElements()       { return elements_; }
    const PlyElement* findElement(const std::string& name) const;

private:
    // Strict typed column fetch shared by getVertexProperty / getFaceProperty.
    // Values are stored internally as double but losslessly (every supported
    // on-disk type fits exactly), so when the type matches, casting back to T
    // reproduces the original values exactly.
    template <typename T>
    std::vector<T> getTypedColumn(const std::string& element,
                                  const std::string& property) const {
        const PlyElement* e = findElement(element);
        if (!e) {
            tc::logWarning() << "[tcxPly] no '" << element << "' element";
            return {};
        }
        int idx = e->findProperty(property);
        if (idx < 0) {
            tc::logWarning() << "[tcxPly] '" << element << "' has no property '"
                         << property << "'";
            return {};
        }
        const PlyProperty& p = e->properties[idx];
        if (p.isList) {
            tc::logWarning() << "[tcxPly] property '" << property
                         << "' is a list, not a scalar";
            return {};
        }
        PlyType want = plyTypeOf<T>();
        if (p.valueType != want) {
            tc::logWarning() << "[tcxPly] property '" << property << "' is "
                         << plyTypeName(p.valueType) << ", not "
                         << plyTypeName(want);
            return {};
        }
        std::vector<T> out(p.scalar.size());
        for (size_t i = 0; i < p.scalar.size(); ++i)
            out[i] = static_cast<T>(p.scalar[i]);
        return out;
    }

    PlyFormat                format_ = PlyFormat::Ascii;
    std::vector<std::string> comments_;
    std::vector<std::string> objInfo_;
    std::vector<PlyElement>  elements_;
};

// -----------------------------------------------------------------------------
// Free-function convenience wrappers — the common "I just want a Mesh" path.
// -----------------------------------------------------------------------------

inline tc::Mesh loadPly(const std::string& path) {
    Ply p;
    p.load(path);
    return p.toMesh();
}

inline bool savePly(const std::string& path, const tc::Mesh& mesh,
                    PlyFormat format = PlyFormat::Ascii) {
    Ply p;
    p.setMesh(mesh);
    return p.save(path, format);
}

} // namespace tcx::ply

// -----------------------------------------------------------------------------
// Backward compatibility. The canonical namespace is now `tcx::ply`. These
// silent aliases keep older code compiling: flat `tcx::Ply` and legacy
// `trussc::Ply`. DEPRECATED — removed in v1.0.0.
// (No [[deprecated]] attribute: under the usual `using namespace tc;` it would
//  warn on idiomatic unqualified use too. See tcxPly README for migration.)
// -----------------------------------------------------------------------------
namespace tcx {
    using ply::Ply;       // deprecated: remove at v1.0.0
    using ply::loadPly;   // deprecated: remove at v1.0.0
    using ply::savePly;   // deprecated: remove at v1.0.0
}
namespace trussc {
    using tcx::ply::Ply;       // deprecated: remove at v1.0.0
    using tcx::ply::loadPly;   // deprecated: remove at v1.0.0
    using tcx::ply::savePly;   // deprecated: remove at v1.0.0
}
