#include "FileTreeWidget.h"

#include <QApplication>
#include <QIcon>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QPolygonF>
#include <QStyle>

FileTreeWidget::FileTreeWidget(const QString &headerLabel, QWidget *parent)
    : QTreeWidget(parent) {
  if (headerLabel.isEmpty())
    setHeaderHidden(true);
  else
    setHeaderLabels(QStringList{headerLabel});
  setRootIsDecorated(true);
  setContextMenuPolicy(Qt::CustomContextMenu);
}

QString FileTreeWidget::itemPath(QTreeWidgetItem *item) const {
  QStringList parts;
  QTreeWidgetItem *current = item;
  while (current && current != invisibleRootItem()) {
    parts.prepend(current->text(0));
    current = current->parent();
  }
  return parts.join('/');
}

void FileTreeWidget::addFile(const QString &filePath, const QString &status) {
  const QStringList parts = filePath.split('/');
  const QString fileName = parts.last();
  if (fileName.isEmpty()) {
    return;
  }
  QTreeWidgetItem *parentItem = invisibleRootItem();
  for (int i = 0; i < parts.size() - 1; ++i) {
    const QString &dirName = parts.at(i);
    QTreeWidgetItem *child = nullptr;
    for (int c = 0; c < parentItem->childCount(); ++c) {
      if (parentItem->child(c)->text(0) == dirName) {
        child = parentItem->child(c);
        break;
      }
    }
    if (!child) {
      child = new QTreeWidgetItem(parentItem, QStringList{dirName});
      child->setIcon(0, statusIcon(QString()));
    }
    parentItem = child;
  }
  QTreeWidgetItem *leaf =
      new QTreeWidgetItem(parentItem, QStringList{fileName});
  leaf->setIcon(0, statusIcon(status));
  if (!status.isEmpty()) {
    leaf->setData(0, Qt::UserRole, status);
  }
}

QIcon FileTreeWidget::statusIcon(const QString &status) {
  if (status.isEmpty())
    return QApplication::style()->standardIcon(QStyle::SP_DirIcon);

  QPixmap pixmap(16, 16);
  pixmap.fill(Qt::transparent);

  QPainter painter(&pixmap);
  painter.setRenderHint(QPainter::Antialiasing);

  if (status == "?" || status == "A") {
    QPen pen(QColor(40, 167, 69));
    pen.setWidth(2);
    pen.setCapStyle(Qt::RoundCap);
    painter.setPen(pen);
    painter.drawLine(8, 4, 8, 12);
    painter.drawLine(4, 8, 12, 8);
  } else if (status == "M") {
    QPen pen(QColor(0, 123, 255));
    pen.setWidth(2);
    pen.setCapStyle(Qt::RoundCap);
    painter.setPen(pen);
    painter.drawLine(5, 13, 11, 5);
    QPolygonF tip;
    tip << QPointF(11, 5) << QPointF(12, 4) << QPointF(13, 5) << QPointF(12, 6);
    painter.setBrush(QColor(0, 123, 255));
    painter.setPen(Qt::NoPen);
    painter.drawPolygon(tip);
    QPolygonF eraser;
    eraser << QPointF(5, 13) << QPointF(4, 14) << QPointF(5, 15)
           << QPointF(6, 14);
    painter.drawPolygon(eraser);
  } else if (status == "D") {
    QPen pen(QColor(220, 53, 69));
    pen.setWidth(2);
    pen.setCapStyle(Qt::RoundCap);
    painter.setPen(pen);
    painter.drawLine(4, 8, 12, 8);
  } else {
    painter.setBrush(QColor(160, 160, 160));
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(pixmap.rect().adjusted(2, 2, -2, -2));
  }

  return QIcon(pixmap);
}
