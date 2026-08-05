#include "Theme.h"

#include <QColor>
#include <QDir>
#include <QFile>
#include <QFontDatabase>
#include <QMap>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPixmap>
#include <QSettings>
#include <QStandardPaths>
#include <QStringList>

namespace {

enum class Chevron { Down, Up, Right, Left };

QString chevronGlyph(Chevron direction, const QColor &color) {
  static const QMap<Chevron, QString> names = {{Chevron::Down, "down"},
                                               {Chevron::Up, "up"},
                                               {Chevron::Right, "right"},
                                               {Chevron::Left, "left"}};

  const QString cacheDir =
      QStandardPaths::writableLocation(QStandardPaths::CacheLocation) +
      QStringLiteral("/glyphs");
  QDir().mkpath(cacheDir);
  const QString path = QStringLiteral("%1/chevron-%2-%3.png")
                           .arg(cacheDir, names.value(direction),
                                color.name(QColor::HexRgb).mid(1));
  if (QFile::exists(path))
    return path;

  const int size = 16;
  const qreal dpr = 2.0;
  QPixmap pixmap(int(size * dpr), int(size * dpr));
  pixmap.setDevicePixelRatio(dpr);
  pixmap.fill(Qt::transparent);

  QPainter painter(&pixmap);
  painter.setRenderHint(QPainter::Antialiasing, true);
  QPen pen(color);
  pen.setWidthF(1.6);
  pen.setCapStyle(Qt::RoundCap);
  pen.setJoinStyle(Qt::RoundJoin);
  painter.setPen(pen);

  const qreal c = size / 2.0;
  const qreal r = 3.2;
  QPainterPath chevron;
  switch (direction) {
  case Chevron::Down:
    chevron.moveTo(c - r, c - r / 2);
    chevron.lineTo(c, c + r / 2);
    chevron.lineTo(c + r, c - r / 2);
    break;
  case Chevron::Up:
    chevron.moveTo(c - r, c + r / 2);
    chevron.lineTo(c, c - r / 2);
    chevron.lineTo(c + r, c + r / 2);
    break;
  case Chevron::Right:
    chevron.moveTo(c - r / 2, c - r);
    chevron.lineTo(c + r / 2, c);
    chevron.lineTo(c - r / 2, c + r);
    break;
  case Chevron::Left:
    chevron.moveTo(c + r / 2, c - r);
    chevron.lineTo(c - r / 2, c);
    chevron.lineTo(c + r / 2, c + r);
    break;
  }
  painter.drawPath(chevron);
  painter.end();

  if (!pixmap.save(path, "PNG"))
    return QString();
  return path;
}

QStringList availableFamilies() {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  return QFontDatabase::families();
#else
  return QFontDatabase().families();
#endif
}

QString firstAvailable(const QStringList &preferred) {
  const QStringList available = availableFamilies();
  for (const QString &family : preferred) {
    if (available.contains(family, Qt::CaseInsensitive))
      return family;
  }
  return QString();
}

QString blend(const QColor &a, const QColor &b, qreal ratio) {
  const auto mix = [ratio](int x, int y) {
    return qBound(0, int(x * (1.0 - ratio) + y * ratio), 255);
  };
  return QColor(mix(a.red(), b.red()), mix(a.green(), b.green()),
                mix(a.blue(), b.blue()))
      .name();
}

} // namespace

