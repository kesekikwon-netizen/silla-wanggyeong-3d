// 0단계 위험 확인: Strata 코드(SurveyStorage::readEmbedded)로 경주 조사를 열어 레이어를 나열한다.
#include <QDir>
#include <QTextStream>

#include <qgsapplication.h>
#include <qgsmaplayer.h>
#include <qgsproject.h>

#include "core/SurveyStorage.h"

int main(int argc, char** argv) {
  QgsApplication app(argc, argv, false);
  QgsApplication::setPrefixPath(QStringLiteral("C:/OSGeo4W/apps/qgis-dev"), true);
  QgsApplication::initQgis();
  QTextStream out(stdout);
  out.setEncoding(QStringConverter::Utf8);
  const QString gpkg = argc > 1 ? QString::fromLocal8Bit(argv[1])
                                : QDir::home().filePath(QString::fromUtf16(u"Documents/경주시/경주시.gpkg"));
  QgsProject project;
  bool crashed = false;
  QString err;
  const bool ok = SurveyStorage::readEmbedded(&project, gpkg, &crashed, &err, false);
  out << "readEmbedded=" << ok << " crashed=" << crashed << " err=" << err << "\n";
  int n = 0;
  for (QgsMapLayer* l : project.mapLayers()) {
    out << "layer: " << l->name() << " valid=" << l->isValid()
        << " heritage=" << l->customProperty(QStringLiteral("ka_hgis/heritage_dataset")).toString() << "\n";
    ++n;
  }
  out << "layers=" << n << "\n";
  out.flush();
  QgsApplication::exitQgis();
  return ok ? 0 : 1;
}
