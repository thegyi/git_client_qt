#include "MainWindow.h"
#include "Theme.h"
#include "ui_MainWindow.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QColorDialog>
#include <QComboBox>
#include <QCursor>
#include <QDateTime>
#include <QDebug>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QFontComboBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QKeySequence>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPalette>
#include <QPlainTextEdit>
#include <QProcess>
#include <QProcessEnvironment>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QSpinBox>
#include <QSplitter>
#include <QStatusBar>
#include <QStyle>
#include <QStyleFactory>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QTextEdit>
#include <QTextStream>
#include <QToolButton>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QUrl>
#include <QVBoxLayout>

void MainWindow::showPreferences() {
  QSettings settings("GitClientQt", "GitClientQt");
  QDialog dlg(this);
  dlg.setWindowTitle(tr("Preferences"));
  dlg.setMinimumSize(550, 450);
  auto *mainLayout = new QVBoxLayout(&dlg);

  auto *tabs = new QTabWidget(&dlg);
  mainLayout->addWidget(tabs);

  auto *generalTab = new QWidget(&dlg);
  auto *generalLayout = new QFormLayout(generalTab);

  auto *pullModeCombo = new QComboBox(&dlg);
  pullModeCombo->addItem(tr("Pull (fast-forward if possible)"),
                         QStringLiteral("ffIfPossible"));
  pullModeCombo->addItem(tr("Pull (fast-forward only)"),
                         QStringLiteral("ffOnly"));
  pullModeCombo->addItem(tr("Pull (rebase)"), QStringLiteral("rebase"));
  pullModeCombo->addItem(tr("Fetch all"), QStringLiteral("fetchAll"));
  pullModeCombo->setCurrentIndex(pullModeCombo->findData(
      settings.value("pullMode", QStringLiteral("ffIfPossible"))));

  auto *reopenBox =
      new QCheckBox(tr("Reopen last repository on startup"), &dlg);
  reopenBox->setChecked(settings.value("reopenLastRepo", true).toBool());

  auto *gpgKeyEdit = new QLineEdit(&dlg);
  gpgKeyEdit->setText(settings.value("gpgSigningKey").toString());

  auto *protectedBranchesEdit = new QLineEdit(&dlg);
  protectedBranchesEdit->setPlaceholderText(tr("main, master, develop"));
  protectedBranchesEdit->setToolTip(
      tr("Force pushing after an amend is blocked on these branches."));
  protectedBranchesEdit->setText(
      settings
          .value(QStringLiteral("protectedBranches"),
                 QStringList{QStringLiteral("main"), QStringLiteral("master"),
                             QStringLiteral("develop")})
          .toStringList()
          .join(QStringLiteral(", ")));

  generalLayout->addRow(tr("Default pull mode:"), pullModeCombo);
  generalLayout->addWidget(reopenBox);
  generalLayout->addRow(tr("GPG key ID or email:"), gpgKeyEdit);
  generalLayout->addRow(tr("Protected branches:"), protectedBranchesEdit);
  tabs->addTab(generalTab, tr("General"));

  auto *toolsTab = new QWidget(&dlg);
  auto *toolsLayout = new QFormLayout(toolsTab);

  auto *editorEdit = new QLineEdit(&dlg);
  editorEdit->setPlaceholderText(tr("e.g., code -g"));
  editorEdit->setText(
      settings.value(QStringLiteral("external/editor")).toString());
  auto *diffToolEdit = new QLineEdit(&dlg);
  diffToolEdit->setPlaceholderText(
      tr("Tool name or command (overrides git diff.tool)"));
  diffToolEdit->setText(
      settings.value(QStringLiteral("external/diffTool")).toString());
  auto *mergeToolEdit = new QLineEdit(&dlg);
  mergeToolEdit->setPlaceholderText(
      tr("Tool name or command (overrides git merge.tool)"));
  mergeToolEdit->setText(
      settings.value(QStringLiteral("external/mergeTool")).toString());

  toolsLayout->addRow(tr("External editor command:"), editorEdit);
  toolsLayout->addRow(tr("Diff tool:"), diffToolEdit);
  toolsLayout->addRow(tr("Merge tool:"), mergeToolEdit);
  tabs->addTab(toolsTab, tr("External Tools"));

  auto *themeTab = new QWidget(&dlg);
  auto *themeLayout = new QVBoxLayout(themeTab);

  auto *themeCombo = new QComboBox(&dlg);
  themeCombo->addItem(tr("Dark"), QStringLiteral("dark"));
  themeCombo->addItem(tr("Light"), QStringLiteral("light"));
  themeCombo->addItem(tr("Custom"), QStringLiteral("custom"));
  themeCombo->setCurrentIndex(themeCombo->findData(
      settings.value("theme/mode", QStringLiteral("dark"))));
  themeLayout->addWidget(themeCombo);

  const QVector<QPair<QString, QPalette::ColorRole>> roleList = {
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

  auto *colorGrid = new QGridLayout();
  QVector<QPushButton *> colorButtons;
  for (int i = 0; i < roleList.size(); ++i) {
    auto *label = new QLabel(roleList[i].first + QLatin1Char(':'), &dlg);
    auto *btn = new QPushButton(&dlg);
    btn->setFlat(true);
    colorButtons.append(btn);
    colorGrid->addWidget(label, i, 0);
    colorGrid->addWidget(btn, i, 1);
  }
  themeLayout->addLayout(colorGrid);

  auto setButtonColor = [&](QPushButton *btn, const QColor &c) {
    btn->setProperty("color", c);
    btn->setText(c.name());
    btn->setStyleSheet(QStringLiteral("background-color: %1;").arg(c.name()));
  };

  auto loadColors = [&](const QPalette &pal) {
    for (int i = 0; i < roleList.size(); ++i) {
      const QColor c = pal.color(QPalette::Active, roleList[i].second);
      setButtonColor(colorButtons[i], c);
    }
  };

  loadColors(qApp->palette());

  connect(themeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), &dlg,
          [&, roleList, colorButtons](int) {
            const QString mode = themeCombo->currentData().toString();
            if (mode == QLatin1String("custom"))
              return;

            QPalette pal;
            if (mode == QLatin1String("dark")) {
              pal.setColor(QPalette::Window, QColor(0x1e, 0x1e, 0x1e));
              pal.setColor(QPalette::WindowText, QColor(0xd4, 0xd4, 0xd4));
              pal.setColor(QPalette::Base, QColor(0x25, 0x25, 0x26));
              pal.setColor(QPalette::AlternateBase, QColor(0x2d, 0x2d, 0x30));
              pal.setColor(QPalette::Text, QColor(0xd4, 0xd4, 0xd4));
              pal.setColor(QPalette::Button, QColor(0x3c, 0x3c, 0x3c));
              pal.setColor(QPalette::ButtonText, QColor(0xd4, 0xd4, 0xd4));
              pal.setColor(QPalette::Highlight, QColor(0x09, 0x47, 0x71));
              pal.setColor(QPalette::HighlightedText, QColor(0xff, 0xff, 0xff));
              pal.setColor(QPalette::Link, QColor(0x37, 0x94, 0xff));
              pal.setColor(QPalette::ToolTipBase, QColor(0x1e, 0x1e, 0x1e));
              pal.setColor(QPalette::ToolTipText, QColor(0xd4, 0xd4, 0xd4));
            } else { // light
              pal = QPalette();
            }
            loadColors(pal);
          });

  for (int i = 0; i < colorButtons.size(); ++i) {
    QPushButton *btn = colorButtons[i];
    connect(btn, &QPushButton::clicked, &dlg, [&, i, btn]() {
      const QColor current = btn->property("color").value<QColor>();
      const QColor c = QColorDialog::getColor(
          current, &dlg, tr("Select color for %1").arg(roleList[i].first));
      if (c.isValid()) {
        setButtonColor(btn, c);
        themeCombo->setCurrentIndex(
            themeCombo->findData(QStringLiteral("custom")));
      }
    });
  }

  auto *fontGroup = new QGroupBox(tr("Fonts"), &dlg);
  auto *fontLayout = new QFormLayout(fontGroup);

  auto *menuFontCombo = new QFontComboBox(&dlg);
  menuFontCombo->setCurrentFont(QFont(
      settings
          .value(QStringLiteral("font/menuFamily"), Theme::defaultUiFamily())
          .toString()));
  auto *menuFontSize = new QSpinBox(&dlg);
  menuFontSize->setRange(7, 24);
  menuFontSize->setSuffix(tr(" pt"));
  menuFontSize->setValue(
      settings
          .value(QStringLiteral("font/menuSize"), Theme::kDefaultMenuFontSize)
          .toInt());

  auto *uiFontCombo = new QFontComboBox(&dlg);
  uiFontCombo->setCurrentFont(QFont(
      settings.value(QStringLiteral("font/uiFamily"), Theme::defaultUiFamily())
          .toString()));
  auto *uiFontSize = new QSpinBox(&dlg);
  uiFontSize->setRange(7, 24);
  uiFontSize->setSuffix(tr(" pt"));
  uiFontSize->setValue(
      settings.value(QStringLiteral("font/uiSize"), Theme::kDefaultUiFontSize)
          .toInt());

  auto *monoFontCombo = new QFontComboBox(&dlg);
  monoFontCombo->setFontFilters(QFontComboBox::MonospacedFonts);
  monoFontCombo->setCurrentFont(
      QFont(settings
                .value(QStringLiteral("font/monoFamily"),
                       Theme::defaultMonospaceFamily())
                .toString()));
  auto *monoFontSize = new QSpinBox(&dlg);
  monoFontSize->setRange(7, 24);
  monoFontSize->setSuffix(tr(" pt"));
  monoFontSize->setValue(settings
                             .value(QStringLiteral("font/monoSize"),
                                    Theme::kDefaultMonospaceFontSize)
                             .toInt());

  fontLayout->addRow(tr("Menu font:"), menuFontCombo);
  fontLayout->addRow(tr("Menu size:"), menuFontSize);
  fontLayout->addRow(tr("Content font:"), uiFontCombo);
  fontLayout->addRow(tr("Content size:"), uiFontSize);
  fontLayout->addRow(tr("Monospace font:"), monoFontCombo);
  fontLayout->addRow(tr("Monospace size:"), monoFontSize);

  auto *resetFontsButton = new QPushButton(tr("Reset to defaults"), &dlg);
  connect(resetFontsButton, &QPushButton::clicked, &dlg, [=] {
    menuFontCombo->setCurrentFont(QFont(Theme::defaultUiFamily()));
    menuFontSize->setValue(Theme::kDefaultMenuFontSize);
    uiFontCombo->setCurrentFont(QFont(Theme::defaultUiFamily()));
    uiFontSize->setValue(Theme::kDefaultUiFontSize);
    monoFontCombo->setCurrentFont(QFont(Theme::defaultMonospaceFamily()));
    monoFontSize->setValue(Theme::kDefaultMonospaceFontSize);
  });
  fontLayout->addRow(QString(), resetFontsButton);

  themeLayout->addWidget(fontGroup);

  tabs->addTab(themeTab, tr("Theme"));

  auto *shortcutsTab = new QWidget(&dlg);
  auto *shortcutsLayout = new QVBoxLayout(shortcutsTab);
  auto *table = new QTableWidget(&dlg);
  table->setColumnCount(3);
  table->setHorizontalHeaderLabels({tr("Menu"), tr("Action"), tr("Shortcut")});
  table->horizontalHeader()->setStretchLastSection(true);
  table->setSelectionBehavior(QAbstractItemView::SelectRows);
  table->setEditTriggers(QAbstractItemView::NoEditTriggers);
  shortcutsLayout->addWidget(table);
  tabs->addTab(shortcutsTab, tr("Shortcuts"));

  QMap<QString, QAction *> actionMap;
  QList<QMenu *> menus;
  QSet<QMenu *> visited;
  for (QAction *top : menuBar()->actions()) {
    if (QMenu *m = top->menu())
      menus.append(m);
  }
  while (!menus.isEmpty()) {
    QMenu *menu = menus.takeFirst();
    if (visited.contains(menu))
      continue;
    visited.insert(menu);
    for (QAction *action : menu->actions()) {
      if (action->isSeparator())
        continue;
      if (QMenu *sub = action->menu()) {
        menus.append(sub);
      } else {
        QString menuName = menu->title();
        menuName.remove('&');
        QString text = action->text();
        text.remove('&');
        if (text.isEmpty())
          continue;
        QString key = menuName + QLatin1Char('/') + text;
        actionMap.insert(key, action);
      }
    }
  }

  table->setRowCount(actionMap.size());
  int row = 0;
  QMap<QString, QKeySequenceEdit *> shortcutEdits;
  for (auto it = actionMap.begin(); it != actionMap.end(); ++it) {
    const QString key = it.key();
    const int slash = key.indexOf(QLatin1Char('/'));
    const QString menuName = key.left(slash);
    const QString actionName = key.mid(slash + 1);
    table->setItem(row, 0, new QTableWidgetItem(menuName));
    table->setItem(row, 1, new QTableWidgetItem(actionName));
    auto *seqEdit = new QKeySequenceEdit(it.value()->shortcut(), &dlg);
    shortcutEdits.insert(key, seqEdit);
    table->setCellWidget(row, 2, seqEdit);
    ++row;
  }

  auto *buttons = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
  mainLayout->addWidget(buttons);
  connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

  if (dlg.exec() != QDialog::Accepted)
    return;

  const QString pullMode = pullModeCombo->currentData().toString();
  settings.setValue("pullMode", pullMode);
  settings.setValue("reopenLastRepo", reopenBox->isChecked());
  const QString themeMode = themeCombo->currentData().toString();
  settings.setValue("theme/mode", themeMode);

  QPalette newPalette;
  if (themeMode == QLatin1String("dark")) {
    newPalette.setColor(QPalette::Window, QColor(0x1e, 0x1e, 0x1e));
    newPalette.setColor(QPalette::WindowText, QColor(0xd4, 0xd4, 0xd4));
    newPalette.setColor(QPalette::Base, QColor(0x25, 0x25, 0x26));
    newPalette.setColor(QPalette::AlternateBase, QColor(0x2d, 0x2d, 0x30));
    newPalette.setColor(QPalette::Text, QColor(0xd4, 0xd4, 0xd4));
    newPalette.setColor(QPalette::Button, QColor(0x3c, 0x3c, 0x3c));
    newPalette.setColor(QPalette::ButtonText, QColor(0xd4, 0xd4, 0xd4));
    newPalette.setColor(QPalette::Highlight, QColor(0x09, 0x47, 0x71));
    newPalette.setColor(QPalette::HighlightedText, QColor(0xff, 0xff, 0xff));
    newPalette.setColor(QPalette::Link, QColor(0x37, 0x94, 0xff));
    newPalette.setColor(QPalette::ToolTipBase, QColor(0x1e, 0x1e, 0x1e));
    newPalette.setColor(QPalette::ToolTipText, QColor(0xd4, 0xd4, 0xd4));
  } else if (themeMode == QLatin1String("light")) {
    newPalette.setColor(QPalette::Window, QColor(0xf0, 0xf0, 0xf0));
    newPalette.setColor(QPalette::WindowText, QColor(0x00, 0x00, 0x00));
    newPalette.setColor(QPalette::Base, QColor(0xff, 0xff, 0xff));
    newPalette.setColor(QPalette::AlternateBase, QColor(0xf5, 0xf5, 0xf5));
    newPalette.setColor(QPalette::Text, QColor(0x00, 0x00, 0x00));
    newPalette.setColor(QPalette::Button, QColor(0xe0, 0xe0, 0xe0));
    newPalette.setColor(QPalette::ButtonText, QColor(0x00, 0x00, 0x00));
    newPalette.setColor(QPalette::Highlight, QColor(0x00, 0x78, 0xd7));
    newPalette.setColor(QPalette::HighlightedText, QColor(0xff, 0xff, 0xff));
    newPalette.setColor(QPalette::Link, QColor(0x00, 0x00, 0xff));
    newPalette.setColor(QPalette::ToolTipBase, QColor(0xff, 0xff, 0xff));
    newPalette.setColor(QPalette::ToolTipText, QColor(0x00, 0x00, 0x00));
  } else {
    for (int i = 0; i < roleList.size(); ++i) {
      const QColor c = colorButtons[i]->property("color").value<QColor>();
      if (c.isValid())
        newPalette.setColor(QPalette::Active, roleList[i].second, c);
    }
  }
  settings.setValue(QStringLiteral("font/menuFamily"),
                    menuFontCombo->currentFont().family());
  settings.setValue(QStringLiteral("font/menuSize"), menuFontSize->value());
  settings.setValue(QStringLiteral("font/uiFamily"),
                    uiFontCombo->currentFont().family());
  settings.setValue(QStringLiteral("font/uiSize"), uiFontSize->value());
  settings.setValue(QStringLiteral("font/monoFamily"),
                    monoFontCombo->currentFont().family());
  settings.setValue(QStringLiteral("font/monoSize"), monoFontSize->value());
  settings.sync();

  qApp->setStyle(QStyleFactory::create("Fusion"));
  qApp->setPalette(newPalette);
  qApp->setFont(Theme::uiFont());
  qApp->setStyleSheet(Theme::buildStyleSheet(newPalette));
  applyFonts();

  for (int i = 0; i < roleList.size(); ++i) {
    const QColor c = newPalette.color(QPalette::Active, roleList[i].second);
    if (c.isValid())
      settings.setValue(QLatin1String("theme/palette/") + roleList[i].first, c);
  }

  settings.setValue("gpgSigningKey", gpgKeyEdit->text().trimmed());

  settings.setValue(QStringLiteral("external/editor"),
                    editorEdit->text().trimmed());
  settings.setValue(QStringLiteral("external/diffTool"),
                    diffToolEdit->text().trimmed());
  settings.setValue(QStringLiteral("external/mergeTool"),
                    mergeToolEdit->text().trimmed());

  QStringList protectedBranches;
  for (const QString &part :
       protectedBranchesEdit->text().split(QLatin1Char(','))) {
    const QString trimmed = part.trimmed();
    if (!trimmed.isEmpty())
      protectedBranches << trimmed;
  }
  settings.setValue(QStringLiteral("protectedBranches"), protectedBranches);
  settings.sync();
  updateAmendWarning();

  for (auto it = actionMap.begin(); it != actionMap.end(); ++it) {
    QKeySequenceEdit *seqEdit = shortcutEdits.value(it.key());
    if (!seqEdit)
      continue;
    const QKeySequence seq = seqEdit->keySequence();
    it.value()->setShortcut(seq);
    settings.setValue(QLatin1String("shortcuts/") + it.key(), seq.toString());
  }

  if (m_pullButton && m_pullButton->menu()) {
    for (QAction *action : m_pullButton->menu()->actions()) {
      if (action->data().toString() == pullMode) {
        action->setChecked(true);
        break;
      }
    }
  }

  if (pullMode == QLatin1String("ffOnly")) {
    m_pullArgs.clear();
    m_pullArgs << "pull" << "--ff-only";
  } else if (pullMode == QLatin1String("rebase")) {
    m_pullArgs.clear();
    m_pullArgs << "pull" << "--rebase";
  } else if (pullMode == QLatin1String("fetchAll")) {
    m_pullArgs.clear();
    m_pullArgs << "fetch" << "--all";
  } else {
    m_pullArgs.clear();
    m_pullArgs << "pull";
  }

  updateRecentRepos();
}

