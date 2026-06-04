# tcxPly

PLY（Stanford Polygon File Format）ファイルを `tc::Mesh` として読み書きする TrussC アドオン。
メッシュも点群も、ASCII / バイナリ（リトル・ビッグエンディアン）も読める。

```cpp
#include <tcxPly.h>
using namespace tcx;

// 読む（ワンライナー）
Mesh mesh = loadPly("bunny.ply");
mesh.draw();

// 書く
savePly("out.ply", mesh);                          // ASCII
savePly("out.ply", mesh, PlyFormat::BinaryLittleEndian);
```

## なにができる

- **読み込み**: `ascii` / `binary_little_endian` / `binary_big_endian` をヘッダから自動判定
- **メッシュ ↔ 点群**: 面（`face`）があればインデックス付きメッシュ、無ければ `PrimitiveMode::Points` の点群になる
- **標準属性 → Mesh**: 位置 `x,y,z` / 法線 `nx,ny,nz` / 頂点色 `red,green,blue[,alpha]`（uchar 0-255 は 0-1 に変換）/ テクスチャ座標 `s,t`・`u,v`・`texture_u,texture_v`
- **ポリゴンの三角形分割**: 4角形以上の面はファン分割して三角形に
- **非標準プロパティの保持**: `quality` や `intensity` のような頂点ごと／面ごとの追加スカラーを保持し、名前で取得できる
- **メタデータの保持**: `format` / `comment` / `obj_info` を保持し、`load` → `save` で復元
- **書き出し**: `ascii` と `binary_little_endian`（`PlyFormat` で切替）

## API

`loadPly` / `savePly` のフリー関数は「とりあえず Mesh が欲しい」用。メタデータや追加プロパティを
触りたいときは `Ply` クラスを使う。

```cpp
Ply ply;
ply.load("scan.ply");

Mesh mesh      = ply.toMesh();
BoundingBox bb = ply.getBoundingBox();   // bb.center(), bb.size()

// プロパティは「型付き」で取得する。テンプレート引数の型が PLY の型と
// 一致しないと（変換せず）空 vector を返す。標準/非標準で区別はない。
auto crv = ply.getVertexProperty<float>("curvature");    // float プロパティ
auto red = ply.getVertexProperty<uint8_t>("red");        // uchar プロパティ
// どんなプロパティがあるか（名前＋型）を一覧
for (auto& p : ply.getVertexProperties())
    logNotice() << p.name << " : " << plyTypeName(p.type);

for (auto& c : ply.getComments())  logNotice() << c;
for (auto& o : ply.getObjInfo())   logNotice() << o;

// Mesh から書き出し（メタデータも付けられる）
Ply out;
out.setMesh(mesh);
out.addComment("exported from MyApp");
out.save("out.ply", PlyFormat::BinaryLittleEndian);
```

主なメソッド:

| メソッド | 説明 |
|---|---|
| `load(path)` / `save(path, format)` | 読み書き（`path` は `getDataPath` で解決、絶対パスはそのまま） |
| `toMesh()` / `setMesh(mesh)` | `tc::Mesh` との相互変換 |
| `getVertexProperty<T>(name)` / `getFaceProperty<T>(name)` | スカラー列を**型付き**で取得（`T` が一致しなければ空、変換しない。`T` 既定は `float`） |
| `getVertexProperties()` / `getFaceProperties()` | プロパティの一覧（`PlyPropertyInfo{name, type, isList}`） |
| `getComments()` / `getObjInfo()` / `addComment()` / `addObjInfo()` | ファイルメタデータ |
| `getFormat()` / `setFormat()` | フォーマット |
| `getBoundingBox()` | 頂点の AABB（`BoundingBox{min, max, center(), size()}`） |
| `getNumVertices()` / `getNumFaces()` | 件数 |

## サンプル

```bash
cd addons/tcxPly/example-load     # 色付き点群を表示、.ply をD&Dで読み込み
trusscli update
trusscli run

cd ../example-save                # Mesh を生成 → 保存 → 読み戻して表示
trusscli update
trusscli run
```

`example-load` は起動時に **fragment.ply**（Redwood のリビングルーム実スキャンの
RGB色付き点群・パブリックドメイン）を表示する。同梱しているのは tcxPly 自身の
`save()` で 1/4 に間引き＋上下反転して書き出した派生版（curvature や camera 要素も
保持）。`.ply` をウィンドウに**ドラッグ＆ドロップ**すれば任意のファイルを読み込む
（Gaussian Splat の `f_dc_*` からの色復元付き）。メッシュをドロップした場合は
`[SPACE]` で **face → wireframe → dots** を切替できる。同梱データのライセンスは
[LICENSES.md](LICENSES.md) を参照。

![example-load の実行画面（fragment.ply の色付き点群表示）](docs/example-load.png)

## 制限

- 書き出しは ASCII とリトルエンディアンバイナリのみ（ビッグエンディアンは読み込みのみ対応）
- `toMesh()` は `vertex` / `face` 要素のみを解釈する。`edge` やカメラ要素など独自 element は
  `getElements()` から生データで触れるが Mesh には反映しない
- 値は内部的に `double` で保持するため、`int64` / `uint64` のような 64bit 整数プロパティは非対応

## License

MIT — see [LICENSES.md](LICENSES.md).
