#pragma once

#include <QImage>
#include <QString>
#include <QStringList>
#include <vector>

#include "BangriGridEngine.h"
#include "BangriSurveyEvaluator.h"
#include "BangriTerrainValidator.h"
#include "DbpiaBangriLedger.h"
#include "FossilizedRoadAnalyzer.h"
#include "SillaMoundReconstructor.h"

struct SheetInfo {
  QString title;
  bool hasTrench = false;        // 시굴 트렌치 띠가 그려져 있으면 범례에 넣는다
  double visibleWidthM = 0;     // 화면 가운데에서 보이는 지상 폭
  double yawDegFromNorth = 0;    // 시점 방위(북 0° 시계 방향)
  QString crsLabel;              // 예: "EPSG:5187 (동부원점)"
  QString demName;
  QString imageryName;           // 예: "Google Satellite XYZ" 또는 빈 문자열
  float zMin = 0;
  float zMax = 0;
  bool elevationRamp = false;    // 표고 색띠 보기일 때만 범례에 색띠를 그린다
  QStringList lines;             // 평가 요약 등 본문(한 줄씩)
  QString generated;             // 만든 날짜 문자열
};

// 신라왕경·9주5소경 방리구획 5패널 정밀 고고학 도면 제작용 통합 데이터
struct BangriSheetData {
  SheetInfo baseInfo;
  strata3d::BangriCityPreset preset;
  strata3d::BangriGridResult grid;
  std::vector<strata3d::BangriAnchor> anchors;
  std::vector<strata3d::BangriAnchorResidual> residuals;
  double anchorRmseM = 0.0;
  strata3d::FossilizationResult fossilization;
  strata3d::BangriValidationResult validation;
  strata3d::BangriSurveyEvaluation surveyEval;
  std::vector<strata3d::SillaMound> mounds;
  std::vector<strata3d::DbpiaPaperEntry> citations;
  bool bareEarthMode = true;
  bool reconstructMounds = true;
  float zExaggeration = 1.5f;
};

// 입체지형 도면: 화면 + 제목·축척자·방위표·고도/출처 범례·좌표계·평가 요약.
// 색과 글꼴은 KaTheme 토큰만 쓴다(hgis 도면 스튜디오와 같은 디자인 언어).
QImage composeSheet(const QImage& view, const SheetInfo& info);

// 신라왕경·9주5소경 5패널 정밀 방리구획 고고학 도면(3400×2400px급 A3 가로 레이아웃)을 합성한다.
QImage composeBangriSheet(const QImage& view, const BangriSheetData& data);

// 도면 PNG 를 A3 가로 PDF 한 쪽으로 저장한다.
bool saveSheetPdf(const QImage& sheet, const QString& path, const QString& title);

// 1·2·5×10ⁿ 중 목표 길이 이하에서 가장 큰 눈금 길이(m)
double niceScaleLengthM(double targetM);
