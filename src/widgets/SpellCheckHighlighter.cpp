#include "SpellCheckHighlighter.h"

#include <QFile>
#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <QRegularExpressionMatchIterator>
#include <QTextCharFormat>

SpellCheckHighlighter::SpellCheckHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent) {}

void SpellCheckHighlighter::setDictionary(const QString &path) {
  m_words.clear();
  if (path.isEmpty())
    return;

  QFile f(path);
  if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
    return;

  while (!f.atEnd()) {
    const QByteArray raw = f.readLine().trimmed();
    if (raw.isEmpty())
      continue;
    m_words.insert(QString::fromUtf8(raw).toLower());
  }

  rehighlight();
}

void SpellCheckHighlighter::setEnabled(bool enabled) {
  if (m_enabled == enabled)
    return;
  m_enabled = enabled;
  rehighlight();
}

void SpellCheckHighlighter::highlightBlock(const QString &text) {
  if (!m_enabled || m_words.isEmpty())
    return;

  static const QRegularExpression wordRe(QStringLiteral("\\b\\w+\\b"));
  QRegularExpressionMatchIterator it = wordRe.globalMatch(text);

  QTextCharFormat fmt;
  fmt.setUnderlineStyle(QTextCharFormat::SpellCheckUnderline);
  fmt.setUnderlineColor(Qt::red);

  while (it.hasNext()) {
    const QRegularExpressionMatch match = it.next();
    const QString word = match.captured().toLower();
    if (word.length() <= 1)
      continue;
    if (!m_words.contains(word))
      setFormat(match.capturedStart(), match.capturedLength(), fmt);
  }
}