void MainWindow::editGitignore() {
  if (m_currentPath.isEmpty()) {
    statusBar()->showMessage(tr("Open a repository first."), 0);
    return;
  }

  const QString gitignorePath = m_currentPath + QStringLiteral("/.gitignore");

  QDialog dlg(this);
  dlg.setWindowTitle(tr("Edit .gitignore"));
  dlg.setMinimumSize(500, 400);
  dlg.resize(700, 500);
  auto *layout = new QVBoxLayout(&dlg);
  auto *edit = new QTextEdit(&dlg);
  edit->setFont(Theme::monospaceFont(10));
  layout->addWidget(edit);

  QString content;
  QFile file(gitignorePath);
  if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    content = QString::fromUtf8(file.readAll());
    file.close();
  }
  edit->setPlainText(content);

  auto *buttons = new QDialogButtonBox(
      QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dlg);
  layout->addWidget(buttons);
  connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

  if (dlg.exec() == QDialog::Accepted) {
    QFile out(gitignorePath);
    if (out.open(QIODevice::WriteOnly | QIODevice::Text)) {
      out.write(edit->toPlainText().toUtf8());
      out.close();
      loadWorkingTree();
    } else {
      statusBar()->showMessage(tr("Could not write .gitignore."), 0);
    }
  }
}

