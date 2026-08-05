#ifndef DIFFPRESENTER_H
#define DIFFPRESENTER_H

#include <QObject>
#include <QString>
#include <QStringList>

class DiffPresenter : public QObject {
  Q_OBJECT

public:
  explicit DiffPresenter(QObject *parent = nullptr);

  void setMonospaceFont(const QString &family, int pointSize);

  QString formatDiff(const QStringList &lines, bool includeHunkLinks = false,
                     bool unstageLink = false) const;
  bool isLfsPointer(const QStringList &lines) const;
  QString lfsPointerHtml(const QStringList &lines) const;

private:
  QString m_fontFamily;
  int m_fontSize = 0;
};

#endif // DIFFPRESENTER_H
