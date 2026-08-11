#ifndef DIFFPRESENTER_H
#define DIFFPRESENTER_H

#include <QObject>
#include <QString>
#include <QStringList>

class DiffPresenter : public QObject {
  Q_OBJECT

public:
  enum class DiffMode { Unified, SideBySide };

  explicit DiffPresenter(QObject *parent = nullptr);

  void setMonospaceFont(const QString &family, int pointSize);

  void setMode(DiffMode mode);
  DiffMode mode() const;

  QString formatDiff(const QStringList &lines, bool includeHunkLinks = false,
                     bool unstageLink = false) const;
  QString formatCurrent() const;
  bool hasCurrent() const;
  bool isLfsPointer(const QStringList &lines) const;
  QString lfsPointerHtml(const QStringList &lines) const;

private:
  QString formatUnifiedDiff(const QStringList &lines, bool includeHunkLinks,
                            bool unstageLink) const;
  QString formatSideBySideDiff(const QStringList &lines, bool includeHunkLinks,
                               bool unstageLink) const;

  QString m_fontFamily;
  int m_fontSize = 0;
  DiffMode m_mode = DiffMode::Unified;
  mutable QStringList m_lastDiffLines;
  mutable bool m_lastIncludeHunkLinks = false;
  mutable bool m_lastUnstageLink = false;
};

#endif // DIFFPRESENTER_H
