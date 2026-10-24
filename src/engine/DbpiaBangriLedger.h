#pragma once

#include <QString>
#include <QStringList>
#include <vector>

namespace strata3d {

struct DbpiaPaperEntry {
  QString nodeId;
  QString nodeType;
  QString title;
  QString authors;
  QString journal;
  QString publisher;
  QString year;
  QString abstractText;
  QString url;
  QStringList matchedQueries;
  QStringList keywords;
  QString oneLineSummary;
  QString coreQuestion;
  QString coreAnswer;
};

class DbpiaBangriLedger {
 public:
  DbpiaBangriLedger() = default;

  // jsonPath 가 비어 있으면 실행 파일 주변이나 S3D_SOURCE_DIR/docs 에서 자동으로 찾는다.
  bool loadFromFile(const QString& jsonPath = QString(), QString* errorOut = nullptr);

  bool loaded() const { return !papers_.empty(); }
  int totalPapers() const { return int(papers_.size()); }
  const std::vector<DbpiaPaperEntry>& papers() const { return papers_; }

  const DbpiaPaperEntry* findByNodeId(const QString& nodeId) const;

  // 프리셋에 지정된 핵심 nodeId 목록을 우선 배치하고, 부족하면 키워드 매칭으로 채운다.
  std::vector<DbpiaPaperEntry> matchForPreset(const QStringList& preferredNodeIds,
                                              const QStringList& fallbackKeywords,
                                              int maxCount = 8) const;

  static QString defaultLedgerPath();

 private:
  std::vector<DbpiaPaperEntry> papers_;
};

}  // namespace strata3d
