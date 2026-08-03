#ifndef DIFFPRESENTER_H
#define DIFFPRESENTER_H

#include <QObject>
#include <QString>
#include <QStringList>

class DiffPresenter : public QObject {
  Q_OBJECT

public:
  explicit DiffPresenter(QObject *parent = nullptr);

  QString formatDiff(const QStringList &lines) const;
  bool isLfsPointer(const QStringList &lines) const;
  QString lfsPointerHtml(const QStringList &lines) const;
};

#endif // DIFFPRESENTER_H