void MainWindow::editGitattributes() {
  if (m_currentPath.isEmpty()) {
    statusBar()->showMessage(tr("Open a repository first."), 0);
    return;
  }

  const QString gitattributesPath =
      m_currentPath + QStringLiteral("/.gitattributes");

  QDialog dlg(this);
  dlg.setWindowTitle(tr("Edit .gitattributes"));
  dlg.setMinimumSize(500, 400);
  dlg.resize(700, 500);
  auto *layout = new QVBoxLayout(&dlg);
  auto *edit = new QTextEdit(&dlg);
  edit->setFont(Theme::monospaceFont(10));
  layout->addWidget(edit);

  QString content;
  QFile file(gitattributesPath);
  if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    content = QString::fromUtf8(file.readAll());
    file.close();
  }
  edit->setPlainText(content);

  auto *buttons = new QDialogButtonBox(
      QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dlg);
  layout->addWidget(buttons);
  connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

  if (dlg.exec() == QDialog::Accepted) {
    QFile out(gitattributesPath);
    if (out.open(QIODevice::WriteOnly | QIODevice::Text)) {
      out.write(edit->toPlainText().toUtf8());
      out.close();
      loadWorkingTree();
    } else {
      statusBar()->showMessage(tr("Could not write .gitattributes."), 0);
    }
  }
}

void MainWindow::showCommitHooksAndTemplates() {
  if (m_currentPath.isEmpty()) {
    statusBar()->showMessage(tr("Open a repository first."), 0);
    return;
  }

  QDialog dlg(this);
  dlg.setWindowTitle(tr("Commit hooks and templates"));
  dlg.setMinimumSize(700, 500);
  dlg.resize(800, 600);
  auto *mainLayout = new QVBoxLayout(&dlg);
  auto *tabs = new QTabWidget(&dlg);
  mainLayout->addWidget(tabs);

  auto *templateTab = new QWidget(tabs);
  auto *templateLayout = new QVBoxLayout(templateTab);
  auto *templatePathLayout = new QHBoxLayout();
  auto *templatePath = new QLineEdit(templateTab);
  auto *browseButton = new QPushButton(tr("Browse..."), templateTab);
  templatePathLayout->addWidget(templatePath);
  templatePathLayout->addWidget(browseButton);
  templateLayout->addLayout(templatePathLayout);
  auto *templateEdit = new QTextEdit(templateTab);
  templateEdit->setFont(Theme::monospaceFont(10));
  templateLayout->addWidget(templateEdit);
  tabs->addTab(templateTab, tr("Commit template"));

  auto *hooksTab = new QWidget(tabs);
  auto *hooksLayout = new QHBoxLayout(hooksTab);
  auto *hooksList = new QListWidget(hooksTab);
  auto *hookEdit = new QTextEdit(hooksTab);
  hookEdit->setFont(Theme::monospaceFont(10));
  auto *saveHookButton = new QPushButton(tr("Save hook"), hooksTab);
  auto *hookEditLayout = new QVBoxLayout();
  hookEditLayout->addWidget(hookEdit);
  hookEditLayout->addWidget(saveHookButton);
  hooksLayout->addWidget(hooksList, 1);
  hooksLayout->addLayout(hookEditLayout, 3);
  tabs->addTab(hooksTab, tr("Hooks"));

  auto *buttons = new QDialogButtonBox(
      QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dlg);
  mainLayout->addWidget(buttons);
  connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

  auto loadTemplate = [&](const QString &path) {
    templatePath->setText(path);
    QFile f(path);
    if (f.open(QIODevice::ReadOnly | QIODevice::Text))
      templateEdit->setPlainText(QString::fromUtf8(f.readAll()));
    else
      templateEdit->clear();
  };

  const QString configuredTemplate =
      m_gitExecutor->run(m_currentPath, {"config", "commit.template"}).value(0);
  if (!configuredTemplate.isEmpty())
    loadTemplate(configuredTemplate);
  else
    loadTemplate(m_currentPath + QStringLiteral("/.gitmessage"));

  const QString hooksPath = m_currentPath + QStringLiteral("/.git/hooks");
  QDir hooksDir(hooksPath);
  for (const QString &entry :
       hooksDir.entryList(QDir::Files | QDir::NoDotAndDotDot))
    hooksList->addItem(entry);

  auto loadHook = [&]() {
    const QString fileName =
        hooksList->currentItem() ? hooksList->currentItem()->text() : QString();
    if (fileName.isEmpty()) {
      hookEdit->clear();
      return;
    }
    QFile f(hooksDir.absoluteFilePath(fileName));
    if (f.open(QIODevice::ReadOnly | QIODevice::Text))
      hookEdit->setPlainText(QString::fromUtf8(f.readAll()));
    else
      hookEdit->clear();
  };

  connect(hooksList, &QListWidget::currentTextChanged, this,
          [&](const QString &) { loadHook(); });

  connect(browseButton, &QPushButton::clicked, this, [&]() {
    const QString file = QFileDialog::getOpenFileName(
        this, tr("Select commit template"), m_currentPath);
    if (!file.isEmpty())
      loadTemplate(file);
  });

  connect(saveHookButton, &QPushButton::clicked, this, [&]() {
    const QString fileName =
        hooksList->currentItem() ? hooksList->currentItem()->text() : QString();
    if (fileName.isEmpty())
      return;
    QFile f(hooksDir.absoluteFilePath(fileName));
    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
      f.write(hookEdit->toPlainText().toUtf8());
      f.close();
      statusBar()->showMessage(tr("Hook %1 saved").arg(fileName));
    } else {
      statusBar()->showMessage(tr("Could not write hook %1.").arg(fileName), 0);
    }
  });

  if (dlg.exec() == QDialog::Accepted) {
    const QString templatePathText = templatePath->text().trimmed();
    if (templatePathText.isEmpty()) {
      m_gitExecutor->exec(m_currentPath,
                          {"config", "--unset", "commit.template"});
      return;
    }

    QFile f(templatePathText);
    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
      f.write(templateEdit->toPlainText().toUtf8());
      f.close();
      m_gitExecutor->exec(m_currentPath,
                          {"config", "commit.template", templatePathText});
    } else {
      statusBar()->showMessage(tr("Could not write commit template."), 0);
    }
  }
}

