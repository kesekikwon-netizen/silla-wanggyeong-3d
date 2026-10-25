#include "StrataReceive.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMainWindow>
#include <QMessageBox>
#include <QProgressDialog>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTextStream>
#include <QWidget>

#include <qgsapplication.h>
#include <qgscoordinatetransform.h>
#include <qgsfeatureiterator.h>
#include <qgsgeometry.h>
#include <qgsproject.h>
#include <qgstaskmanager.h>
#include <qgsvectorlayer.h>

#include "KaDownloadUi.h"
#include "KaHeritageBrowser.h"
#include "KaHeritageSetupDialogs.h"
#include "KaReferenceDownloadJob.h"
#include "core/CadastralImport.h"
#include "core/CadastralPortal.h"
#include "core/GeologyMapService.h"
#include "core/HeritageImport.h"
#include "core/HeritageIntranetSettings.h"
#include "core/HeritageRegionResolver.h"
#include "core/HeritageStyle.h"
#include "core/LayerOps.h"
#include "core/ReferenceStorage.h"
#include "core/RiverMapService.h"
#include "core/SoilMapService.h"
#include "core/VworldSettings.h"

namespace {
constexpr double kScopeBufferM = 5000.0;  // Strata 의 주변유적·지적과 같은 5 km

// 조사구역 도형을 모두 합친 것(작업 좌표계 = 프로젝트 좌표계)
QgsGeometry surveyUnion() {
  QgsProject* project = QgsProject::instance();
  QVector<QgsGeometry> parts;
  for (QgsMapLayer* l : project->mapLayers()) {
    auto* v = qobject_cast<QgsVectorLayer*>(l);
    if (!v || LayerOps::layerKeyOf(v) != QLatin1String("survey_area")) continue;
    QgsFeatureRequest req;
    req.setNoAttributes();
    const QgsCoordinateTransform tr(v->crs(), project->crs(), project->transformContext());
    QgsFeature f;
    QgsFeatureIterator it = v->getFeatures(req);
    while (it.nextFeature(f)) {
      QgsGeometry g = f.geometry();
      if (g.isNull() || g.isEmpty()) continue;
      try {
        g.transform(tr);
      } catch (const QgsCsException&) {
        continue;
      }
      parts.append(g);
    }
  }
  return parts.isEmpty() ? QgsGeometry() : QgsGeometry::unaryUnion(parts);
}
}  // namespace

StrataReceive::StrataReceive(QMainWindow* window, std::function<QString()> surveyPath, QObject* parent)
    : QObject(parent), window_(window), surveyPath_(std::move(surveyPath)) {}

void StrataReceive::say(const QString& message, int ms) {
  // 창의 상태줄은 나중에 바뀔 수 있어(setStatusBar) 그때그때 묻는다
  if (window_ && window_->statusBar()) window_->statusBar()->showMessage(message, ms);
}

StrataReceive::~StrataReceive() {
  if (job_) job_->cancel();
}

QgsRectangle StrataReceive::surveyBox() {
  const QgsGeometry g = surveyUnion();
  return g.isNull() ? QgsRectangle() : g.boundingBox();
}

void StrataReceive::warn(const QString& title, const QString& text) {
  QMessageBox::warning(window_, title, text);
}

// ---- 유산(주변유적): Strata fetchNearbyHeritage 와 같은 순서 ----------------------------------

void StrataReceive::fetchHeritage() {
  const QString survey = surveyPath_();
  if (survey.isEmpty()) {
    warn(QStringLiteral("주변유적 받기"), QStringLiteral("조사를 먼저 열거나 저장하세요.\n받은 자료는 그 조사에만 붙습니다."));
    return;
  }
  auto* areaVl = LayerOps::findByLayerKey(QgsProject::instance(), QStringLiteral("survey_area"));
  const QgsGeometry area = surveyUnion();
  if (!areaVl || area.isNull() || area.isEmpty()) {
    warn(QStringLiteral("주변유적 받기"), QStringLiteral("조사구역이 비어 있습니다. 「그리기 › 조사구역 그리기」로 먼저 그리세요.\n그린 구역으로 어느 시·군인지 판정합니다."));
    return;
  }
  // 창을 먼저 띄운다. 판정이 늦어도 아무것도 못 보는 일이 없게(Strata 와 같음)
  ensureHeritageBrowser();
  browser_->show();
  browser_->raise();
  browser_->activateWindow();
  browser_->showWaiting(QStringLiteral("조사구역이 속한 시·군을 찾는 중입니다…"));
  if (!resolver_) {
    resolver_ = new HeritageRegionResolver(this);
    connect(resolver_, &HeritageRegionResolver::failed, this, [this](const QString& why) {
      resolver_->cancel();
      if (browser_) browser_->showWaiting(why);
      openHeritageBrowserFor({}, why);
    });
    connect(resolver_, &HeritageRegionResolver::resolved, this, [this](const HeritageRegion& region) { openHeritageBrowserFor(region); });
  }
  resolver_->resolve(area, QgsProject::instance()->crs(), QgsProject::instance());
}

