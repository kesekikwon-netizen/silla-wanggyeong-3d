#include "DbpiaBangriLedger.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>

namespace strata3d {

QString DbpiaBangriLedger::defaultLedgerPath() {
  const QString rel = QStringLiteral("docs/dbpia_silla_bangri_9ju5sogyeong_ledger.json");
  QStringList candidates;
#ifdef S3D_SOURCE_DIR
  candidates << QDir(QStringLiteral(S3D_SOURCE_DIR)).filePath(rel);
#endif
  if (QCoreApplication::instance()) {
    const QString appDir = QCoreApplication::applicationDirPath();
    candidates << QDir(appDir).filePath(rel)
               << QDir(appDir + QStringLiteral("/..")).filePath(rel)
               << QDir(appDir + QStringLiteral("/../..")).filePath(rel);
  }
  candidates << QDir::current().filePath(rel)
             << QStringLiteral("C:/Users/권을/Documents/hgis-3d-engine/docs/dbpia_silla_bangri_9ju5sogyeong_ledger.json");

  for (const QString& c : candidates) {
    if (QFileInfo::exists(c)) return QFileInfo(c).canonicalFilePath();
  }
  return candidates.front();
}

bool DbpiaBangriLedger::loadFromFile(const QString& jsonPath, QString* errorOut) {
  const QString path = jsonPath.isEmpty() ? defaultLedgerPath() : jsonPath;
  QFile f(path);
  if (!f.open(QIODevice::ReadOnly)) {
    if (errorOut) *errorOut = QStringLiteral("DBpia 학술 원장 파일을 열지 못했습니다: %1").arg(path);
    return false;
  }
  QJsonParseError pe;
  const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &pe);
  if (doc.isNull() || !doc.isObject()) {
    if (errorOut) *errorOut = QStringLiteral("DBpia 학술 원장 JSON 파싱 실패: %1").arg(pe.errorString());
    return false;
  }

  papers_.clear();
  const QJsonArray arr = doc.object().value(QStringLiteral("papers")).toArray();
  papers_.reserve(size_t(arr.size()));

  for (const QJsonValue& v : arr) {
    if (!v.isObject()) continue;
    const QJsonObject o = v.toObject();
    DbpiaPaperEntry e;
    e.nodeId = o.value(QStringLiteral("nodeId")).toString().trimmed();
    if (e.nodeId.isEmpty()) continue;
    e.nodeType = o.value(QStringLiteral("nodeType")).toString();
    e.title = o.value(QStringLiteral("title")).toString();
    e.authors = o.value(QStringLiteral("authors")).toString();
    e.journal = o.value(QStringLiteral("journal")).toString();
    e.publisher = o.value(QStringLiteral("publisher")).toString();
    e.year = o.value(QStringLiteral("year")).toString();
    e.abstractText = o.value(QStringLiteral("abstract")).toString();
    e.url = o.value(QStringLiteral("url")).toString();

    for (const QJsonValue& qv : o.value(QStringLiteral("matchedQueries")).toArray())
      e.matchedQueries << qv.toString();
    for (const QJsonValue& kv : o.value(QStringLiteral("keywords")).toArray())
      e.keywords << kv.toString();

    const QJsonObject ai = o.value(QStringLiteral("aiFullTextAnalysis")).toObject();
    if (!ai.isEmpty()) {
      const QJsonObject qa = ai.value(QStringLiteral("coreQa")).toObject().value(QStringLiteral("core_content_qa")).toObject();
      e.coreQuestion = qa.value(QStringLiteral("question")).toString();
      e.coreAnswer = qa.value(QStringLiteral("answer")).toString();

      const QJsonObject ch = ai.value(QStringLiteral("chapterSummary")).toObject().value(QStringLiteral("document")).toObject();
      const QJsonArray secs = ch.value(QStringLiteral("sections")).toArray();
      if (!secs.isEmpty()) {
        e.oneLineSummary = secs.last().toObject().value(QStringLiteral("content")).toString();
      }
      if (e.oneLineSummary.isEmpty() && !e.coreAnswer.isEmpty()) {
        e.oneLineSummary = e.coreAnswer;
      }
    }
    if (e.oneLineSummary.isEmpty() && !e.abstractText.isEmpty()) {
      e.oneLineSummary = e.abstractText.left(200);
    }
    if (e.coreAnswer.isEmpty() && !e.abstractText.isEmpty()) {
      e.coreAnswer = e.abstractText;
    }
    papers_.push_back(std::move(e));
  }
  return !papers_.empty();
}

const DbpiaPaperEntry* DbpiaBangriLedger::findByNodeId(const QString& nodeId) const {
  for (const DbpiaPaperEntry& p : papers_) {
    if (p.nodeId.compare(nodeId, Qt::CaseInsensitive) == 0) return &p;
  }
  return nullptr;
}

std::vector<DbpiaPaperEntry> DbpiaBangriLedger::matchForPreset(const QStringList& preferredNodeIds,
                                                               const QStringList& fallbackKeywords,
                                                               int maxCount) const {
  std::vector<DbpiaPaperEntry> out;
  QSet<QString> used;

  for (const QString& nid : preferredNodeIds) {
    if (int(out.size()) >= maxCount) break;
    if (const DbpiaPaperEntry* p = findByNodeId(nid)) {
      if (!used.contains(p->nodeId)) {
        used.insert(p->nodeId);
        out.push_back(*p);
      }
    }
  }

  for (const DbpiaPaperEntry& p : papers_) {
    if (int(out.size()) >= maxCount) break;
    if (used.contains(p.nodeId)) continue;
    bool hit = false;
    for (const QString& kw : fallbackKeywords) {
      if (kw.isEmpty()) continue;
      if (p.title.contains(kw, Qt::CaseInsensitive) ||
          p.abstractText.contains(kw, Qt::CaseInsensitive) ||
          p.matchedQueries.join(QLatin1Char(' ')).contains(kw, Qt::CaseInsensitive)) {
        hit = true;
        break;
      }
    }
    if (hit) {
      used.insert(p.nodeId);
      out.push_back(p);
    }
  }
  return out;
}

}  // namespace strata3d