void MainWindow::showRepositorySettings() {
  if (m_currentPath.isEmpty()) {
    statusBar()->showMessage(tr("Open a repository first."), 0);
    return;
  }

  QDialog dlg(this);
  dlg.setWindowTitle(tr("Repository Settings"));
  dlg.setMinimumWidth(560);
  auto *layout = new QVBoxLayout(&dlg);
  auto *form = new QFormLayout();
  form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

  auto *nameEdit = new QLineEdit(&dlg);
  auto *emailEdit = new QLineEdit(&dlg);
  auto *branchEdit = new QLineEdit(&dlg);
  auto *autocrlfCombo = new QComboBox(&dlg);
  autocrlfCombo->setEditable(true);
  autocrlfCombo->addItems({QString(), QStringLiteral("true"),
                           QStringLiteral("false"), QStringLiteral("input")});
  auto *diffToolEdit = new QLineEdit(&dlg);
  auto *mergeToolEdit = new QLineEdit(&dlg);
  auto *lfsUrlEdit = new QLineEdit(&dlg);
  auto *lfsLocksverifyBox =
      new QCheckBox(tr("Verify LFS locks before push"), &dlg);

  nameEdit->setText(
      m_gitExecutor->run(m_currentPath, {"config", "user.name"}).value(0));
  emailEdit->setText(
      m_gitExecutor->run(m_currentPath, {"config", "user.email"}).value(0));
  branchEdit->setText(
      m_gitExecutor->run(m_currentPath, {"config", "init.defaultBranch"})
          .value(0));
  autocrlfCombo->setCurrentText(
      m_gitExecutor->run(m_currentPath, {"config", "core.autocrlf"}).value(0));
  diffToolEdit->setText(
      m_gitExecutor->run(m_currentPath, {"config", "diff.tool"}).value(0));
  mergeToolEdit->setText(
      m_gitExecutor->run(m_currentPath, {"config", "merge.tool"}).value(0));
  lfsUrlEdit->setText(
      m_gitExecutor->run(m_currentPath, {"config", "lfs.url"}).value(0));
  const QString locksverify =
      m_gitExecutor->run(m_currentPath, {"config", "--bool", "lfs.locksverify"})
          .value(0);
  lfsLocksverifyBox->setChecked(locksverify == QLatin1String("true"));

  form->addRow(tr("User name:"), nameEdit);
  form->addRow(tr("User email:"), emailEdit);
  form->addRow(tr("Default branch:"), branchEdit);
  form->addRow(tr("Auto CRLF:"), autocrlfCombo);
  form->addRow(tr("Diff tool:"), diffToolEdit);
  form->addRow(tr("Merge tool:"), mergeToolEdit);
  form->addRow(tr("LFS URL:"), lfsUrlEdit);
  form->addRow(tr("LFS locks verify:"), lfsLocksverifyBox);
  layout->addLayout(form);

  auto *buttons = new QDialogButtonBox(
      QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dlg);
  layout->addWidget(buttons);
  connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

  if (dlg.exec() != QDialog::Accepted)
    return;

  auto setConfig = [this](const QString &key, const QString &value) {
    if (value.isEmpty()) {
      m_gitExecutor->exec(m_currentPath, {"config", "--local", "--unset", key});
    } else {
      m_gitExecutor->exec(m_currentPath, {"config", "--local", key, value});
    }
  };

  setConfig(QStringLiteral("user.name"), nameEdit->text());
  setConfig(QStringLiteral("user.email"), emailEdit->text());
  setConfig(QStringLiteral("init.defaultBranch"), branchEdit->text());
  setConfig(QStringLiteral("core.autocrlf"), autocrlfCombo->currentText());
  setConfig(QStringLiteral("diff.tool"), diffToolEdit->text());
  setConfig(QStringLiteral("merge.tool"), mergeToolEdit->text());
  setConfig(QStringLiteral("lfs.url"), lfsUrlEdit->text().trimmed());
  setConfig(QStringLiteral("lfs.locksverify"),
            lfsLocksverifyBox->isChecked() ? "true" : "false");

  statusBar()->showMessage(tr("Repository settings saved"));
}

void MainWindow::showHunkStaging(const QString &path, bool unstage) {
  if (m_currentPath.isEmpty())
    return;

  QStringList args = QStringList{"-C", m_currentPath, "diff"};
  if (unstage)
    args.append("--cached");
  args.append("--");
  args.append(path);

  QProcess p;
  p.start("git", args);
  if (!p.waitForFinished(10000) || p.exitCode() != 0)
    return;

  const QString raw = QString::fromLocal8Bit(p.readAllStandardOutput());
  if (raw.isEmpty())
    return;

  const QStringList lines = raw.split('\n');

  QList<int> hunkStarts;
  const QRegularExpression hunkRe(
      QStringLiteral("^@@ -(\\d+)(?:,(\\d+))? \\+(\\d+)(?:,(\\d+))? @@"));
  for (int i = 0; i < lines.size(); ++i) {
    if (hunkRe.match(lines[i]).hasMatch())
      hunkStarts.append(i);
  }

  if (hunkStarts.isEmpty()) {
    if (unstage)
      m_gitExecutor->exec(m_currentPath, {"reset", "HEAD", "--", path});
    else
      m_gitExecutor->exec(m_currentPath, {"add", "--", path});
    loadWorkingTree();
    return;
  }

  const int firstHunk = hunkStarts.first();
  QStringList headerLines;
  for (int i = 0; i < firstHunk; ++i)
    headerLines.append(lines[i]);

  struct Hunk {
    int start;
    int end;
  };
  QList<Hunk> hunks;
  for (int i = 0; i < hunkStarts.size(); ++i) {
    int start = hunkStarts[i];
    int end =
        (i + 1 < hunkStarts.size()) ? hunkStarts[i + 1] - 1 : lines.size() - 1;
    hunks.append({start, end});
  }

  QDialog dlg(this);
  dlg.setWindowTitle(unstage ? tr("Unstage hunks - %1").arg(path)
                             : tr("Stage hunks - %1").arg(path));
  auto *layout = new QVBoxLayout(&dlg);

  auto *list = new QListWidget(&dlg);
  list->setFont(Theme::monospaceFont(10));
  list->setMinimumHeight(80);

  auto *preview = new QTextEdit(&dlg);
  preview->setReadOnly(true);
  preview->setFont(Theme::monospaceFont(10));

  for (const Hunk &h : hunks) {
    QStringList hunkLines;
    for (int i = h.start; i <= h.end; ++i)
      hunkLines.append(lines[i]);
    auto *wi = new QListWidgetItem(lines[h.start], list);
    wi->setFlags(wi->flags() | Qt::ItemIsUserCheckable);
    wi->setCheckState(Qt::Checked);
    wi->setData(Qt::UserRole, hunkLines.join(QLatin1Char('\n')));
  }

  auto updatePreview = [this, list, preview]() {
    auto *wi = list->currentItem();
    if (!wi)
      return;
    preview->setHtml(m_diffPresenter->formatDiff(
        wi->data(Qt::UserRole).toString().split(QLatin1Char('\n'))));
  };
  connect(list, &QListWidget::currentItemChanged, this, updatePreview);
  list->setCurrentRow(0);
  if (!hunks.isEmpty()) {
    preview->setHtml(m_diffPresenter->formatDiff(
        lines.mid(hunks[0].start, hunks[0].end - hunks[0].start + 1)));
  }

  auto *splitter = new QSplitter(Qt::Vertical, &dlg);
  splitter->addWidget(list);
  splitter->addWidget(preview);
  splitter->setStretchFactor(0, 0);
  splitter->setStretchFactor(1, 1);
  layout->addWidget(splitter);

  auto *buttons = new QDialogButtonBox(
      QDialogButtonBox::Apply | QDialogButtonBox::Cancel, &dlg);
  layout->addWidget(buttons);
  connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

  if (dlg.exec() != QDialog::Accepted)
    return;

  QStringList patchLines = headerLines;
  for (int i = 0; i < list->count(); ++i) {
    QListWidgetItem *wi = list->item(i);
    if (wi->checkState() == Qt::Checked)
      patchLines.append(
          wi->data(Qt::UserRole).toString().split(QLatin1Char('\n')));
  }

  if (patchLines.size() == headerLines.size()) {
    statusBar()->showMessage(tr("Select at least one hunk to apply."), 0);
    return;
  }

  const QString patch = patchLines.join(QLatin1Char('\n')) + QLatin1Char('\n');

  QTemporaryFile tempFile;
  if (!tempFile.open()) {
    statusBar()->showMessage(tr("Could not create patch file."), 0);
    return;
  }
  tempFile.write(patch.toUtf8());
  tempFile.close();

  const QStringList applyArgs =
      unstage ? QStringList{"apply", "--cached", "-R", tempFile.fileName()}
              : QStringList{"apply", "--cached", tempFile.fileName()};
  if (m_gitExecutor->exec(m_currentPath, applyArgs)) {
    loadWorkingTree();
    statusBar()->showMessage(unstage ? tr("Hunks unstaged")
                                     : tr("Hunks staged"));
  } else {
    statusBar()->showMessage(unstage ? tr("Failed to unstage hunks")
                                     : tr("Failed to stage hunks"));
  }
}

