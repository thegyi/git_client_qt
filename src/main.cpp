#include <QApplication>
#include <QByteArray>
#include <QDir>
#include <QPalette>
#include <QSettings>
#include <QStyleFactory>
#include <QVector>

#include "MainWindow.h"
#include "Theme.h"

static void applyTheme() {
  QSettings settings("GitClientQt", "GitClientQt");
  const QString mode =
      settings.value("theme/mode", QStringLiteral("dark")).toString();

  QPalette palette;
  if (mode == QLatin1String("light")) {
    palette.setColor(QPalette::Window, QColor(0xf0, 0xf0, 0xf0));
    palette.setColor(QPalette::WindowText, QColor(0x00, 0x00, 0x00));
    palette.setColor(QPalette::Base, QColor(0xff, 0xff, 0xff));
    palette.setColor(QPalette::AlternateBase, QColor(0xf5, 0xf5, 0xf5));
    palette.setColor(QPalette::Text, QColor(0x00, 0x00, 0x00));
    palette.setColor(QPalette::Button, QColor(0xe0, 0xe0, 0xe0));
    palette.setColor(QPalette::ButtonText, QColor(0x00, 0x00, 0x00));
    palette.setColor(QPalette::Highlight, QColor(0x00, 0x78, 0xd7));
    palette.setColor(QPalette::HighlightedText, QColor(0xff, 0xff, 0xff));
    palette.setColor(QPalette::Link, QColor(0x00, 0x00, 0xff));
    palette.setColor(QPalette::ToolTipBase, QColor(0xff, 0xff, 0xff));
    palette.setColor(QPalette::ToolTipText, QColor(0x00, 0x00, 0x00));
  } else if (mode == QLatin1String("dark")) {
    palette.setColor(QPalette::Window, QColor(0x1e, 0x1e, 0x1e));
    palette.setColor(QPalette::WindowText, QColor(0xd4, 0xd4, 0xd4));
    palette.setColor(QPalette::Base, QColor(0x25, 0x25, 0x26));
    palette.setColor(QPalette::AlternateBase, QColor(0x2d, 0x2d, 0x30));
    palette.setColor(QPalette::Text, QColor(0xd4, 0xd4, 0xd4));
    palette.setColor(QPalette::Button, QColor(0x3c, 0x3c, 0x3c));
    palette.setColor(QPalette::ButtonText, QColor(0xd4, 0xd4, 0xd4));
    palette.setColor(QPalette::Highlight, QColor(0x09, 0x47, 0x71));
    palette.setColor(QPalette::HighlightedText, QColor(0xff, 0xff, 0xff));
    palette.setColor(QPalette::Link, QColor(0x37, 0x94, 0xff));
    palette.setColor(QPalette::ToolTipBase, QColor(0x1e, 0x1e, 0x1e));
    palette.setColor(QPalette::ToolTipText, QColor(0xd4, 0xd4, 0xd4));
  } else { // custom
    const QVector<QPair<QString, QPalette::ColorRole>> roles = {
        {QStringLiteral("Window"), QPalette::Window},
        {QStringLiteral("WindowText"), QPalette::WindowText},
        {QStringLiteral("Base"), QPalette::Base},
        {QStringLiteral("AlternateBase"), QPalette::AlternateBase},
        {QStringLiteral("Text"), QPalette::Text},
        {QStringLiteral("Button"), QPalette::Button},
        {QStringLiteral("ButtonText"), QPalette::ButtonText},
        {QStringLiteral("Highlight"), QPalette::Highlight},
        {QStringLiteral("HighlightedText"), QPalette::HighlightedText},
        {QStringLiteral("Link"), QPalette::Link},
        {QStringLiteral("ToolTipBase"), QPalette::ToolTipBase},
        {QStringLiteral("ToolTipText"), QPalette::ToolTipText}};
    for (const auto &p : roles) {
      const QColor color =
          settings.value(QLatin1String("theme/palette/") + p.first)
              .value<QColor>();
      if (color.isValid())
        palette.setColor(QPalette::Active, p.second, color);
    }
  }
  qApp->setStyle(QStyleFactory::create("Fusion"));
  qApp->setPalette(palette);
  qApp->setFont(Theme::uiFont());
  qApp->setStyleSheet(Theme::buildStyleSheet(palette));
}

#ifdef Q_OS_UNIX
static void loadKdeCursorSettings() {
  if (qgetenv("XCURSOR_THEME").isEmpty() || qgetenv("XCURSOR_SIZE").isEmpty()) {
    const QString kcminputrc =
        QDir::homePath() + QStringLiteral("/.config/kcminputrc");
    const QSettings inputSettings(kcminputrc, QSettings::IniFormat);

    if (qgetenv("XCURSOR_THEME").isEmpty()) {
      const QString theme =
          inputSettings.value(QStringLiteral("Mouse/cursorTheme")).toString();
      if (!theme.isEmpty())
        qputenv("XCURSOR_THEME", theme.toLocal8Bit());
    }

    if (qgetenv("XCURSOR_SIZE").isEmpty()) {
      const int size =
          inputSettings.value(QStringLiteral("Mouse/cursorSize"), -1).toInt();
      if (size > 0)
        qputenv("XCURSOR_SIZE", QByteArray::number(size));
    }
  }
}
#endif

int main(int argc, char *argv[]) {
#ifdef Q_OS_UNIX
  const QByteArray sessionType = qgetenv("XDG_SESSION_TYPE");
  if (sessionType == "wayland" && qgetenv("QT_QPA_PLATFORM").isEmpty())
    qputenv("QT_QPA_PLATFORM", "wayland");

  loadKdeCursorSettings();
#endif

  QApplication app(argc, argv);
  applyTheme();
  MainWindow window;
  window.show();
  return app.exec();
}