void StrataReceive::ensureHeritageBrowser() {
  if (browser_) return;
  browser_ = new KaHeritageBrowser(window_);
  connect(browser_, &KaHeritageBrowser::failed, this, [this](const QString& why) {
    say(QStringLiteral("주변유적 받기: ") + why, 10000);
  });
  connect(browser_, &KaHeritageBrowser::stageChanged, this, [this](HeritageStage, const QString& message) {
    say(message, 6000);
  });
  // 한 종류를 받을 때마다 바로 프로젝트에 올린다(Strata 와 같음)
  connect(browser_, &KaHeritageBrowser::datasetReady, this, [this](HeritageDataset dataset, const QStringList& files) {
    const QString archiveRoot = QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)).filePath(QStringLiteral("주변유적/SHP"));
    QDir().mkpath(archiveRoot);
    const HeritageImport::Result r = HeritageImport::loadDataset(QgsProject::instance(), dataset, files, archiveRoot, browser_->regionLabelForImport());
    for (const QString& m : r.messages)
      say(m, 8000);
    if (!r.ok()) {
      browser_->rejectDataset(r.error, r.retryableDownload);
      return;
    }
    if (r.emptyInScope) return;
    QgsProject::instance()->setDirty(true);
    emit received(QStringLiteral("%1 %2곳").arg(HeritageStyle::layerName(dataset)).arg(r.featureCount));
  });
  connect(browser_, &KaHeritageBrowser::allFinished, this, [this] {
    browser_->hide();
    say(QStringLiteral("주변유적 자료 처리를 마쳤습니다. 받은 자료는 레이어 칸 「참조 지도」에 있고, 「저장」하면 조사에 남습니다."), 12000);
  });
  connect(browser_, &KaHeritageBrowser::agreementAccepted, this, &StrataReceive::saveAgreementReceipt);
}

void StrataReceive::openHeritageBrowserFor(const HeritageRegion& region, const QString& reason) {
  ensureHeritageBrowser();
  KaHeritageRegionDialog choice(region, reason, browser_);  // 조사 5 km 안의 이웃 시·군도 보인다
  if (choice.exec() != QDialog::Accepted) {
    browser_->showWaiting(QStringLiteral("취소했습니다."));
    return;
  }
  if (!HeritageIntranetSettings::hasCredentials()) {
    KaHeritageAccountDialog account(window_);
    if (account.exec() != QDialog::Accepted || !HeritageIntranetSettings::hasCredentials()) {
      browser_->showWaiting(QStringLiteral("받기를 시작하지 않았습니다. 국가유산 인트라넷 아이디·비밀번호를 저장한 뒤 다시 눌러 주세요."));
      return;
    }
  }
  // 받는 자리는 Strata 와 같은 로컬 폴더(앱 이름이 같아 같은 자리): OneDrive 동기화 폴더를 거치지 않는다
  const QString root = QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation))
                           .filePath(QStringLiteral("주변유적/%1 %2/원본").arg(choice.sido(), choice.city()));
  if (!QDir().mkpath(root)) {
    warn(QStringLiteral("주변유적 받기"), QStringLiteral("주변유적 자료를 둘 폴더를 만들지 못했습니다: ") + root);
    return;
  }
  browser_->setDownloadRoot(root);
  browser_->setTarget(choice.sido(), choice.city(), HeritageStyle::allDatasets());
  browser_->setFetchPlan(choice.plan());
  browser_->show();
  browser_->raise();
  browser_->start();
}

void StrataReceive::saveAgreementReceipt(const QDateTime& when, const QString& terms) {
  const QString survey = surveyPath_();
  if (survey.isEmpty()) return;
  const QString dir = QDir(QFileInfo(survey).absolutePath()).filePath(QStringLiteral("주변유적/receipts"));
  if (!QDir().mkpath(dir)) return;
  QFile file(QDir(dir).filePath(QStringLiteral("서약서-%1.txt").arg(when.toString(QStringLiteral("yyyyMMdd-HHmmss")))));
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;
  QTextStream out(&file);
  out.setEncoding(QStringConverter::Utf8);
  out << QStringLiteral("국가유산 공간정보 원본자료 사용 서약서 동의 기록") << Qt::endl;
  out << QStringLiteral("동의 시각: ") << when.toString(Qt::ISODate) << Qt::endl;
  out << QStringLiteral("기관 계정: ") << HeritageIntranetSettings::describeForLog() << Qt::endl;
  out << QStringLiteral("받은 곳: Strata 3D") << Qt::endl;
  out << QStringLiteral("----") << Qt::endl;
  out << terms << Qt::endl;
}