void MainWindow::showInteractiveRebase(const QString &baseSha) {
  if (m_currentPath.isEmpty() || baseSha.isEmpty())
    return;

  const QStringList logLines = m_gitExecutor->run(
      m_currentPath, {"log", baseSha + QLatin1String("..HEAD"), "--reverse",
                      "--pretty=format:%H %s"});
  if (logLines.isEmpty()) {
    statusBar()->showMessage(tr("There are no commits after the selected one."),
                             0);
    return;
  }

  struct Commit {
    QString sha;
    QString message;
  };
  QList<Commit> commits;
  for (const QString &line : logLines) {
    const int sp = line.indexOf(QLatin1Char(' '));
    if (sp < 0)
      continue;
    commits.append({line.left(sp), line.mid(sp + 1)});
  }
  if (commits.isEmpty())
    return;

  QDialog dlg(this);
  dlg.setWindowTitle(tr("Interactive rebase"));
  auto *layout = new QVBoxLayout(&dlg);
  auto *table = new QTableWidget(commits.size(), 3, &dlg);
  table->setHorizontalHeaderLabels({tr("Action"), tr("Commit"), tr("Message")});
  table->setColumnWidth(0, 100);
  table->setColumnWidth(1, 260);
  table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
  table->verticalHeader()->setVisible(false);

  const QStringList actions = {QStringLiteral("pick"), QStringLiteral("reword"),
                               QStringLiteral("squash"),
                               QStringLiteral("fixup"), QStringLiteral("drop")};
  for (int i = 0; i < commits.size(); ++i) {
    auto *combo = new QComboBox(&dlg);
    for (const QString &a : actions)
      combo->addItem(tr(a.toLatin1().constData()));
    table->setCellWidget(i, 0, combo);
    auto *shaItem =
        new QTableWidgetItem(commits[i].sha.left(7) + QLatin1Char(' ') +
                             commits[i].message.left(40));
    shaItem->setFlags(shaItem->flags() & ~Qt::ItemIsEditable);
    table->setItem(i, 1, shaItem);
    auto *msgItem = new QTableWidgetItem(commits[i].message);
    table->setItem(i, 2, msgItem);
  }
  layout->addWidget(table);

  auto *buttons = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
  layout->addWidget(buttons);
  connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

  if (dlg.exec() != QDialog::Accepted)
    return;

  QTemporaryDir tempDir;
  if (!tempDir.isValid())
    return;

  int messageCount = 0;
  QStringList todoLines;
  for (int i = 0; i < commits.size(); ++i) {
    auto *combo = qobject_cast<QComboBox *>(table->cellWidget(i, 0));
    const QString action = combo ? combo->currentText() : tr("pick");
    const QString sha = commits[i].sha;
    const QString newMsg =
        table->item(i, 2) ? table->item(i, 2)->text() : commits[i].message;

    if (action == tr("reword") || action == tr("squash")) {
      QFile f(tempDir.filePath(QStringLiteral("msg_%1.txt").arg(messageCount)));
      if (f.open(QIODevice::WriteOnly | QIODevice::Text))
        f.write(newMsg.toUtf8());
      ++messageCount;
    }
    todoLines.append(action + QLatin1Char(' ') + sha + QLatin1Char(' ') +
                     (action == tr("drop") ? QString() : newMsg));
  }

  const QString todoFile = tempDir.filePath(QStringLiteral("todo.txt"));
  {
    QFile f(todoFile);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
      return;
    f.write(todoLines.join(QLatin1Char('\n')).toUtf8());
    f.write("\n", 1);
  }

  const QString seqEditor = tempDir.filePath(QStringLiteral("seq-editor.sh"));
  {
    QFile f(seqEditor);
    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
      f.write(QStringLiteral("#!/bin/sh\ncp \"%1\" \"$1\"\n")
                  .arg(todoFile)
                  .toUtf8());
      f.close();
      f.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
    }
  }

  const QString msgCounter = tempDir.filePath(QStringLiteral("counter.txt"));
  if (messageCount > 0) {
    QFile c(msgCounter);
    if (c.open(QIODevice::WriteOnly | QIODevice::Text))
      c.write("0");
  }

  const QString msgEditor = tempDir.filePath(QStringLiteral("msg-editor.sh"));
  if (messageCount > 0) {
    QFile f(msgEditor);
    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
      f.write(QStringLiteral("#!/bin/sh\n"
                             "COUNTER=\"%1\"\n"
                             "DIR=\"%2\"\n"
                             "IDX=$(cat \"$COUNTER\")\n"
                             "echo $((IDX+1)) > \"$COUNTER\"\n"
                             "cp \"$DIR/msg_${IDX}.txt\" \"$1\"\n")
                  .arg(msgCounter, tempDir.path())
                  .toUtf8());
      f.close();
      f.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
    }
  }

  QProcess p;
  QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
  env.insert(QStringLiteral("GIT_SEQUENCE_EDITOR"), seqEditor);
  if (messageCount > 0)
    env.insert(QStringLiteral("GIT_EDITOR"), msgEditor);
  p.setProcessEnvironment(env);
  p.start(QStringLiteral("git"),
          QStringList{QStringLiteral("-C"), m_currentPath,
                      QStringLiteral("rebase"), QStringLiteral("-i"), baseSha});
  if (!p.waitForStarted(5000) || !p.waitForFinished(120000)) {
    p.kill();
    p.waitForFinished(1000);
    statusBar()->showMessage(
        tr("Interactive rebase timed out or failed to start"));
    return;
  }

  const int code = p.exitCode();
  const QString output = QString::fromLocal8Bit(p.readAllStandardOutput() +
                                                p.readAllStandardError());
  if (code == 0) {
    loadRepository(m_currentPath);
    statusBar()->showMessage(tr("Interactive rebase completed"));
  } else {
    statusBar()->showMessage(
        output.isEmpty() ? tr("Interactive rebase failed") : output, 0);
  }
}

void MainWindow::showConflictResolver(const QString &operation) {
  if (m_currentPath.isEmpty())
    return;

  QDialog dlg(this);
  dlg.setWindowTitle(tr("Resolve conflicts"));
  auto *layout = new QVBoxLayout(&dlg);

  layout->addWidget(new QLabel(tr("Conflicted files:"), &dlg));
  auto *list = new QListWidget(&dlg);
  list->setSelectionMode(QAbstractItemView::SingleSelection);
  layout->addWidget(list);

  auto *btnLayout = new QHBoxLayout;
  auto *oursBtn = new QPushButton(tr("Use ours"), &dlg);
  auto *theirsBtn = new QPushButton(tr("Use theirs"), &dlg);
  auto *resolvedBtn = new QPushButton(tr("Mark resolved"), &dlg);
  auto *toolBtn = new QPushButton(tr("Open in merge tool"), &dlg);
  auto *continueBtn = new QPushButton(tr("Continue"), &dlg);
  auto *abortBtn = new QPushButton(tr("Abort"), &dlg);
  btnLayout->addWidget(oursBtn);
  btnLayout->addWidget(theirsBtn);
  btnLayout->addWidget(resolvedBtn);
  btnLayout->addWidget(toolBtn);
  btnLayout->addStretch();
  btnLayout->addWidget(continueBtn);
  btnLayout->addWidget(abortBtn);
  layout->addLayout(btnLayout);

  continueBtn->setVisible(!operation.isEmpty());
  abortBtn->setVisible(!operation.isEmpty());

  auto refresh = [&]() {
    list->clear();
    for (const QString &file : m_gitExecutor->run(
             m_currentPath, {"diff", "--name-only", "--diff-filter=U"}))
      list->addItem(file);
    const bool hasSelection = list->currentItem() != nullptr;
    const bool hasMergeTool =
        !m_gitExecutor->run(m_currentPath, {"config", "merge.tool"}).isEmpty();
    oursBtn->setEnabled(hasSelection);
    theirsBtn->setEnabled(hasSelection);
    resolvedBtn->setEnabled(hasSelection);
    toolBtn->setEnabled(hasSelection && hasMergeTool);
    continueBtn->setEnabled(!operation.isEmpty());
    abortBtn->setEnabled(!operation.isEmpty());
  };

  refresh();
  connect(list, &QListWidget::itemSelectionChanged, refresh);

  connect(oursBtn, &QPushButton::clicked, this, [&]() {
    auto *item = list->currentItem();
    if (!item)
      return;
    const QString file = item->text();
    m_gitExecutor->exec(m_currentPath, {"checkout", "--ours", "--", file});
    m_gitExecutor->exec(m_currentPath, {"add", "--", file});
    refresh();
  });

  connect(theirsBtn, &QPushButton::clicked, this, [&]() {
    auto *item = list->currentItem();
    if (!item)
      return;
    const QString file = item->text();
    m_gitExecutor->exec(m_currentPath, {"checkout", "--theirs", "--", file});
    m_gitExecutor->exec(m_currentPath, {"add", "--", file});
    refresh();
  });

  connect(resolvedBtn, &QPushButton::clicked, this, [&]() {
    auto *item = list->currentItem();
    if (!item)
      return;
    const QString file = item->text();
    m_gitExecutor->exec(m_currentPath, {"add", "--", file});
    refresh();
  });

  connect(toolBtn, &QPushButton::clicked, this, [&]() {
    auto *item = list->currentItem();
    if (!item)
      return;
    const QString file = item->text();
    launchGitTool({QStringLiteral("mergetool"), QStringLiteral("-y"),
                   QStringLiteral("--"), file},
                  false);
  });

  connect(continueBtn, &QPushButton::clicked, this, [&]() {
    QProcess p;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("GIT_EDITOR"), QStringLiteral("true"));
    p.setProcessEnvironment(env);
    p.start(QStringLiteral("git"),
            QStringList{QStringLiteral("-C"), m_currentPath, operation,
                        QStringLiteral("--continue")});
    if (p.waitForStarted(5000) && p.waitForFinished(120000) &&
        p.exitStatus() == QProcess::NormalExit && p.exitCode() == 0) {
      loadRepository(m_currentPath);
      dlg.accept();
      statusBar()->showMessage(tr("%1 completed").arg(operation));
    } else {
      statusBar()->showMessage(QString::fromLocal8Bit(p.readAllStandardError()),
                               0);
      refresh();
    }
  });

  connect(abortBtn, &QPushButton::clicked, this, [&]() {
    if (m_gitExecutor->exec(m_currentPath,
                            {operation, QStringLiteral("--abort")})) {
      loadRepository(m_currentPath);
      dlg.accept();
      statusBar()->showMessage(tr("%1 aborted").arg(operation));
    }
  });

  dlg.resize(600, 400);
  dlg.exec();
}

