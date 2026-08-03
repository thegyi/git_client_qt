#include <QApplication>
#include <QByteArray>
#include <QDir>
#include <QSettings>

#include "MainWindow.h"

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
  MainWindow window;
  window.show();
  return app.exec();
}
