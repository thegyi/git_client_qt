#ifndef THEME_H
#define THEME_H

#include <QFont>
#include <QPalette>
#include <QString>

namespace Theme {

constexpr int kDefaultUiFontSize = 10;
constexpr int kDefaultMenuFontSize = 10;
constexpr int kDefaultMonospaceFontSize = 10;

QString buildStyleSheet(const QPalette &palette);

QString defaultUiFamily();

QString defaultMonospaceFamily();

QFont uiFont();

QFont menuFont();

QFont monospaceFont(int fallbackPointSize = kDefaultMonospaceFontSize);

} // namespace Theme

#endif // THEME_H