void MainWindow::showReflog() {
  if (m_currentPath.isEmpty()) {
    statusBar()->showMessage(tr("Open a repository first."), 0);
    return;
  }

  const QStringList raw =
      m_gitExecutor->run(m_currentPath, {QStringLiteral("reflog")});
  if (raw.isEmpty()) {
    statusBar()->showMessage(tr("Reflog is empty."), 0);
    return;
  }

  struct ReflogEntry {
    QString sha;
    QString ref;
    QString message;
  };
  QList<ReflogEntry> entries;
  for (const QString &line : raw) {
    const int firstSpace = line.indexOf(QLatin1Char(' '));
    if (firstSpace < 0)
      continue;
    const QString sha = line.left(firstSpace);
    const int colon = line.indexOf(QLatin1Char(':'), firstSpace);
    if (colon < 0)
      continue;
    const QString ref =
        line.mid(firstSpace + 1, colon - firstSpace - 1).trimmed();
    const QString message = line.mid(colon + 1).trimmed();
    entries.append({sha, ref, message});
  }

  QDialog dlg(this);
  dlg.setWindowTitle(tr("Reflog"));
  auto *layout = new QVBoxLayout(&dlg);
  auto *table = new QTableWidget(entries.size(), 4, &dlg);
  table->setHorizontalHeaderLabels(
      {tr("SHA"), tr("Ref"), tr("Message"), tr("Action")});
  table->horizontalHeader()->setSectionResizeMode(
      QHeaderView::ResizeToContents);
  table->horizontalHeader()->setStretchLastSection(false);
  table->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  table->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  table->setMinimumSize(100, 100);
  table->verticalHeader()->setVisible(false);

  for (int i = 0; i < entries.size(); ++i) {
    auto *shaItem = new QTableWidgetItem(entries[i].sha.left(7));
    shaItem->setData(Qt::UserRole, entries[i].sha);
    shaItem->setFlags(shaItem->flags() & ~Qt::ItemIsEditable);
    table->setItem(i, 0, shaItem);
    auto *refItem = new QTableWidgetItem(entries[i].ref);
    refItem->setFlags(refItem->flags() & ~Qt::ItemIsEditable);
    table->setItem(i, 1, refItem);
    auto *msgItem = new QTableWidgetItem(entries[i].message);
    msgItem->setFlags(msgItem->flags() & ~Qt::ItemIsEditable);
    table->setItem(i, 2, msgItem);
    auto *actionItem =
        new QTableWidgetItem(entries[i].message.split(' ').value(0));
    actionItem->setFlags(actionItem->flags() & ~Qt::ItemIsEditable);
    table->setItem(i, 3, actionItem);
  }
  layout->addWidget(table);

  table->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(table, &QTableWidget::customContextMenuRequested, this,
          [this, table, &dlg](const QPoint &pos) {
            auto *item = table->itemAt(pos);
            if (!item)
              return;
            const int row = item->row();
            const QString sha =
                table->item(row, 0)->data(Qt::UserRole).toString();
            QMenu menu(&dlg);
            auto *checkoutAction =
                menu.addAction(tr("Checkout %1").arg(sha.left(7)));
            auto *resetAction =
                menu.addAction(tr("Reset to %1").arg(sha.left(7)));
            auto *branchAction =
                menu.addAction(tr("Create branch from %1").arg(sha.left(7)));
            QAction *selected = menu.exec(table->mapToGlobal(pos));
            if (!selected)
              return;
            if (selected == checkoutAction) {
              if (m_gitExecutor->exec(m_currentPath, {"checkout", sha}))
                loadRepository(m_currentPath);
            } else if (selected == resetAction) {
              resetToCommit(sha);
            } else if (selected == branchAction) {
              bool ok;
              const QString name = QInputDialog::getText(
                  this, tr("Create Branch"), tr("Branch name:"),
                  QLineEdit::Normal, QString(), &ok);
              if (ok && !name.isEmpty()) {
                if (m_gitExecutor->exec(m_currentPath,
                                        {"checkout", "-b", name, sha}))
                  loadRepository(m_currentPath);
              }
            }
          });

  auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dlg);
  layout->addWidget(buttons);
  connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

  dlg.setFixedSize(900, 500);
  dlg.exec();
}

