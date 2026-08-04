#include "DiffViewWidget.h"

DiffViewWidget::DiffViewWidget(QWidget *parent) : QTextBrowser(parent) {
  setReadOnly(true);
  setOpenLinks(false);
  setOpenExternalLinks(false);
}

void DiffViewWidget::setDiffHtml(const QString &html) { setHtml(html); }

void DiffViewWidget::showEmpty(const QString &title, const QString &message) {
  setHtml(
      QStringLiteral("<html>"
                     "<body style=\"background-color:#1e1e1e; color:#aaaaaa;\">"
                     "<div style=\"padding:20px; text-align:center;\">"
                     "<h3 style=\"margin:0 0 10px 0;\">%1</h3>"
                     "<p style=\"margin:0;\">%2</p>"
                     "</div>"
                     "</body>"
                     "</html>")
          .arg(title.toHtmlEscaped(), message.toHtmlEscaped()));
}

void DiffViewWidget::showError(const QString &message) {
  setHtml(
      QStringLiteral("<html>"
                     "<body style=\"background-color:#1e1e1e; color:#ff6b6b;\">"
                     "<div style=\"padding:20px; text-align:center;\">"
                     "<h3 style=\"margin:0 0 10px 0;\">%1</h3>"
                     "<p style=\"margin:0;\">%2</p>"
                     "</div>"
                     "</body>"
                     "</html>")
          .arg(tr("Error").toHtmlEscaped(), message.toHtmlEscaped()));
}
