#include "DiffPresenter.h"

#include <QRegularExpression>
#include <QRegularExpressionMatch>

DiffPresenter::DiffPresenter(QObject *parent) : QObject(parent) {}

void DiffPresenter::setMonospaceFont(const QString &family, int pointSize) {
  m_fontFamily = family;
  m_fontSize = pointSize;
}

QString DiffPresenter::formatDiff(const QStringList &lines,
                                  bool includeHunkLinks,
                                  bool unstageLink) const {
  const int fontSize = qMax(8, m_fontSize > 0 ? m_fontSize : 10);
  const QString fontFamily =
      m_fontFamily.isEmpty() ? QStringLiteral("monospace") : m_fontFamily;

  const QString baseColor = QStringLiteral("#1e1e1e");
  const QString textColor = QStringLiteral("#cccccc");
  const QString gutterColor = QStringLiteral("#2b2b2b");
  const QString gutterTextColor = QStringLiteral("#888888");
  const QString headerBg = QStringLiteral("#3c3c3c");
  const QString headerText = QStringLiteral("#aaaaaa");
  const QString addBg = QStringLiteral("#1e4d2b");
  const QString addText = QStringLiteral("#d4edda");
  const QString delBg = QStringLiteral("#4d1e1e");
  const QString delText = QStringLiteral("#f8d7da");
  const QString linkColor = QStringLiteral("#4fc3f7");

  const QString bodyStyle =
      QStringLiteral(
          "background-color:%1; color:%2; font-family:'%3',monospace; "
          "font-size:%4pt")
          .arg(baseColor, textColor, fontFamily, QString::number(fontSize));

  const QString headerStyle =
      QStringLiteral("background-color:%1; color:%2").arg(headerBg, headerText);

  const QString gutterStyle =
      QStringLiteral("background-color:%1; color:%2; text-align:right; "
                     "width:45px; border-right:1px solid #3c3c3c")
          .arg(gutterColor, gutterTextColor);

  const QString hunkLinkStyle =
      QStringLiteral("color:%1; text-decoration:none; float:right; "
                     "padding-right:6px")
          .arg(linkColor);

  QString html =
      QStringLiteral(
          "<html>"
          "<body style=\"%1\">"
          "<table width=\"100%\" cellspacing=\"0\" cellpadding=\"0\">")
          .arg(bodyStyle);

  int oldLine = -1;
  int newLine = -1;
  int hunkIndex = 0;
  QRegularExpression hunkRe(
      QStringLiteral("@@ -(\\d+)(?:,\\d+)? [+](\\d+)(?:,\\d+)? @@"));

  for (const QString &line : lines) {
    if (line.startsWith(QStringLiteral("@@"))) {
      const QRegularExpressionMatch m = hunkRe.match(line);
      if (m.hasMatch()) {
        oldLine = m.captured(1).toInt();
        newLine = m.captured(2).toInt();
      }
      const QString link =
          includeHunkLinks
              ? QStringLiteral("<a href=\"git:hunk:%1\" style=\"%2\">%3</a>")
                    .arg(QString::number(hunkIndex++), hunkLinkStyle,
                         unstageLink ? tr("unstage hunk") : tr("stage hunk"))
              : QString();
      html += QStringLiteral("<tr><td colspan=\"3\" style=\"%1\">")
                  .arg(headerStyle) +
              link + QStringLiteral("<pre style=\"margin:0\">") +
              line.toHtmlEscaped() + QStringLiteral("</pre></td></tr>");
      continue;
    }

    if (line.startsWith(QStringLiteral("diff --git")) ||
        line.startsWith(QStringLiteral("index ")) ||
        line.startsWith(QStringLiteral("--- ")) ||
        line.startsWith(QStringLiteral("+++ "))) {
      html += QStringLiteral("<tr><td colspan=\"3\" style=\"%1\">")
                  .arg(headerStyle) +
              QStringLiteral("<pre style=\"margin:0\">") +
              line.toHtmlEscaped() + QStringLiteral("</pre></td></tr>");
      continue;
    }

    QString bg = baseColor;
    QString fg = textColor;
    QString oldLineNum = QStringLiteral("&nbsp;");
    QString newLineNum = QStringLiteral("&nbsp;");
    const QString content = line.toHtmlEscaped();

    if (line.startsWith('+') && !line.startsWith(QStringLiteral("+++ "))) {
      bg = addBg;
      fg = addText;
      newLineNum = QString::number(newLine++);
    } else if (line.startsWith('-') &&
               !line.startsWith(QStringLiteral("--- "))) {
      bg = delBg;
      fg = delText;
      oldLineNum = QString::number(oldLine++);
    } else {
      if (oldLine >= 0)
        oldLineNum = QString::number(oldLine++);
      if (newLine >= 0)
        newLineNum = QString::number(newLine++);
    }

    const QString lineStyle =
        QStringLiteral("background-color:%1; color:%2; padding-left:4px")
            .arg(bg, fg);

    html += QStringLiteral(
                "<tr>"
                "<td style=\"%1\" align=\"right\"><pre style=\"margin:0\">")
                .arg(gutterStyle) +
            oldLineNum +
            QStringLiteral(
                "</pre></td>"
                "<td style=\"%1\" align=\"right\"><pre style=\"margin:0\">")
                .arg(gutterStyle) +
            newLineNum +
            QStringLiteral("</pre></td>"
                           "<td style=\"%1\"><pre style=\"margin:0\">")
                .arg(lineStyle) +
            content + QStringLiteral("</pre></td></tr>");
  }

  html += QStringLiteral("</table></body></html>");
  return html;
}

bool DiffPresenter::isLfsPointer(const QStringList &lines) const {
  for (const QString &line : lines) {
    const QString trimmed = line.trimmed();
    if (trimmed.startsWith(
            QStringLiteral("version https://git-lfs.github.com/spec/v1")))
      return true;
  }
  return false;
}

QString DiffPresenter::lfsPointerHtml(const QStringList &lines) const {
  QString oid;
  QString size;
  for (const QString &raw : lines) {
    const QString line = raw.trimmed();
    if (line.startsWith(QStringLiteral("oid ")))
      oid = line.mid(4).trimmed();
    else if (line.startsWith(QStringLiteral("size ")))
      size = line.mid(5).trimmed();
  }
  return QStringLiteral(
             "<html>"
             "<body style=\"background-color:#1e1e1e; color:#cccccc; "
             "font-family:sans-serif; padding:16px;\">"
             "<h2>Git LFS pointer</h2>"
             "<p><b>OID:</b> %1</p>"
             "<p><b>Size:</b> %2 bytes</p>"
             "</body></html>")
      .arg(oid.toHtmlEscaped(), size.toHtmlEscaped());
}
