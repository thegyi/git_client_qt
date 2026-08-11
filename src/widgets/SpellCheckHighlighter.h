#ifndef SPELLCHECKHIGHLIGHTER_H
#define SPELLCHECKHIGHLIGHTER_H

#include <QSet>
#include <QString>
#include <QSyntaxHighlighter>

class SpellCheckHighlighter : public QSyntaxHighlighter {
  Q_OBJECT
public:
  explicit SpellCheckHighlighter(QTextDocument *parent = nullptr);

  void setDictionary(const QString &path);
  void setEnabled(bool enabled);

protected:
  void highlightBlock(const QString &text) override;

private:
  bool m_enabled = false;
  QSet<QString> m_words;
};

#endif // SPELLCHECKHIGHLIGHTER_H
