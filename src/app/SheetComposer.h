#pragma once

#include <QImage>
#include <QString>
#include <QStringList>

struct SheetInfo {
  QString title;
  double visibleWidthM = 0;      // 화면 가운데에서 보이는 지상 폭
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

// 입체지형 도면: 화면 + 제목·축척자·방위표·고도/출처 범례·좌표계·평가 요약.
// 색과 글꼴은 KaTheme 토큰만 쓴다(hgis 도면 스튜디오와 같은 디자인 언어).
QImage composeSheet(const QImage& view, const SheetInfo& info);

// 도면 PNG 를 A3 가로 PDF 한 쪽으로 저장한다.
bool saveSheetPdf(const QImage& sheet, const QString& path, const QString& title);

// 1·2·5×10ⁿ 중 목표 길이 이하에서 가장 큰 눈금 길이(m)
double niceScaleLengthM(double targetM);