namespace Theme {

QString buildStyleSheet(const QPalette &palette) {
  const QColor window = palette.color(QPalette::Window);
  const QColor windowText = palette.color(QPalette::WindowText);
  const QColor base = palette.color(QPalette::Base);
  const QColor text = palette.color(QPalette::Text);
  const QColor highlight = palette.color(QPalette::Highlight);
  const QColor highlightedText = palette.color(QPalette::HighlightedText);

  const bool isDark = window.lightness() < 128;
  const QColor contrast = isDark ? QColor(Qt::white) : QColor(Qt::black);

  const QString surface = blend(window, contrast, 0.04);
  const QString surfaceRaised = blend(window, contrast, 0.09);
  const QString border = blend(window, contrast, 0.16);
  const QString hover = blend(window, contrast, 0.13);
  const QString mutedText = blend(windowText, window, 0.35);

  QString sheet =
      QStringLiteral(
          "QMenuBar {"
          "  background-color: %1;"
          "  color: %2;"
          "  border: none;"
          "  border-bottom: 1px solid %3;"
          "  padding: 4px 6px;"
          "  spacing: 2px;"
          "}"
          "QMenuBar::item {"
          "  background: transparent;"
          "  padding: 6px 12px;"
          "  margin: 0px 1px;"
          "  border-radius: 6px;"
          "}"
          "QMenuBar::item:selected {"
          "  background-color: %4;"
          "}"
          "QMenuBar::item:pressed {"
          "  background-color: %5;"
          "  color: %6;"
          "}"
          "QMenu {"
          "  background-color: %7;"
          "  color: %8;"
          "  border: 1px solid %3;"
          "  border-radius: 8px;"
          "  padding: 6px;"
          "}"
          "QMenu::item {"
          "  color: %8;"
          "  padding: 7px 28px 7px 14px;"
          "  border-radius: 6px;"
          "  margin: 1px 2px;"
          "}"
          "QMenu::item:selected {"
          "  background-color: %5;"
          "  color: %6;"
          "}"
          "QMenu::item:disabled {"
          "  color: %9;"
          "}"
          "QMenu::separator {"
          "  height: 1px;"
          "  background: %3;"
          "  margin: 5px 8px;"
          "}"
          "QMenu::icon {"
          "  padding-left: 8px;"
          "}"
          "QToolBar {"
          "  background-color: %1;"
          "  border: none;"
          "  border-bottom: 1px solid %3;"
          "  padding: 4px 6px;"
          "  spacing: 4px;"
          "}"
          "QToolBar::separator {"
          "  width: 1px;"
          "  background: %3;"
          "  margin: 4px 6px;"
          "}"
          "QToolButton {"
          "  background: transparent;"
          "  color: %2;"
          "  border: 1px solid transparent;"
          "  border-radius: 6px;"
          "  padding: 5px 10px;"
          "}"
          "QToolButton:hover {"
          "  background-color: %4;"
          "  border-color: %3;"
          "}"
          "QToolButton:pressed, QToolButton:checked {"
          "  background-color: %1;"
          "}"
          "QToolButton[popupMode=\"1\"] {"
          "  padding-right: 20px;"
          "}"
          "QToolButton::menu-button {"
          "  border: none;"
          "  background: transparent;"
          "  border-top-right-radius: 6px;"
          "  border-bottom-right-radius: 6px;"
          "  width: 18px;"
          "}"
          "QToolButton::menu-button:hover {"
          "  background-color: %4;"
          "}"
          "QPushButton {"
          "  background-color: %1;"
          "  color: %2;"
          "  border: 1px solid %3;"
          "  border-radius: 6px;"
          "  padding: 6px 14px;"
          "}"
          "QPushButton:hover {"
          "  background-color: %4;"
          "}"
          "QPushButton:pressed {"
          "  background-color: %5;"
          "  color: %6;"
          "}"
          "QPushButton:disabled {"
          "  color: %9;"
          "  background-color: %7;"
          "}"
          "QLineEdit, QTextEdit, QPlainTextEdit {"
          "  color: %8;"
          "  background-color: %11;"
          "  border: 1px solid %3;"
          "  border-radius: 6px;"
          "  padding: 5px 8px;"
          "  selection-background-color: %5;"
          "  selection-color: %6;"
          "}"
          "QLineEdit:focus, QTextEdit:focus, QPlainTextEdit:focus {"
          "  border-color: %5;"
          "}"
          "QAbstractItemView {"
          "  background-color: %11;"
          "  alternate-background-color: %7;"
          "  color: %8;"
          "  border: 1px solid %3;"
          "  border-radius: 8px;"
          "  outline: none;"
          "  selection-background-color: %5;"
          "  selection-color: %6;"
          "}"
          "QTreeView, QTableView, QListView {"
          "  show-decoration-selected: 1;"
          "}"
          "QTreeView::item, QListView::item {"
          "  border: none;"
          "  padding: 4px 6px;"
          "  border-radius: 4px;"
          "}"
          "QTreeView::item:hover, QListView::item:hover,"
          "QTableView::item:hover {"
          "  background-color: %4;"
          "}"
          "QTreeView::item:selected, QListView::item:selected,"
          "QTableView::item:selected {"
          "  background-color: %5;"
          "  color: %6;"
          "}"
          "QTreeView::branch {"
          "  background: transparent;"
          "}"
          "QComboBox {"
          "  background-color: %11;"
          "  color: %8;"
          "  border: 1px solid %3;"
          "  border-radius: 6px;"
          "  padding: 5px 8px;"
          "  min-height: 18px;"
          "}"
          "QComboBox:hover {"
          "  border-color: %5;"
          "}"
          "QComboBox::drop-down {"
          "  subcontrol-origin: padding;"
          "  subcontrol-position: center right;"
          "  border: none;"
          "  background: transparent;"
          "  width: 22px;"
          "}"
          "QComboBox QAbstractItemView {"
          "  background-color: %7;"
          "  border: 1px solid %3;"
          "  border-radius: 8px;"
          "  padding: 5px;"
          "}"
          "QComboBox QAbstractItemView::item {"
          "  border-radius: 5px;"
          "  padding: 5px 8px;"
          "  min-height: 20px;"
          "}"
          "QCheckBox, QRadioButton {"
          "  spacing: 7px;"
          "  padding: 2px 0px;"
          "}"
          "QCheckBox::indicator, QRadioButton::indicator {"
          "  width: 15px;"
          "  height: 15px;"
          "  border: 1px solid %3;"
          "  background-color: %11;"
          "}"
          "QCheckBox::indicator {"
          "  border-radius: 4px;"
          "}"
          "QRadioButton::indicator {"
          "  border-radius: 8px;"
          "}"
          "QCheckBox::indicator:hover, QRadioButton::indicator:hover {"
          "  border-color: %5;"
          "}"
          "QCheckBox::indicator:checked, QRadioButton::indicator:checked {"
          "  background-color: %5;"
          "  border-color: %5;"
          "}"
          "QGroupBox {"
          "  border: 1px solid %3;"
          "  border-radius: 8px;"
          "  margin-top: 12px;"
          "  padding: 8px 6px 6px 6px;"
          "}"
          "QGroupBox::title {"
          "  subcontrol-origin: margin;"
          "  subcontrol-position: top left;"
          "  left: 10px;"
          "  padding: 0px 5px;"
          "  color: %9;"
          "}"
          "QTabBar::tab {"
          "  background: %7;"
          "  color: %9;"
          "  border: 1px solid %3;"
          "  border-bottom: none;"
          "  border-top-left-radius: 7px;"
          "  border-top-right-radius: 7px;"
          "  padding: 6px 14px;"
          "  margin-right: 2px;"
          "}"
          "QTabBar::tab:selected {"
          "  background: %11;"
          "  color: %8;"
          "}"
          "QTabBar::tab:hover {"
          "  background: %4;"
          "}"
          "QHeaderView::section {"
          "  background-color: %1;"
          "  color: %2;"
          "  border: none;"
          "  border-right: 1px solid %3;"
          "  border-bottom: 1px solid %3;"
          "  padding: 6px 8px;"
          "}"
          "QSplitter::handle {"
          "  background: %3;"
          "}"
          "QSplitter::handle:hover {"
          "  background: %5;"
          "}"
          "QStatusBar {"
          "  background-color: %1;"
          "  border-top: 1px solid %3;"
          "  color: %9;"
          "}"
          "QScrollBar:vertical {"
          "  background: transparent;"
          "  width: 11px;"
          "  margin: 0px;"
          "}"
          "QScrollBar:horizontal {"
          "  background: transparent;"
          "  height: 11px;"
          "  margin: 0px;"
          "}"
          "QScrollBar::handle {"
          "  background: %3;"
          "  border-radius: 5px;"
          "  min-height: 28px;"
          "  min-width: 28px;"
          "}"
          "QScrollBar::handle:hover {"
          "  background: %4;"
          "}"
          "QScrollBar::add-line, QScrollBar::sub-line {"
          "  height: 0px;"
          "  width: 0px;"
          "}"
          "QScrollBar::add-page, QScrollBar::sub-page {"
          "  background: transparent;"
          "}"
          "QToolTip {"
          "  background-color: %7;"
          "  color: %8;"
          "  border: 1px solid %3;"
          "  border-radius: 6px;"
          "  padding: 5px 8px;"
          "}"
          "QMainWindow::separator {"
          "  background: %3;"
          "  width: 3px;"
          "  height: 3px;"
          "}")
          .arg(surface, windowText.name(), border, hover, highlight.name(),
               highlightedText.name(), surfaceRaised, text.name(), mutedText)
          .arg(base.name());

  const QFont menu = menuFont();
  sheet += QStringLiteral("QMenuBar, QMenuBar::item, QMenu, QMenu::item {"
                          "  font-family: \"%1\";"
                          "  font-size: %2pt;"
                          "}")
               .arg(menu.family())
               .arg(menu.pointSizeF());

  const QColor arrowColor = QColor(mutedText);
  const QString down = chevronGlyph(Chevron::Down, arrowColor);
  const QString up = chevronGlyph(Chevron::Up, arrowColor);
  const QString right = chevronGlyph(Chevron::Right, arrowColor);
  if (down.isEmpty() || up.isEmpty() || right.isEmpty())
    return sheet;

  sheet +=
      QStringLiteral("QComboBox::down-arrow, QToolButton::menu-indicator,"
                     "QHeaderView::down-arrow, QAbstractSpinBox::down-arrow {"
                     "  image: url(\"%1\");"
                     "  width: 16px;"
                     "  height: 16px;"
                     "}"
                     "QComboBox::down-arrow:on {"
                     "  image: url(\"%2\");"
                     "}"
                     "QHeaderView::up-arrow, QAbstractSpinBox::up-arrow {"
                     "  image: url(\"%2\");"
                     "  width: 16px;"
                     "  height: 16px;"
                     "}"
                     "QToolButton::menu-indicator {"
                     "  subcontrol-origin: padding;"
                     "  subcontrol-position: center right;"
                     "  right: 4px;"
                     "}"
                     "QToolButton::menu-arrow {"
                     "  image: url(\"%1\");"
                     "  width: 16px;"
                     "  height: 16px;"
                     "}"
                     "QMenu::right-arrow {"
                     "  image: url(\"%3\");"
                     "  width: 16px;"
                     "  height: 16px;"
                     "  margin-right: 6px;"
                     "}"
                     "QTreeView::branch:closed:has-children {"
                     "  image: url(\"%3\");"
                     "}"
                     "QTreeView::branch:open:has-children {"
                     "  image: url(\"%1\");"
                     "}")
          .arg(down, up, right);

  return sheet;
}

QString defaultUiFamily() {
  static const QStringList preferred = {
      QStringLiteral("Inter"),       QStringLiteral("Cantarell"),
      QStringLiteral("Noto Sans"),   QStringLiteral("Ubuntu"),
      QStringLiteral("DejaVu Sans"), QStringLiteral("Segoe UI")};
  return firstAvailable(preferred);
}

QString defaultMonospaceFamily() {
  static const QStringList preferred = {
      QStringLiteral("JetBrains Mono"), QStringLiteral("Fira Code"),
      QStringLiteral("Cascadia Code"),  QStringLiteral("Source Code Pro"),
      QStringLiteral("Noto Sans Mono"), QStringLiteral("DejaVu Sans Mono")};
  const QString family = firstAvailable(preferred);
  return family.isEmpty() ? QStringLiteral("monospace") : family;
}

QFont uiFont() {
  const QSettings settings(QStringLiteral("GitClientQt"),
                           QStringLiteral("GitClientQt"));
  QString family =
      settings.value(QStringLiteral("font/uiFamily")).toString().trimmed();
  if (family.isEmpty())
    family = defaultUiFamily();

  QFont font;
  if (!family.isEmpty())
    font.setFamily(family);
  font.setPointSizeF(
      settings.value(QStringLiteral("font/uiSize"), kDefaultUiFontSize)
          .toDouble());
  font.setHintingPreference(QFont::PreferFullHinting);
  return font;
}

QFont menuFont() {
  const QSettings settings(QStringLiteral("GitClientQt"),
                           QStringLiteral("GitClientQt"));
  QString family =
      settings.value(QStringLiteral("font/menuFamily")).toString().trimmed();
  if (family.isEmpty())
    family = defaultUiFamily();

  QFont font;
  if (!family.isEmpty())
    font.setFamily(family);
  font.setPointSizeF(
      settings.value(QStringLiteral("font/menuSize"), kDefaultMenuFontSize)
          .toDouble());
  font.setHintingPreference(QFont::PreferFullHinting);
  return font;
}

QFont monospaceFont(int fallbackPointSize) {
  const QSettings settings(QStringLiteral("GitClientQt"),
                           QStringLiteral("GitClientQt"));
  QString family =
      settings.value(QStringLiteral("font/monoFamily")).toString().trimmed();
  if (family.isEmpty())
    family = defaultMonospaceFamily();

  QFont font;
  font.setStyleHint(QFont::Monospace);
  font.setFamily(family);
  font.setPointSize(
      settings.value(QStringLiteral("font/monoSize"), fallbackPointSize)
          .toInt());
  return font;
}

} // namespace Theme
