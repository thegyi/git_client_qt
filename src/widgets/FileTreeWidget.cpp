#include "FileTreeWidget.h"

#include <QApplication>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QIcon>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QPolygonF>
#include <QStyle>
#include <QTreeWidgetItemIterator>
#include <functional>

FileTreeWidget::FileTreeWidget(const QString &headerLabel, QWidget *parent)
    : QTreeWidget(parent) {
  if (headerLabel.isEmpty())
    setHeaderHidden(true);
  else
    setHeaderLabels(QStringList{headerLabel});
  setRootIsDecorated(true);
  setContextMenuPolicy(Qt::CustomContextMenu);
  setSelectionMode(QAbstractItemView::SingleSelection);
  setDragEnabled(true);
  setAcceptDrops(true);
  setDragDropMode(QAbstractItemView::DragDrop);
  setDefaultDropAction(Qt::MoveAction);
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

QTreeWidgetItem *FileTreeWidget::itemForPath(const QString &path) {
  QTreeWidgetItemIterator it(this);
  while (*it) {
    if (itemPath(*it) == path)
      return *it;
    ++it;
  }
  return nullptr;
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

  const auto drawGlyph = [&](const QColor &color,
                             const std::function<void()> &draw) {
    painter.setBrush(color);
    painter.setPen(Qt::NoPen);
    painter.drawRoundedRect(pixmap.rect().adjusted(2, 2, -2, -2), 4, 4);
    painter.setPen(
        QPen(Qt::white, 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);
    draw();
  };

  if (status == "?") {
    drawGlyph(QColor(108, 117, 125), [&]() {
      painter.drawLine(8, 5, 8, 7);
      painter.drawPoint(8, 11);
    });
  } else if (status == "A") {
    drawGlyph(QColor(40, 167, 69), [&]() {
      painter.drawLine(8, 4, 8, 12);
      painter.drawLine(4, 8, 12, 8);
    });
  } else if (status == "M" || status == "T") {
    drawGlyph(QColor(0, 123, 255), [&]() {
      painter.drawLine(5, 12, 11, 5);
      painter.drawLine(11, 5, 9, 5);
      painter.drawLine(11, 5, 11, 7);
    });
  } else if (status == "D") {
    drawGlyph(QColor(220, 53, 69), [&]() { painter.drawLine(4, 8, 12, 8); });
  } else if (status == "R") {
    drawGlyph(QColor(156, 39, 176), [&]() {
      painter.drawLine(4, 8, 12, 8);
      painter.drawLine(9, 5, 12, 8);
      painter.drawLine(9, 11, 12, 8);
    });
  } else if (status == "C") {
    drawGlyph(QColor(0, 150, 136), [&]() {
      painter.drawRect(4, 5, 4, 4);
      painter.drawRect(7, 7, 4, 4);
    });
  } else if (status == "!" || status == "-") {
    drawGlyph(QColor(108, 117, 125), [&]() {
      painter.drawLine(5, 5, 11, 11);
      painter.drawLine(11, 5, 5, 11);
    });
  } else {
    painter.setBrush(QColor(160, 160, 160));
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(pixmap.rect().adjusted(2, 2, -2, -2));
  }

  return QIcon(pixmap);
}

void FileTreeWidget::startDrag(Qt::DropActions) {
  QTreeWidgetItem *item = currentItem();
  if (!item)
    return;

  const QString path = itemPath(item);
  if (path.isEmpty())
    return;

  auto *mimeData = new QMimeData();
  mimeData->setData(QStringLiteral("application/x-gitclientqt-filepath"),
                    path.toUtf8());

  auto *drag = new QDrag(this);
  drag->setMimeData(mimeData);
  drag->exec(Qt::MoveAction);
}

void FileTreeWidget::dragEnterEvent(QDragEnterEvent *event) {
  if (event->mimeData()->hasFormat(
          QStringLiteral("application/x-gitclientqt-filepath"))) {
    event->acceptProposedAction();
  } else {
    QTreeWidget::dragEnterEvent(event);
  }
}

void FileTreeWidget::dragMoveEvent(QDragMoveEvent *event) {
  if (event->mimeData()->hasFormat(
          QStringLiteral("application/x-gitclientqt-filepath"))) {
    event->acceptProposedAction();
  } else {
    QTreeWidget::dragMoveEvent(event);
  }
}

void FileTreeWidget::dropEvent(QDropEvent *event) {
  const QByteArray data = event->mimeData()->data(
      QStringLiteral("application/x-gitclientqt-filepath"));
  if (data.isEmpty() || event->source() == this) {
    event->ignore();
    return;
  }

  const QString path = QString::fromUtf8(data);
  if (!path.isEmpty()) {
    event->acceptProposedAction();
    emit fileDropped(path);
  } else {
    event->ignore();
  }
}
