#include "StrataProjectLayers.h"

#include <QDir>
#include <QFileInfo>
#include <QStringList>
#include <QXmlStreamReader>
#include <memory>

#include <cpl_vsi.h>
#include <gdal_priv.h>
#include <ogrsf_frmts.h>

namespace strata3d {

namespace {

thread_local QStringList g_diag;  // 못 읽은 사유(오류 문장에 붙인다)

// .qgz(zip) 바이트에서 첫 .qgs 를 꺼낸다. GDAL 의 /vsimem/ + /vsizip/ 를 쓴다(임시 파일 없음).
QByteArray qgsFromZipBytes(const QByteArray& zip) {
  const QByteArray mem = QByteArray("/vsimem/strata3d_project_") + QByteArray::number(quintptr(&zip)) + ".zip";  // /vsizip/ 는 .zip 확장자로 zip 을 알아본다(.qgz 는 못 알아봄)
  VSILFILE* f = VSIFileFromMemBuffer(mem.constData(), reinterpret_cast<GByte*>(const_cast<char*>(zip.constData())), vsi_l_offset(zip.size()), FALSE);
  if (!f) { g_diag << QStringLiteral("메모리 파일 실패"); return {}; }
  VSIFCloseL(f);
  QByteArray out;
  const QByteArray dir = "/vsizip/" + mem;
  char** list = VSIReadDir(dir.constData());
  if (!list) g_diag << QStringLiteral("zip 목록을 읽지 못함(%1 바이트)").arg(zip.size());
  for (int i = 0; list && list[i]; ++i) {
    const QByteArray name(list[i]);
    if (!name.endsWith(".qgs")) continue;
    VSILFILE* q = VSIFOpenL((dir + "/" + name).constData(), "rb");
    if (!q) continue;
    VSIFSeekL(q, 0, SEEK_END);
    const vsi_l_offset n = VSIFTellL(q);
    VSIFSeekL(q, 0, SEEK_SET);
    out.resize(qsizetype(n));
    VSIFReadL(out.data(), 1, size_t(n), q);
    VSIFCloseL(q);
    break;
  }
  if (out.isEmpty() && list) g_diag << QStringLiteral("zip 안에 .qgs 없음");
  CSLDestroy(list);
  VSIUnlink(mem.constData());
  return out;
}

QByteArray readFileBytes(const QString& path) {
  VSILFILE* f = VSIFOpenL(path.toUtf8().constData(), "rb");
  if (!f) return {};
  VSIFSeekL(f, 0, SEEK_END);
  const vsi_l_offset n = VSIFTellL(f);
  VSIFSeekL(f, 0, SEEK_SET);
  QByteArray b(qsizetype(n), Qt::Uninitialized);
  VSIFReadL(b.data(), 1, size_t(n), f);
  VSIFCloseL(f);
  return b;
}

// 조사 .gpkg 의 qgis_projects 표(QGIS 의 GeoPackage 프로젝트 저장소). content 는 .qgz 바이트다.
QByteArray qgsFromGpkg(const QString& gpkg) {
  GDALAllRegister();
  const std::unique_ptr<GDALDataset> ds(GDALDataset::Open(gpkg.toUtf8().constData(), GDAL_OF_VECTOR | GDAL_OF_READONLY));
  if (!ds) { g_diag << QStringLiteral("gpkg 열기 실패"); return {}; }
  // qgis_projects 는 OGR 레이어가 아닌 일반 표라 GPKG 원래 SQL 로 묻는다(SQLITE 방언은 OGR 레이어만 보인다)
  OGRLayer* t = ds->ExecuteSQL("SELECT content FROM qgis_projects LIMIT 1", nullptr, nullptr);
  if (!t) { g_diag << QStringLiteral("qgis_projects 표 없음"); return {}; }
  QByteArray zip;
  if (OGRFeature* f = t->GetNextFeature()) {
    int n = 0;
    // 열 형식이 글자(TEXT, 16진수)면 글자로, 이진(BLOB)이면 바이트로 읽는다
    if (f->GetFieldDefnRef(0)->GetType() == OFTBinary) {
      const GByte* b = f->GetFieldAsBinary(0, &n);
      if (b && n > 0) zip = QByteArray(reinterpret_cast<const char*>(b), n);
    } else {
      zip = QByteArray(f->GetFieldAsString(0));
    }
    OGRFeature::DestroyFeature(f);
  }
  ds->ReleaseResultSet(t);
  if (zip.isEmpty()) g_diag << QStringLiteral("qgis_projects.content 비어 있음");
  // QGIS 는 content 를 .qgz 바이트의 16진수 글자로 적는다(경주시.gpkg 에서 확인: "504B0304…")
  if (zip.startsWith("504B") || zip.startsWith("504b")) zip = QByteArray::fromHex(zip);
  if (zip.startsWith("PK")) return qgsFromZipBytes(zip);
  return zip;  // 압축하지 않은 XML 로 저장된 경우
}

void parseQgs(const QByteArray& xml, StrataProject* out) {
  QXmlStreamReader r(xml);
  QStringList stack;
  StrataLayerRef cur;
  QStringList propScope;  // 지금 열린 <properties name=…> 묶음들(바깥 → 안)
  bool inLayer = false, inCustom = false;
  while (!r.atEnd()) {
    const auto tok = r.readNext();
    if (tok == QXmlStreamReader::StartElement) {
      const QString n = r.name().toString();
      stack << n;
      if (n == QLatin1String("maplayer")) {
        inLayer = true;
        cur = StrataLayerRef{};
      } else if (inLayer && n == QLatin1String("customproperties")) {
        inCustom = true;
      } else if (inLayer && inCustom && (n == QLatin1String("Option") || n == QLatin1String("property"))) {
        const auto a = r.attributes();
        const QString key = a.hasAttribute(QLatin1String("name")) ? a.value(QLatin1String("name")).toString() : a.value(QLatin1String("key")).toString();
        if (!key.isEmpty() && a.hasAttribute(QLatin1String("value"))) cur.props.insert(key, a.value(QLatin1String("value")).toString());
      } else if (inLayer && !inCustom && stack.size() >= 2 && stack[stack.size() - 2] == QLatin1String("maplayer")) {
        if (n == QLatin1String("datasource")) cur.source = r.readElementText(), stack.removeLast();
        else if (n == QLatin1String("layername")) cur.name = r.readElementText(), stack.removeLast();
        else if (n == QLatin1String("provider")) cur.provider = r.readElementText(), stack.removeLast();
      } else if (!inLayer && n == QLatin1String("properties")) {
        // 프로젝트 속성: <properties name="ka_hgis"><properties name="survey_dir" type="QString">경로</properties>
        const QString pname = r.attributes().value(QLatin1String("name")).toString();
        if (pname == QLatin1String("survey_dir") && !propScope.isEmpty() && propScope.last() == QLatin1String("ka_hgis")) {
          out->surveyDir = r.readElementText();
          stack.removeLast();
        } else {
          propScope << pname;
        }
      }
    } else if (tok == QXmlStreamReader::EndElement) {
      const QString n = r.name().toString();
      if (!stack.isEmpty()) stack.removeLast();
      if (n == QLatin1String("customproperties")) inCustom = false;
      if (!inLayer && n == QLatin1String("properties") && !propScope.isEmpty()) propScope.removeLast();
      if (n == QLatin1String("maplayer") && inLayer) {
        inLayer = false;
        if (!cur.source.isEmpty()) out->layers.push_back(cur);
      }
    }
  }
}

// datasource 문자열 → 파일 경로와 |layername=
void splitSource(StrataLayerRef* l, const QString& gpkgDir, const QString& surveyDir) {
  const QStringList parts = l->source.split(QLatin1Char('|'));
  QString path = parts.value(0).trimmed();
  for (const QString& p : parts.mid(1))
    if (p.startsWith(QLatin1String("layername="))) l->subLayer = p.mid(10);
  if (path.startsWith(QLatin1String("./")) || QFileInfo(path).isRelative()) path = QDir(gpkgDir).filePath(path);
  path = QDir::cleanPath(path);
  // 조사 폴더가 옮겨졌으면 survey_dir 앞부분을 지금 폴더로 바꾼다(hgis SurveyBundle.cpp:255 와 같은 생각)
  if (!QFileInfo::exists(path) && !surveyDir.isEmpty()) {
    const QString old = QDir::cleanPath(surveyDir);
    if (path.startsWith(old, Qt::CaseInsensitive)) path = QDir::cleanPath(gpkgDir + path.mid(old.size()));
  }
  l->path = path;
  l->exists = QFileInfo::exists(path);
}

}  // namespace

bool readStrataProject(const QString& gpkgPath, StrataProject* out, QString* err) {
  *out = StrataProject{};
  g_diag.clear();
  out->gpkgPath = gpkgPath;
  const QFileInfo fi(gpkgPath);
  QByteArray xml = qgsFromGpkg(gpkgPath);
  if (!xml.isEmpty()) out->projectFrom = QStringLiteral("gpkg:qgis_projects");
  if (xml.isEmpty()) {
    const QString qgz = fi.dir().filePath(fi.completeBaseName() + QStringLiteral(".qgz"));
    if (QFileInfo::exists(qgz)) xml = qgsFromZipBytes(readFileBytes(qgz)), out->projectFrom = QStringLiteral("qgz");
  }
  if (xml.isEmpty()) {
    const QString qgs = fi.dir().filePath(fi.completeBaseName() + QStringLiteral(".qgs"));
    if (QFileInfo::exists(qgs)) xml = readFileBytes(qgs), out->projectFrom = QStringLiteral("qgs");
  }
  if (xml.isEmpty()) {
    if (err) *err = QStringLiteral("조사 프로젝트 기록을 읽지 못했습니다(.gpkg 안 qgis_projects, 옆의 .qgz/.qgs): %1 [%2]").arg(gpkgPath, g_diag.join(QStringLiteral("; ")));
    return false;
  }
  parseQgs(xml, out);
  const QString gpkgDir = fi.absolutePath();
  for (StrataLayerRef& l : out->layers) splitSource(&l, gpkgDir, out->surveyDir);
  if (out->surveyDir.isEmpty()) out->surveyDir = gpkgDir;
  return true;
}

std::vector<const StrataLayerRef*> layersWithProp(const StrataProject& p, const QString& key) {
  std::vector<const StrataLayerRef*> v;
  for (const StrataLayerRef& l : p.layers)
    if (l.props.contains(key)) v.push_back(&l);
  return v;
}

std::vector<LocalVectorSource> heritageSourcesOf(const StrataProject& p, QStringList* missingOut) {
  std::vector<LocalVectorSource> out;
  for (const StrataLayerRef* l : layersWithProp(p, QStringLiteral("ka_hgis/heritage_dataset"))) {
    if (!l->exists) {
      if (missingOut) *missingOut << l->name + QStringLiteral(" (") + l->path + QLatin1Char(')');
      continue;
    }
    out.push_back({l->path, l->subLayer, l->name, l->props.value(QStringLiteral("ka_hgis/label_field"))});
  }
  return out;
}

}  // namespace strata3d
