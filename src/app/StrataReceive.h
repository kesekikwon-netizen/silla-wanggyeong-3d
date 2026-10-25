#pragma once

#include <QObject>
#include <QPointer>
#include <QString>
#include <functional>

#include <qgsrectangle.h>

class KaHeritageBrowser;
class HeritageRegionResolver;
class QgsTask;
class QMainWindow;
struct HeritageRegion;

// 「자료 받기」·「지형 위에 올리기」의 받기를 Strata 코드 그대로 3D 창에서 돌린다(설계도 v3 0b단계).
// 화면(지도 캔버스)이 없으므로 범위는 Strata 의 「화면 범위」 대신 조사구역 상자 + 5 km 로 정한다(설계도 7장).
// 받은 레이어는 Strata 처럼 조사 프로젝트(QgsProject::instance())의 「참조 지도」에 들어가고, 저장은 「저장」이 한다.
class StrataReceive : public QObject {
  Q_OBJECT
 public:
  enum class Kind { Heritage, Cadastral, Soil, Geology, River };

  // surveyPath: 지금 연 조사 .gpkg(비면 받지 않는다)
  StrataReceive(QMainWindow* window, std::function<QString()> surveyPath, QObject* parent = nullptr);
  ~StrataReceive() override;

  void fetchHeritage();
  void fetchCadastral();
  void fetchThematic(Kind kind);  // Soil · Geology · River
  bool busy() const { return bool(job_); }

  // 조사구역(ka_hgis/layer_key=survey_area)을 모두 합친 상자(작업 좌표계). 없으면 빈 상자.
  static QgsRectangle surveyBox();

 signals:
  // 레이어가 프로젝트에 들어왔다(3D 를 다시 그려야 한다). what = 사람이 읽는 이름
  void received(const QString& what);

 private:
  void ensureHeritageBrowser();
  void openHeritageBrowserFor(const HeritageRegion& region, const QString& reason = {});
  void saveAgreementReceipt(const QDateTime& when, const QString& terms);
  void warn(const QString& title, const QString& text);
  void say(const QString& message, int ms);

  QMainWindow* window_ = nullptr;
  std::function<QString()> surveyPath_;
  QPointer<KaHeritageBrowser> browser_;
  QPointer<HeritageRegionResolver> resolver_;
  QPointer<QgsTask> job_;
};