// ---- 지적: Strata downloadCadastral 과 같은 요청(조사구역 합 + 5 km) --------------------------

void StrataReceive::fetchCadastral() {
  if (job_) {
    say(QStringLiteral("지도 자료를 준비 중입니다. 완료를 기다리거나 취소하세요."), 5000);
    return;
  }
  const QString survey = surveyPath_();
  if (survey.isEmpty() || !QFileInfo::exists(survey)) {
    QMessageBox::information(window_, QStringLiteral("지적도"), QStringLiteral("먼저 조사를 열거나 저장하고 조사구역을 그려 주세요."));
    return;
  }
  QgsProject* project = QgsProject::instance();
  CadastralPortal::Request request;
  request.surveyCrs = project->crs();
  request.workCrs = project->crs();
  request.context = project->transformContext();
  request.survey = surveyUnion();
  if (request.survey.isNull()) {
    QMessageBox::information(window_, QStringLiteral("지적도"), QStringLiteral("조사구역을 먼저 그려 주세요. 경계에서 주변 5km의 지적도를 받습니다."));
    return;
  }
  request.apiKey = VworldSettings::loadApiKey();
  request.credentials = CadastralPortal::credentials();
  if (request.credentials.username.isEmpty() || request.credentials.password.isEmpty()) {
    // 계정 넣는 창은 Strata 「기타 › 계정과 키」에 있다(같은 설정 파일을 쓴다)
    warn(QStringLiteral("지적도"), QStringLiteral("VWorld 지적 계정이 없습니다. Strata 의 「기타 › 계정과 키」에서 넣은 뒤 다시 받으세요."));
    return;
  }
  request.directory = QFileInfo(survey).absoluteDir().filePath(QStringLiteral("지적도"));
  auto* dialog = new KaDownloadProgressDialog(QStringLiteral("지적도 다운로드"),
                                              QStringLiteral("VWorld · 조사구역 경계에서 주변 5km의 지적도를 준비합니다."), window_);
  dialog->setRange(0, 100);
  dialog->setValue(0);
  const QPointer<KaDownloadProgressDialog> progress(dialog);
  const QPointer<StrataReceive> self(this);
  auto prepare = [request, progress](QgsFeedback*, const std::function<bool()>& canceled) {
    return CadastralPortal::prepare(request, canceled, [progress](int value, const QString& phase) {
      if (progress)
        QMetaObject::invokeMethod(progress, [progress, value, phase] {
          if (progress) {
            progress->setLabelText(phase);
            progress->setValue(value);
          }
        }, Qt::QueuedConnection);
    });
  };
  auto complete = [self, progress](const PreparedReferenceMap& result) {
    if (progress) {
      progress->hide();
      progress->deleteLater();
    }
    if (!self) return;
    if (result.status == PreparedReferenceMap::Status::Cancelled) {
      self->say(QStringLiteral("지적도 받기를 취소했습니다."), 5000);
      return;
    }
    QString error = result.error;
    QgsMapLayer* layer = result.isReady() ? CadastralImport::addPrepared(QgsProject::instance(), nullptr, result, &error) : nullptr;
    if (!layer) {
      self->warn(QStringLiteral("지적도 받기"), (result.accountRejected ? QStringLiteral("아이디나 비밀번호가 맞지 않습니다.\n") : QString()) +
                                                    (error.isEmpty() ? QStringLiteral("서버 응답이 없거나 받은 자료를 올리지 못했습니다.") : error));
      return;
    }
    QgsProject::instance()->setDirty(true);
    emit self->received(QStringLiteral("조사 주변 5km 지적도"));
  };
  auto* job = new KaReferenceDownloadJob(QStringLiteral("조사 주변 지적도"), std::move(prepare), std::move(complete));
  job_ = job;
  connect(dialog, &KaDownloadProgressDialog::canceled, job, &QgsTask::cancel);
  dialog->show();
  QgsApplication::taskManager()->addTask(job);
}

// ---- 토양·지질·수계: Strata startReferenceDownload 와 같은 서비스, 범위만 조사구역 + 5 km ----------

