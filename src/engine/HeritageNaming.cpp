#include "HeritageNaming.h"

#include <QRegularExpression>
#include <QStringList>
#include <algorithm>

namespace strata3d {
namespace {

// 긴 이름이 먼저 와야 「사적 및 명승」이 「사적」으로 잘리지 않는다.
const QStringList& kinds() {
  static const QStringList k = {
      QStringLiteral("사적 및 명승"),       QStringLiteral("국가민속문화유산"),   QStringLiteral("국가등록문화유산"),
      QStringLiteral("국가무형유산"),       QStringLiteral("국가민속문화재"),     QStringLiteral("국가등록문화재"),
      QStringLiteral("시도유형문화유산"),   QStringLiteral("시도민속문화유산"),   QStringLiteral("시도등록문화유산"),
      QStringLiteral("시도기념물"),         QStringLiteral("문화유산자료"),       QStringLiteral("문화재자료"),
      QStringLiteral("유형문화유산"),       QStringLiteral("유형문화재"),         QStringLiteral("천연기념물"),
      QStringLiteral("민속문화재"),         QStringLiteral("기념물"),             QStringLiteral("국보"),
      QStringLiteral("보물"),               QStringLiteral("사적"),               QStringLiteral("명승")};
  return k;
}

const QRegularExpression& numberRe() {
  // 제6호, 제 6 호, 제19·188호, 제1,245호, 제29-1호
  static const QRegularExpression re(QStringLiteral("\\s*제\\s*\\d+(?:\\s*[·,\\-]\\s*\\d+)*\\s*호(?![가-힣])"));
  return re;
}

bool isOnlyKind(QString token) {
  token = token.trimmed();
  return !token.isEmpty() && kinds().contains(token);
}

// 괄호 안을 「·」「,」「&」로 나눠 종목만 있는 조각을 빼고 다시 붙인다. 다 빠지면 빈 문자열.
QString cleanGroup(const QString& inner) {
  static const QRegularExpression sep(QStringLiteral("\\s*[·,&]\\s*"));
  QStringList kept;
  bool dropped = false;
  for (const QString& part : inner.split(sep)) {
    if (part.trimmed().isEmpty() || isOnlyKind(part)) dropped = true;
    else kept << part.trimmed();
  }
  if (!dropped) return inner.trimmed();  // 뺀 조각이 없으면 원래 글(「1,245 m」 같은 쉼표) 그대로
  return kept.join(QStringLiteral(" · "));
}

}  // namespace

QString displayHeritageName(const QString& raw) {
  QString s = raw;
  s.remove(numberRe());
  // 국가유산청 목록에서 온 이름의 앞머리 「[사적] 」: 종목은 따로 보이므로 이름에서 뺀다
  static const QRegularExpression lead(QStringLiteral("^\\s*\\[([^\\]]*)\\]\\s*"));
  if (const auto m = lead.match(s); m.hasMatch() && isOnlyKind(m.captured(1))) s.remove(0, m.capturedLength());

  static const QRegularExpression group(QStringLiteral("\\s*\\(([^()]*)\\)"));
  QString out;
  qsizetype last = 0;
  for (auto it = group.globalMatch(s); it.hasNext();) {
    const auto m = it.next();
    out += s.mid(last, m.capturedStart() - last);
    const QString kept = cleanGroup(m.captured(1));
    if (!kept.isEmpty()) out += QStringLiteral(" (") + kept + QLatin1Char(')');
    last = m.capturedEnd();
  }
  out += s.mid(last);

  static const QRegularExpression spaces(QStringLiteral("\\s{2,}"));
  static const QRegularExpression dangling(QStringLiteral("[\\s·&,]+$"));
  out.replace(spaces, QStringLiteral(" "));
  out.remove(dangling);
  return out.trimmed();
}

QString designationKind(const QString& text) {
  for (const QString& k : kinds()) {
    if (text.contains(k)) return k;
  }
  return {};
}

QString schematicModelWording(const QString& text) {
  QString s = text;
  s.replace(QStringLiteral("실측 복원"), QStringLiteral("도식"));
  static const QRegularExpression model(QStringLiteral("3D(\\s*입체)?\\s*복원"));
  s.replace(model, QStringLiteral("3D\\1 도식"));
  return s;
}

QString siteNameOnly(const QString& raw) {
  QString s = displayHeritageName(raw);
  static const QRegularExpression paren(QStringLiteral("\\s*[(（][^()（）]*[)）]"));
  s.remove(paren);
  // 앞에 붙은 종목은 긴 이름부터 지운다(「국가민속문화유산 」이 「국가」보다 먼저)
  QStringList ks = kinds();
  std::sort(ks.begin(), ks.end(), [](const QString& a, const QString& b) { return a.size() > b.size(); });
  for (bool again = true; again;) {
    again = false;
    for (const QString& k : ks) {
      if (s.startsWith(k + QLatin1Char(' '))) {
        s = s.mid(k.size() + 1).trimmed();
        again = true;
        break;
      }
    }
  }
  return s.trimmed();
}

bool containsDesignationNumber(const QString& text) {
  return text.contains(numberRe());
}

}  // namespace strata3d