void MainWindow::showBlame(const QString &path, const QString &revision) {
  if (m_currentPath.isEmpty() || path.isEmpty())
    return;

  QStringList args = {QStringLiteral("blame"), QStringLiteral("--porcelain")};
  if (!revision.isEmpty())
    args.append(revision);
  args.append(QStringLiteral("--"));
  args.append(path);

  QDialog dlg(this);
  dlg.setWindowTitle(tr("Blame %1").arg(path));
  auto *layout = new QVBoxLayout(&dlg);
  auto *table = new QTableWidget(&dlg);
  table->setColumnCount(4);
  table->setHorizontalHeaderLabels(
      {tr("SHA"), tr("Summary"), tr("Date"), tr("Source")});
  table->horizontalHeader()->setVisible(false);
  table->verticalHeader()->setVisible(false);
  table->setShowGrid(false);
  table->setSelectionBehavior(QAbstractItemView::SelectRows);
  table->setEditTriggers(QAbstractItemView::NoEditTriggers);
  table->setFont(Theme::monospaceFont(10));
  table->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  layout->addWidget(table);

  QString currentSha;
  QString currentSummary;
  qint64 currentTime = 0;

  const QRegularExpression hunkRe(
      QStringLiteral("^(\\^?[0-9a-fA-F]{40}) (\\d+) (\\d+) (\\d+)$"));
  for (const QString &line : m_gitExecutor->run(m_currentPath, args)) {
    const QRegularExpressionMatch m = hunkRe.match(line);
    if (m.hasMatch()) {
      currentSha = m.captured(1);
      currentSha.remove(QLatin1Char('^'));
      currentSummary.clear();
      currentTime = 0;
    } else if (line.startsWith(QStringLiteral("author-time "))) {
      currentTime = line.mid(12).toLongLong();
    } else if (line.startsWith(QStringLiteral("summary "))) {
      currentSummary = line.mid(8);
    } else if (line.startsWith(QLatin1Char('\t'))) {
      const QString content = line.mid(1);
      const QString date = currentTime > 0
                               ? QDateTime::fromSecsSinceEpoch(currentTime)
                                     .toString(QStringLiteral("yyyy-MM-dd"))
                               : QString();

      const int row = table->rowCount();
      table->insertRow(row);

      auto *shaItem = new QTableWidgetItem(currentSha.left(7));
      shaItem->setBackground(QColor(0x25, 0x25, 0x26));
      shaItem->setForeground(QColor(0x9c, 0xdc, 0xfe));
      shaItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
      table->setItem(row, 0, shaItem);

      auto *summaryItem = new QTableWidgetItem(currentSummary.left(60));
      summaryItem->setToolTip(currentSummary);
      summaryItem->setBackground(QColor(0x25, 0x25, 0x26));
      summaryItem->setForeground(QColor(0xdc, 0xdc, 0xaa));
      summaryItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
      table->setItem(row, 1, summaryItem);

      auto *dateItem = new QTableWidgetItem(date);
      dateItem->setBackground(QColor(0x25, 0x25, 0x26));
      dateItem->setForeground(QColor(0x80, 0x80, 0x80));
      dateItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
      table->setItem(row, 2, dateItem);

      auto *contentItem = new QTableWidgetItem(content);
      contentItem->setBackground(QColor(0x1e, 0x1e, 0x1e));
      contentItem->setForeground(QColor(0xd4, 0xd4, 0xd4));
      contentItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
      table->setItem(row, 3, contentItem);
    }
  }

  auto *closeBtn = new QPushButton(tr("Close"), &dlg);
  connect(closeBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
  layout->addWidget(closeBtn);
  dlg.resize(1000, 700);

  table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
  table->setColumnWidth(0, 70);
  table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
  table->setColumnWidth(1, 150);
  table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
  table->setColumnWidth(2, 80);
  table->horizontalHeader()->setStretchLastSection(true);

  dlg.show();
  table->resizeRowsToContents();
  dlg.exec();
}

void MainWindow::showFileHistory(const QString &path) {
  if (m_currentPath.isEmpty() || path.isEmpty())
    return;

  QDialog dlg(this);
  dlg.setWindowTitle(tr("History of %1").arg(path));
  auto *layout = new QVBoxLayout(&dlg);
  auto *table = new QTableWidget(&dlg);
  table->setColumnCount(4);
  table->setHorizontalHeaderLabels(
      {tr("SHA"), tr("Date"), tr("Author"), tr("Subject")});
  table->setSelectionBehavior(QAbstractItemView::SelectRows);
  table->setEditTriggers(QAbstractItemView::NoEditTriggers);
  table->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  layout->addWidget(table);

  for (const QString &line : m_gitExecutor->run(
           m_currentPath,
           {QStringLiteral("log"), QStringLiteral("--follow"),
            QStringLiteral("--date=format:%Y-%m-%d %H:%M:%S"),
            QStringLiteral("--pretty=format:%H%x1f%h%x1f%an%x1f%ad%x1f%s%n"),
            QStringLiteral("--"), path})) {
    const QStringList fields = line.split(QChar(0x1f), Qt::SkipEmptyParts);
    if (fields.size() < 5)
      continue;
    const int row = table->rowCount();
    table->insertRow(row);

    auto *shaItem = new QTableWidgetItem(fields.at(1));
    shaItem->setData(Qt::UserRole, fields.at(0));
    shaItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    table->setItem(row, 0, shaItem);

    auto *dateItem = new QTableWidgetItem(fields.at(3));
    dateItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    table->setItem(row, 1, dateItem);

    auto *authorItem = new QTableWidgetItem(fields.at(2));
    authorItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    table->setItem(row, 2, authorItem);

    auto *subjectItem = new QTableWidgetItem(fields.at(4));
    subjectItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    table->setItem(row, 3, subjectItem);
  }

  connect(table, &QTableWidget::itemDoubleClicked, this,
          [&](QTableWidgetItem *item) {
            if (!item)
              return;
            const QString sha =
                table->item(item->row(), 0)->data(Qt::UserRole).toString();
            for (int r = 0; r < m_commitTable->rowCount(); ++r) {
              QTableWidgetItem *shaItem = m_commitTable->item(r, 7);
              if (shaItem && shaItem->data(Qt::UserRole).toString() == sha) {
                m_commitTable->selectRow(r);
                QTableWidgetItem *msgItem = m_commitTable->item(r, 3);
                if (msgItem) {
                  m_commitTable->setCurrentItem(msgItem);
                  m_commitTable->scrollToItem(msgItem,
                                              QAbstractItemView::EnsureVisible);
                }
                dlg.accept();
                break;
              }
            }
          });

  auto *closeBtn = new QPushButton(tr("Close"), &dlg);
  connect(closeBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
  layout->addWidget(closeBtn);
  dlg.resize(900, 500);

  table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
  table->setColumnWidth(0, 80);
  table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
  table->setColumnWidth(1, 130);
  table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
  table->setColumnWidth(2, 150);
  table->horizontalHeader()->setStretchLastSection(true);

  dlg.exec();
}

void MainWindow::showMergeDialog(const QString &branchToMerge) {
  if (m_currentPath.isEmpty()) {
    statusBar()->showMessage(tr("Open a repository first."), 0);
    return;
  }

  const QString currentBranch =
      m_gitExecutor->run(m_currentPath, {"rev-parse", "--abbrev-ref", "HEAD"})
          .value(0);
  if (currentBranch.isEmpty()) {
    statusBar()->showMessage(tr("Could not determine the current branch."), 0);
    return;
  }

  QDialog dlg(this);
  dlg.setWindowTitle(tr("Merge into %1").arg(currentBranch));
  dlg.setMinimumSize(650, 480);
  auto *layout = new QVBoxLayout(&dlg);

  // Branch selector
  auto *branchLayout = new QHBoxLayout;
  branchLayout->addWidget(new QLabel(tr("Merge branch:"), &dlg));
  auto *branchCombo = new QComboBox(&dlg);
  branchCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  branchLayout->addWidget(branchCombo);
  layout->addLayout(branchLayout);

  QStringList localBranches = m_gitExecutor->run(
      m_currentPath, {"branch", "--format=%(refname:short)"});
  localBranches.removeAll(currentBranch);
  QStringList remoteBranches = m_gitExecutor->run(
      m_currentPath, {"branch", "-r", "--format=%(refname:short)"});
  for (const QString &b : localBranches)
    branchCombo->addItem(b);
  if (!remoteBranches.isEmpty()) {
    branchCombo->insertSeparator(branchCombo->count());
    for (const QString &b : remoteBranches)
      branchCombo->addItem(b);
  }

  if (!branchToMerge.isEmpty()) {
    int idx = branchCombo->findText(branchToMerge);
    if (idx >= 0)
      branchCombo->setCurrentIndex(idx);
  }

  // Strategy selector
  auto *strategyLayout = new QHBoxLayout;
  strategyLayout->addWidget(new QLabel(tr("Strategy:"), &dlg));
  auto *strategyCombo = new QComboBox(&dlg);
  strategyCombo->addItem(tr("Default (fast-forward if possible)"), QString());
  strategyCombo->addItem(tr("No fast-forward (always create merge commit)"),
                         QStringLiteral("--no-ff"));
  strategyCombo->addItem(tr("Fast-forward only (fail if not possible)"),
                         QStringLiteral("--ff-only"));
  strategyCombo->addItem(tr("Squash (combine all commits into one)"),
                         QStringLiteral("--squash"));
  strategyLayout->addWidget(strategyCombo);
  layout->addLayout(strategyLayout);

  // Preview area
  auto *previewGroup = new QGroupBox(tr("Merge preview"), &dlg);
  auto *previewLayout = new QVBoxLayout(previewGroup);
  auto *statsLabel = new QLabel(&dlg);
  statsLabel->setWordWrap(true);
  previewLayout->addWidget(statsLabel);
  auto *fileList = new QTreeWidget(&dlg);
  fileList->setHeaderLabels({tr("File"), tr("Status")});
  fileList->setRootIsDecorated(false);
  fileList->setAlternatingRowColors(true);
  previewLayout->addWidget(fileList);
  auto *conflictLabel = new QLabel(&dlg);
  conflictLabel->setWordWrap(true);
  conflictLabel->setStyleSheet(
      QStringLiteral("color: red; font-weight: bold;"));
  conflictLabel->setVisible(false);
  previewLayout->addWidget(conflictLabel);
  layout->addWidget(previewGroup);

  // Buttons
  auto *buttons = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
  auto *mergeButton = buttons->button(QDialogButtonBox::Ok);
  mergeButton->setText(tr("Merge"));
  mergeButton->setEnabled(false);
  layout->addWidget(buttons);
  connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

  // Preview update lambda
  auto updatePreview = [&]() {
    fileList->clear();
    statsLabel->clear();
    conflictLabel->setVisible(false);
    mergeButton->setEnabled(false);

    const QString branch = branchCombo->currentText();
    if (branch.isEmpty())
      return;

    // Check if already up-to-date
    const QString mergeBase =
        m_gitExecutor->run(m_currentPath, {"merge-base", currentBranch, branch})
            .value(0);
    const QString branchSha =
        m_gitExecutor->run(m_currentPath, {"rev-parse", branch}).value(0);
    const QString headSha =
        m_gitExecutor->run(m_currentPath, {"rev-parse", "HEAD"}).value(0);

    if (branchSha == headSha || branchSha == mergeBase) {
      statsLabel->setText(tr("Already up to date — nothing to merge."));
      return;
    }

    // Count commits
    const QStringList commits = m_gitExecutor->run(
        m_currentPath, {"log", "--oneline",
                        QStringLiteral("%1..%2").arg(mergeBase, branchSha)});

    // Get file diff
    const QStringList diffFiles = m_gitExecutor->run(
        m_currentPath, {"diff", "--name-status",
                        QStringLiteral("%1...%2").arg(currentBranch, branch)});

    statsLabel->setText(tr("%1 commit(s), %2 file(s) changed")
                            .arg(commits.size())
                            .arg(diffFiles.size()));

    for (const QString &line : diffFiles) {
      if (line.size() < 2)
        continue;
      const QChar statusChar = line.at(0);
      const QString fileName = line.mid(1).trimmed();
      QString statusText;
      switch (statusChar.toLatin1()) {
      case 'A':
        statusText = tr("Added");
        break;
      case 'M':
        statusText = tr("Modified");
        break;
      case 'D':
        statusText = tr("Deleted");
        break;
      case 'R':
        statusText = tr("Renamed");
        break;
      case 'C':
        statusText = tr("Copied");
        break;
      default:
        statusText = QString(statusChar);
        break;
      }
      auto *item = new QTreeWidgetItem(fileList, {fileName, statusText});
      Q_UNUSED(item)
    }
    fileList->resizeColumnToContents(0);

    // Check for potential conflicts using merge-tree (git 2.38+)
    QString mergeTreeOutput;
    if (m_gitExecutor->exec(
            m_currentPath,
            {"merge-tree", "--write-tree", "--no-messages", headSha, branchSha},
            &mergeTreeOutput)) {
      mergeButton->setEnabled(true);
    } else {
      if (mergeTreeOutput.contains(QLatin1String("CONFLICT")) ||
          mergeTreeOutput.contains(QLatin1String("Auto-merging"))) {
        QStringList conflictFiles;
        for (const QString &cline : mergeTreeOutput.split('\n')) {
          if (cline.startsWith(QLatin1String("CONFLICT")))
            conflictFiles.append(cline);
        }
        conflictLabel->setText(tr("Warning: merge will produce conflicts:\n%1")
                                   .arg(conflictFiles.join('\n')));
        conflictLabel->setVisible(true);
      }
      mergeButton->setEnabled(true);
    }
  };

  connect(branchCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          &dlg, [&]() { updatePreview(); });

  // Initial preview
  updatePreview();

  if (dlg.exec() != QDialog::Accepted)
    return;

  const QString selectedBranch = branchCombo->currentText();
  const QString strategy = strategyCombo->currentData().toString();

  QStringList args = {"merge"};
  if (!strategy.isEmpty())
    args << strategy;
  args << "-m"
       << tr("Merge branch '%1' into %2").arg(selectedBranch, currentBranch)
       << selectedBranch;

  QString output;
  if (m_gitExecutor->exec(m_currentPath, args, &output)) {
    loadRepository(m_currentPath);
    statusBar()->showMessage(
        tr("Merged %1 into %2").arg(selectedBranch, currentBranch));
  } else {
    const QStringList conflicted = m_gitExecutor->run(
        m_currentPath, {"diff", "--name-only", "--diff-filter=U"});
    if (!conflicted.isEmpty()) {
      loadRepository(m_currentPath);
      showConflictResolver(QStringLiteral("merge"));
    } else {
      statusBar()->showMessage(output.isEmpty() ? tr("Merge failed") : output,
                               0);
    }
  }
}

void MainWindow::showArchiveDialog() {
  if (m_currentPath.isEmpty()) {
    statusBar()->showMessage(tr("Open a repository first."), 0);
    return;
  }

  QDialog dlg(this);
  dlg.setWindowTitle(tr("Create Archive"));
  dlg.setMinimumWidth(500);
  auto *layout = new QFormLayout(&dlg);

  // Tree-ish (branch, tag, or commit)
  auto *refCombo = new QComboBox(&dlg);
  refCombo->setEditable(true);
  QStringList refs;
  refs += m_gitExecutor->run(m_currentPath,
                             {"branch", "--format=%(refname:short)"});
  refs +=
      m_gitExecutor->run(m_currentPath, {"tag", "--format=%(refname:short)"});
  refCombo->addItems(refs);
  refCombo->setCurrentText(QStringLiteral("HEAD"));
  layout->addRow(tr("Ref / commit:"), refCombo);

  // Format
  auto *formatCombo = new QComboBox(&dlg);
  formatCombo->addItem(QStringLiteral("zip"), QStringLiteral("zip"));
  formatCombo->addItem(QStringLiteral("tar.gz"), QStringLiteral("tar.gz"));
  formatCombo->addItem(QStringLiteral("tar"), QStringLiteral("tar"));
  layout->addRow(tr("Format:"), formatCombo);

  // Prefix
  auto *prefixEdit = new QLineEdit(&dlg);
  const QString repoName = QDir(m_currentPath).dirName();
  prefixEdit->setText(repoName + '/');
  prefixEdit->setPlaceholderText(tr("Optional path prefix inside archive"));
  layout->addRow(tr("Prefix:"), prefixEdit);

  // Path filter
  auto *pathEdit = new QLineEdit(&dlg);
  pathEdit->setPlaceholderText(
      tr("Leave empty to archive entire tree, or specify a subdirectory"));
  layout->addRow(tr("Path (optional):"), pathEdit);

  auto *buttons = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
  buttons->button(QDialogButtonBox::Ok)->setText(tr("Export"));
  layout->addRow(buttons);
  connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

  if (dlg.exec() != QDialog::Accepted)
    return;

  const QString ref = refCombo->currentText().trimmed();
  const QString format = formatCombo->currentData().toString();
  const QString prefix = prefixEdit->text();
  const QString pathFilter = pathEdit->text().trimmed();

  if (ref.isEmpty()) {
    statusBar()->showMessage(tr("No ref specified."), 0);
    return;
  }

  const QString defaultName =
      QStringLiteral("%1-%2.%3")
          .arg(repoName, ref.left(12).replace('/', '-'), format);
  const QString outputPath = QFileDialog::getSaveFileName(
      this, tr("Save Archive"), QDir::homePath() + '/' + defaultName,
      format == QStringLiteral("zip")      ? tr("Zip files (*.zip)")
      : format == QStringLiteral("tar.gz") ? tr("Gzip tar files (*.tar.gz)")
                                           : tr("Tar files (*.tar)"));
  if (outputPath.isEmpty())
    return;

  QStringList args = {"archive", "--format=" + format,
                      "--output=" + outputPath};
  if (!prefix.isEmpty())
    args << "--prefix=" + prefix;
  args << ref;
  if (!pathFilter.isEmpty())
    args << pathFilter;

  QString output;
  if (m_gitExecutor->exec(m_currentPath, args, &output)) {
    statusBar()->showMessage(tr("Archive saved to %1").arg(outputPath));
  } else {
    statusBar()->showMessage(output.isEmpty() ? tr("Archive failed") : output,
                             0);
  }
}

void MainWindow::showRebaseOntoDialog() {
  if (m_currentPath.isEmpty()) {
    statusBar()->showMessage(tr("Open a repository first."), 0);
    return;
  }

  const QString currentBranch =
      m_gitExecutor->run(m_currentPath, {"rev-parse", "--abbrev-ref", "HEAD"})
          .value(0);
  if (currentBranch.isEmpty()) {
    statusBar()->showMessage(tr("Could not determine the current branch."), 0);
    return;
  }

  QDialog dlg(this);
  dlg.setWindowTitle(tr("Rebase --onto"));
  dlg.setMinimumWidth(500);
  auto *layout = new QFormLayout(&dlg);

  // Description
  auto *descLabel = new QLabel(
      tr("Rebases commits from <b>start</b> (exclusive) to <b>end</b> "
         "(inclusive) onto <b>newbase</b>.<br>"
         "Equivalent to: <code>git rebase --onto &lt;newbase&gt; "
         "&lt;start&gt; &lt;end&gt;</code>"),
      &dlg);
  descLabel->setWordWrap(true);
  layout->addRow(descLabel);

  QStringList branches = m_gitExecutor->run(
      m_currentPath, {"branch", "--format=%(refname:short)"});

  // Newbase
  auto *newbaseCombo = new QComboBox(&dlg);
  newbaseCombo->setEditable(true);
  newbaseCombo->addItems(branches);
  layout->addRow(tr("New base (--onto):"), newbaseCombo);

  // Start (exclusive ancestor)
  auto *startCombo = new QComboBox(&dlg);
  startCombo->setEditable(true);
  startCombo->addItems(branches);
  startCombo->setCurrentText(currentBranch);
  layout->addRow(tr("Start (exclusive):"), startCombo);

  // End (what to rebase)
  auto *endCombo = new QComboBox(&dlg);
  endCombo->setEditable(true);
  endCombo->addItems(branches);
  endCombo->setCurrentText(currentBranch);
  layout->addRow(tr("End (inclusive):"), endCombo);

  auto *buttons = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
  buttons->button(QDialogButtonBox::Ok)->setText(tr("Rebase"));
  layout->addRow(buttons);
  connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

  if (dlg.exec() != QDialog::Accepted)
    return;

  const QString newbase = newbaseCombo->currentText().trimmed();
  const QString start = startCombo->currentText().trimmed();
  const QString end = endCombo->currentText().trimmed();

  if (newbase.isEmpty() || start.isEmpty() || end.isEmpty()) {
    statusBar()->showMessage(tr("All fields are required."), 0);
    return;
  }

  QString output;
  if (m_gitExecutor->exec(m_currentPath,
                          {"rebase", "--onto", newbase, start, end}, &output)) {
    loadRepository(m_currentPath);
    statusBar()->showMessage(
        tr("Rebase --onto %1 %2 %3 completed").arg(newbase, start, end));
  } else {
    if (output.contains(QLatin1String("CONFLICT")) ||
        output.contains(QLatin1String("conflict"))) {
      loadRepository(m_currentPath);
      showConflictResolver(QStringLiteral("rebase"));
    } else {
      statusBar()->showMessage(output.isEmpty() ? tr("Rebase failed") : output,
                               0);
    }
  }
}
