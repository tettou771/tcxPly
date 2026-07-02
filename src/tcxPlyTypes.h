#pragma once

// =============================================================================
// tcxPly types - low-level PLY building blocks.
//
// These describe the on-disk structure of a PLY file generically: a file is a
// sequence of "elements" (vertex, face, edge, ...) and each element has a list
// of "properties" (x, y, z, red, vertex_indices, ...). Keeping this generic is
// what lets tcxPly round-trip non-standard properties (quality, intensity, ...)
// and preserve metadata, instead of only understanding the handful of fields
// that map onto tc::Mesh.
// =============================================================================

#include <TrussC.h>

#include <cstdint>
#include <string>
#include <vector>

namespace tcx::ply {

// Output format. Reading auto-detects any of these from the header; writing
// supports Ascii and BinaryLittleEndian (the two formats in practical use).
enum class PlyFormat {
    Ascii,
    BinaryLittleEndian,
    BinaryBigEndian,
};

// PLY scalar property types. The spec allows both "char/uchar/short/..." and
// the "int8/uint8/..." spellings; both parse to the same enum here.
enum class PlyType {
    Int8, Uint8, Int16, Uint16, Int32, Uint32, Float32, Float64,
    Invalid,
};

// Bytes occupied by one value of the given type.
inline int plyTypeSize(PlyType t) {
    switch (t) {
        case PlyType::Int8:
        case PlyType::Uint8:   return 1;
        case PlyType::Int16:
        case PlyType::Uint16:  return 2;
        case PlyType::Int32:
        case PlyType::Uint32:
        case PlyType::Float32: return 4;
        case PlyType::Float64: return 8;
        default:               return 0;
    }
}

inline bool plyTypeIsFloat(PlyType t) {
    return t == PlyType::Float32 || t == PlyType::Float64;
}

// PLY type-name spelling (the canonical "char/uchar/short/..." form).
inline const char* plyTypeName(PlyType t) {
    switch (t) {
        case PlyType::Int8:    return "char";
        case PlyType::Uint8:   return "uchar";
        case PlyType::Int16:   return "short";
        case PlyType::Uint16:  return "ushort";
        case PlyType::Int32:   return "int";
        case PlyType::Uint32:  return "uint";
        case PlyType::Float32: return "float";
        case PlyType::Float64: return "double";
        default:               return "invalid";
    }
}

// Compile-time map from a C++ scalar type to its PlyType. Used by the typed
// getters: getVertexProperty<T>() only returns a column whose on-disk type is
// exactly plyTypeOf<T>() — no implicit conversion. Use the fixed-width types
// (uint8_t, int32_t, float, double, ...).
template <typename> inline constexpr bool ply_always_false_v = false;
template <typename T> constexpr PlyType plyTypeOf() {
    static_assert(ply_always_false_v<T>,
                  "tcxPly: unsupported scalar type for getVertexProperty<T>() / "
                  "getFaceProperty<T>(); use int8_t/uint8_t/int16_t/uint16_t/"
                  "int32_t/uint32_t/float/double");
    return PlyType::Invalid;
}
template <> constexpr PlyType plyTypeOf<int8_t>()   { return PlyType::Int8; }
template <> constexpr PlyType plyTypeOf<uint8_t>()  { return PlyType::Uint8; }
template <> constexpr PlyType plyTypeOf<int16_t>()  { return PlyType::Int16; }
template <> constexpr PlyType plyTypeOf<uint16_t>() { return PlyType::Uint16; }
template <> constexpr PlyType plyTypeOf<int32_t>()  { return PlyType::Int32; }
template <> constexpr PlyType plyTypeOf<uint32_t>() { return PlyType::Uint32; }
template <> constexpr PlyType plyTypeOf<float>()    { return PlyType::Float32; }
template <> constexpr PlyType plyTypeOf<double>()   { return PlyType::Float64; }

// One property of an element. Values are kept as double regardless of the
// on-disk type (double exactly represents every PLY integer up to 32 bits and
// all float32); `valueType` remembers the real type so it can be written back.
//
// A property is either scalar (one value per element row, in `scalar`) or a
// list (variable-length per row, in `list`) — never both.
struct PlyProperty {
    std::string name;
    bool        isList    = false;
    PlyType     countType = PlyType::Uint8;    // list length type (list only)
    PlyType     valueType = PlyType::Float32;  // value type (scalar & list)

    std::vector<double>              scalar;   // size == element count (scalar)
    std::vector<std::vector<double>> list;     // size == element count (list)
};

// A named group of rows (e.g. "vertex" with 1024 rows), column-oriented: each
// PlyProperty carries its own full column.
struct PlyElement {
    std::string              name;
    size_t                   count = 0;
    std::vector<PlyProperty> properties;

    int findProperty(const std::string& n) const {
        for (size_t i = 0; i < properties.size(); ++i) {
            if (properties[i].name == n) return static_cast<int>(i);
        }
        return -1;
    }
};

// Name + type description of one property, returned by getVertexProperties() /
// getFaceProperties() so callers can discover what a file holds.
struct PlyPropertyInfo {
    std::string name;
    PlyType     type;    // for a list property, the element (value) type
    bool        isList;
};

// Axis-aligned bounding box of a point set. `empty` is true when computed from
// zero vertices (min/max are then left at the origin).
struct BoundingBox {
    tc::Vec3 min;
    tc::Vec3 max;
    bool     empty = true;

    tc::Vec3 center() const { return (min + max) * 0.5f; }
    tc::Vec3 size()   const { return max - min; }
};

} // namespace tcx::ply

// -----------------------------------------------------------------------------
// Backward compatibility. The canonical namespace is now `tcx::ply`. These
// silent aliases keep older code compiling: flat `tcx::PlyType` and legacy
// `trussc::PlyType`. DEPRECATED — removed in v1.0.0.
// (No [[deprecated]] attribute: under the usual `using namespace tc;` it would
//  warn on idiomatic unqualified use too. See tcxPly README for migration.)
// -----------------------------------------------------------------------------
namespace tcx {
    using ply::PlyFormat;        // deprecated: remove at v1.0.0
    using ply::PlyType;          // deprecated: remove at v1.0.0
    using ply::plyTypeSize;      // deprecated: remove at v1.0.0
    using ply::plyTypeIsFloat;   // deprecated: remove at v1.0.0
    using ply::plyTypeName;      // deprecated: remove at v1.0.0
    using ply::plyTypeOf;        // deprecated: remove at v1.0.0
    using ply::PlyProperty;      // deprecated: remove at v1.0.0
    using ply::PlyElement;       // deprecated: remove at v1.0.0
    using ply::PlyPropertyInfo;  // deprecated: remove at v1.0.0
    using ply::BoundingBox;      // deprecated: remove at v1.0.0
}
namespace trussc {
    using tcx::ply::PlyFormat;        // deprecated: remove at v1.0.0
    using tcx::ply::PlyType;          // deprecated: remove at v1.0.0
    using tcx::ply::plyTypeSize;      // deprecated: remove at v1.0.0
    using tcx::ply::plyTypeIsFloat;   // deprecated: remove at v1.0.0
    using tcx::ply::plyTypeName;      // deprecated: remove at v1.0.0
    using tcx::ply::plyTypeOf;        // deprecated: remove at v1.0.0
    using tcx::ply::PlyProperty;      // deprecated: remove at v1.0.0
    using tcx::ply::PlyElement;       // deprecated: remove at v1.0.0
    using tcx::ply::PlyPropertyInfo;  // deprecated: remove at v1.0.0
    using tcx::ply::BoundingBox;      // deprecated: remove at v1.0.0
}