void StrataReceive::fetchThematic(Kind kind) {
  if (job_) {
    say(QStringLiteral("지도 자료를 내려받고 있습니다. 완료를 기다리거나 취소하세요."), 5000);
    return;
  }
  const QString title = kind == Kind::Soil ? QStringLiteral("토양도") : kind == Kind::Geology ? QStringLiteral("지질도") : QStringLiteral("수계도");
  const QString dir = ReferenceStorage::surveyFolder(surveyPath_());
  if (dir.isEmpty()) {
    QMessageBox::information(window_, title, QStringLiteral("먼저 「새 조사」를 만들거나 조사를 여세요. 받은 자료는 조사 폴더에 둡니다."));
    return;
  }
  QgsRectangle box = surveyBox();
  if (box.isNull() || (box.width() <= 0 && box.height() <= 0)) {
    QMessageBox::information(window_, title, QStringLiteral("조사구역을 먼저 그려 주세요. 조사구역 둘레 5 km 를 받습니다."));
    return;
  }
  box.grow(kScopeBufferM);
  QgsRectangle ext = box;
  const QgsCoordinateReferenceSystem crs5186(QStringLiteral("EPSG:5186"));
  if (QgsProject::instance()->crs() != crs5186) {
    try {
      ext = QgsCoordinateTransform(QgsProject::instance()->crs(), crs5186, QgsProject::instance()).transformBoundingBox(box);
    } catch (const QgsException&) {
      warn(title, QStringLiteral("조사구역 범위를 좌표 변환하지 못했습니다."));
      return;
    }
  }
  QString apiKey;
  if (kind == Kind::River) {
    apiKey = VworldSettings::loadApiKey();
    if (apiKey.trimmed().isEmpty()) {
      warn(title, QStringLiteral("VWorld 인증키가 없습니다. Strata 「기타 › 계정과 키」에서 먼저 등록하세요."));
      return;
    }
  }
  const QString target = QDir(dir).filePath(kind == Kind::Soil ? QStringLiteral("토양도_흙토람.gpkg")
                                            : kind == Kind::Geology ? QStringLiteral("지질도_KIGAM.gpkg")
                                                                    : QStringLiteral("수계도_VWorld.gpkg"));
  auto* progress = new QProgressDialog(title + QStringLiteral(" 자료를 내려받는 중입니다(조사구역 둘레 5 km)."), QStringLiteral("취소"), 0, 0, window_);
  progress->setWindowTitle(title + QStringLiteral(" 내려받기"));
  progress->setWindowModality(Qt::NonModal);
  progress->setMinimumDuration(0);
  progress->setAutoClose(false);
  progress->setAutoReset(false);
  const auto context = QgsProject::instance()->transformContext();
  auto prepare = [kind, ext, target, apiKey, context](QgsFeedback* feedback) {
    switch (kind) {
      case Kind::Soil: return SoilMapService::prepare(ext, target, context, feedback);
      case Kind::Geology: return GeologyMapService::prepare(ext, target, context, feedback);
      case Kind::River: return RiverMapService::prepare(ext, apiKey, target, context, feedback);
      default: return PreparedReferenceMap{};
    }
  };
  const QPointer<QProgressDialog> dialog(progress);
  const QPointer<StrataReceive> self(this);
  const QString survey = surveyPath_();
  auto complete = [self, dialog, kind, title, survey](const PreparedReferenceMap& result) {
    if (dialog) {
      dialog->hide();
      dialog->deleteLater();
    }
    if (!self) return;
    if (result.status == PreparedReferenceMap::Status::Cancelled) {
      self->say(title + QStringLiteral(" 내려받기를 취소했습니다."), 6000);
      return;
    }
    if (!result.isReady()) {
      self->warn(title + QStringLiteral(" 내려받기 실패"), result.error.isEmpty() ? QStringLiteral("지도 자료를 받지 못했습니다. 인터넷 연결을 확인한 뒤 다시 시도하세요.") : result.error);
      return;
    }
    QString error;
    QgsMapLayer* layer = nullptr;
    const auto foldersBefore = ReferenceStorage::referencedFolderNames(QgsProject::instance());
    if (kind == Kind::Soil) layer = SoilMapService::addPrepared(QgsProject::instance(), nullptr, result, &error);
    else if (kind == Kind::Geology) layer = GeologyMapService::addPrepared(QgsProject::instance(), nullptr, result, &error);
    else layer = RiverMapService::addPrepared(QgsProject::instance(), nullptr, result, &error);
    if (!layer) {
      self->warn(title + QStringLiteral(" 표시 실패"), error.isEmpty() ? QStringLiteral("받은 지도를 열지 못했습니다.") : error);
      return;
    }
    // 바뀐 받기 폴더는 다음 저장 때 지운다(Strata 와 같은 장부)
    auto& ledger = ReferenceStorage::sessionLedger();
    if (result.storage) ledger.adopt(result.storage->path(), survey);
    ledger.supersede(foldersBefore, ReferenceStorage::referencedFolderNames(QgsProject::instance()));
    QgsProject::instance()->setDirty(true);
    if (!result.warnings.isEmpty()) self->warn(title + QStringLiteral(" 확인 사항"), result.warnings.join(QLatin1Char('\n')));
    emit self->received(title);
  };
  auto* job = new KaReferenceDownloadJob(title, std::move(prepare), std::move(complete));
  job_ = job;
  connect(progress, &QProgressDialog::canceled, job, &KaReferenceDownloadJob::cancel);
  progress->show();
  QgsApplication::taskManager()->addTask(job);
}
