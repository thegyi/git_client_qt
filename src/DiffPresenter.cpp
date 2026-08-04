#include "DiffPresenter.h"

#include <QRegularExpression>
#include <QRegularExpressionMatch>

DiffPresenter::DiffPresenter(QObject *parent) : QObject(parent) {}

QString DiffPresenter::formatDiff(const QStringList &lines,
                                  bool includeHunkLinks,
                                  bool unstageLink) const {
  QString html = QStringLiteral(
      "<html>"
      "<body style=\"background-color:#1e1e1e\">"
      "<table width=\"100%\" cellspacing=\"0\" cellpadding=\"0\">");

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
              ? QStringLiteral("<a href=\"git:hunk:%1\" "
                               "style=\"color:#4fc3f7; "
                               "text-decoration:none; "
                               "float:right; padding-right:6px;\">%2</a>")
                    .arg(QString::number(hunkIndex++),
                         unstageLink ? tr("unstage hunk") : tr("stage hunk"))
              : QString();
      html += QStringLiteral(
                  "<tr><td colspan=\"2\" "
                  "style=\"background-color:#3c3c3c; color:#aaaaaa;\">") +
              link + QStringLiteral("<pre style=\"margin:0\">") +
              line.toHtmlEscaped() + QStringLiteral("</pre></td></tr>");
      continue;
    }

    if (line.startsWith(QStringLiteral("diff --git")) ||
        line.startsWith(QStringLiteral("index ")) ||
        line.startsWith(QStringLiteral("--- ")) ||
        line.startsWith(QStringLiteral("+++ "))) {
      html +=
          QStringLiteral("<tr><td colspan=\"2\" "
                         "style=\"background-color:#3c3c3c; color:#aaaaaa;\">"
                         "<pre style=\"margin:0\">") +
          line.toHtmlEscaped() + QStringLiteral("</pre></td></tr>");
      continue;
    }

    QString bg;
    QString fg;
    QString lineNum = QStringLiteral("&nbsp;");
    const QString content = line.toHtmlEscaped();

    if (line.startsWith('+') && !line.startsWith(QStringLiteral("+++ "))) {
      bg = QStringLiteral("#1e4d2b");
      fg = QStringLiteral("#d4edda");
      lineNum = QString::number(newLine++);
    } else if (line.startsWith('-') &&
               !line.startsWith(QStringLiteral("--- "))) {
      bg = QStringLiteral("#4d1e1e");
      fg = QStringLiteral("#f8d7da");
      lineNum = QString::number(oldLine++);
    } else {
      bg = QStringLiteral("#2b2b2b");
      fg = QStringLiteral("#cccccc");
      if (newLine >= 0)
        lineNum = QString::number(newLine++);
      else if (oldLine >= 0)
        lineNum = QString::number(oldLine++);
    }

    html += QStringLiteral("<tr>"
                           "<td align=\"right\" "
                           "style=\"width:45px; background-color:#2b2b2b; "
                           "color:#888888;\">"
                           "<pre style=\"margin:0\">") +
            lineNum +
            QStringLiteral("</pre></td>"
                           "<td style=\"background-color:") +
            bg + QStringLiteral("; color:") + fg +
            QStringLiteral("; padding-left:4px;\">"
                           "<pre style=\"margin:0\">") +
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
