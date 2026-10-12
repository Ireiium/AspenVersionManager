#include "mainWindow.h"
#include "aboutPage.h"
#include "assoc/command.h"
#include "ui/comboBox.h"
#include "ui/dialogs.h"
#include "ui/widgets.h"
#include "settings/language.h"
#include "settings/theme.h"

#include <QCloseEvent>
#include <QSignalBlocker>
#include <QDesktopServices>
#include <QDialog>
#include <QDir>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <algorithm>
#include <QListWidget>
#include <QMap>
#include <QScrollArea>
#include <QSettings>
#include <QStackedWidget>
#include <QStyle>
#include <QTextOption>
#include <QUrl>
#include <QtConcurrent>
#include <QEvent>
#include <QTimer>

namespace
{
void clearLayout(QVBoxLayout* layout)
{
    while (QLayoutItem* item = layout->takeAt(0))
    {
        delete item->widget();
        delete item;
    }
}
} // namespace

mainWindow::mainWindow(theme& colors, std::shared_ptr<registry> backend, QWidget* parent)
    : QMainWindow(parent), colors(colors), demo(bool(backend)),
      reg(backend ? std::move(backend) : std::make_shared<registry>())
{
    language::instance();
    bindText(this, demo ? "AspenVersionManager · 模拟模式" : "AspenVersionManager", "windowTitle");
    resize(1080, 720);
    setMinimumSize(780, 560);
    QSettings settings;
    restoreGeometry(settings.value("windowGeometry").toByteArray());
    initWindow();
    initVersions();
    initSettings();
    pages->addWidget(new aboutPage(colors, pages));
    connectTasks();
    updateText();
    manual = loadManual();
    refresh();
}

void mainWindow::initWindow()
{
    QWidget* root = new QWidget(this);
    setCentralWidget(root);
    QHBoxLayout* layout = new QHBoxLayout(root);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    QWidget* side = new QWidget(root);
    side->setObjectName("sideBar");
    side->setFixedWidth(uiSize::sideWidth);
    QVBoxLayout* nav = new QVBoxLayout(side);
    nav->setContentsMargins(uiSize::space16, uiSize::space24, uiSize::space16, uiSize::space24);
    nav->setSpacing(uiSize::space8);
    nav->addWidget(textLabel("Aspen Plus", "heading", side));
    nav->addWidget(textLabel("默认版本配置", "muted", side));
    nav->addSpacing(uiSize::space24);
    navButton* versionNav = new navButton("版本管理", side);
    versionNav->setObjectName("versionNav");
    versionNav->setChecked(true);
    navButton* settingsNav = new navButton("设置", side);
    settingsNav->setObjectName("settingsNav");
    navButton* aboutNav = new navButton("关于", side);
    aboutNav->setObjectName("aboutNav");
    nav->addWidget(versionNav);
    nav->addWidget(settingsNav);
    nav->addWidget(aboutNav);
    nav->addStretch();
    nav->addWidget(
        textLabel(demo ? "模拟数据\n不会修改系统关联" : "独立配置工具\n关闭后关联仍然有效", "muted", side));
    nav->addWidget(textLabel(QString("v%1 · Qt %2").arg(AVM_VERSION, QT_VERSION_STR), "muted", side));
    layout->addWidget(side);

    QWidget* content = new QWidget(root);
    QVBoxLayout* body = new QVBoxLayout(content);
    body->setContentsMargins(uiSize::space24, uiSize::space24, uiSize::space24, uiSize::space16);
    body->setSpacing(uiSize::space16);
    layout->addWidget(content, 1);
    pages = new QStackedWidget(content);
    body->addWidget(pages, 1);

    notice = textLabel({}, "muted", content);
    notice->setObjectName("notice");
    body->addWidget(notice);

    connect(versionNav, &QPushButton::clicked, this, [this] { pages->setCurrentIndex(0); });
    connect(settingsNav, &QPushButton::clicked, this, [this] { pages->setCurrentIndex(1); });
    connect(aboutNav, &QPushButton::clicked, this, [this] { pages->setCurrentIndex(2); });
}

void mainWindow::initVersions()
{
    QWidget* versionPage = new QWidget(pages);
    versionPage->setObjectName("versionPage");
    QVBoxLayout* pageLayout = new QVBoxLayout(versionPage);
    pageLayout->setContentsMargins(0, 0, 0, 0);
    pageLayout->setSpacing(uiSize::space16);

    QLabel* titleLabel = textLabel("版本管理", "title", versionPage);
    refreshBtn = new actionButton("刷新", false, versionPage);
    addBtn = new actionButton("手动添加", false, versionPage);
    refreshBtn->setObjectName("refreshBtn");
    addBtn->setObjectName("addBtn");
    refreshBtn->setProperty("role", "quiet");

    QHBoxLayout* topLayout = new QHBoxLayout;
    topLayout->setSpacing(uiSize::space8);
    topLayout->addWidget(titleLabel, 1);
    topLayout->addWidget(refreshBtn);
    topLayout->addWidget(addBtn);
    pageLayout->addLayout(topLayout);

    QWidget* page = new QWidget(pages);
    QVBoxLayout* body = new QVBoxLayout(page);
    body->setSizeConstraint(QLayout::SetMinAndMaxSize);
    body->setContentsMargins(0, 0, uiSize::space8, 0);
    body->setSpacing(uiSize::space16);
    card* status = new card(page);
    status->setObjectName("assocSummary");
    actionButton* more = new actionButton("查看关联详情", false, status);
    more->setObjectName("assocDetailsBtn");
    more->setProperty("role", "quiet");
    summary = textLabel({}, "heading", status);
    summary->setText(QObject::tr("正在查询默认版本…"));
    summary->setObjectName("summary");
    assocState = textLabel({}, "muted", status);
    assocState->setText(".apw / .apwz / .bkp");
    assocState->setObjectName("assocState");

    QVBoxLayout* infoLayout = new QVBoxLayout;
    infoLayout->setSpacing(uiSize::space4);
    infoLayout->addWidget(summary);
    infoLayout->addWidget(assocState);
    QHBoxLayout* summaryLayout = new QHBoxLayout;
    summaryLayout->setSpacing(uiSize::space16);
    summaryLayout->addLayout(infoLayout, 1);
    summaryLayout->addWidget(more);
    status->body->addLayout(summaryLayout);
    body->addWidget(status);
    body->addSpacing(uiSize::space8);
    count = textLabel({}, "heading", page);
    body->addWidget(count);
    cards = new QVBoxLayout;
    cards->setSpacing(uiSize::space12);
    body->addLayout(cards);
    body->addStretch();
    pageLayout->addWidget(scrollPage(page), 1);
    pages->addWidget(versionPage);

    connect(refreshBtn, &QPushButton::clicked, this, &mainWindow::refresh);
    connect(addBtn, &QPushButton::clicked, this, &mainWindow::addVersion);
    connect(more, &QPushButton::clicked, this, [this] { showDetails(); });
}

void mainWindow::initSettings()
{
    QWidget* settingsPage = new QWidget(pages);
    settingsPage->setObjectName("settingsPage");
    QVBoxLayout* pageLayout = new QVBoxLayout(settingsPage);
    pageLayout->setContentsMargins(0, 0, 0, 0);
    pageLayout->setSpacing(uiSize::space16);
    pageLayout->addWidget(textLabel("设置", "title", settingsPage));

    QWidget* page = new QWidget(pages);
    QVBoxLayout* body = new QVBoxLayout(page);
    body->setContentsMargins(0, 0, uiSize::space8, 0);
    body->setSpacing(uiSize::space8);
    themeBox = new comboBox(colors, page);
    themeBox->setObjectName("themeBox");
    themeBox->addItems({"跟随系统", "浅色", "深色"});
    themeBox->setCurrentIndex(static_cast<int>(colors.mode()));
    body->addWidget(textLabel("外观", "heading", page));
    body->addWidget(new settingRow("应用主题", {}, themeBox, page));

    languageBox = new comboBox(colors, page);
    languageBox->setObjectName("languageBox");
    languageBox->addItem("中文", static_cast<int>(langMode::chinese));
    languageBox->addItem("English", static_cast<int>(langMode::english));
    languageBox->setCurrentIndex(languageBox->findData(static_cast<int>(language::instance().mode())));
    body->addWidget(new settingRow("语言", {}, languageBox, page));
    body->addSpacing(uiSize::space16);

    actionButton* openDir = new actionButton("打开目录", false, page);
    body->addWidget(textLabel("配置与恢复", "heading", page));
    openDir->setObjectName("backupDirBtn");
    settingRow* backupRow = new settingRow("关联备份", "修改前自动备份，便于恢复原有配置。", openDir, page);
    QLineEdit* backupPath = new QLineEdit(QDir::toNativeSeparators(backupDir()), backupRow);
    backupPath->setObjectName("backupPath");
    backupPath->setProperty("role", "path");
    backupPath->setReadOnly(true);
    backupPath->setCursorPosition(0);
    backupRow->textLayout->addWidget(backupPath);
    body->addWidget(backupRow);

    restoreBtn = new actionButton("恢复配置", false, page);
    restoreBtn->setObjectName("restoreAction");
    body->addWidget(new settingRow("恢复配置", "选择关联备份，恢复前检查是否有外部改动。", restoreBtn, page));
    body->addSpacing(uiSize::space16);

    body->addWidget(textLabel("系统", "heading", page));
    actionButton* defaults = new actionButton("默认应用", false, page);
    defaults->setObjectName("defaultsBtn");
    body->addWidget(new settingRow("Windows 默认应用", "关联属于其他软件时，在系统设置中选择 Aspen Plus。",
                                   defaults, page));
    actionButton* helpBtn = new actionButton("查看说明", false, page);
    helpBtn->setObjectName("helpBtn");
    body->addWidget(
        new settingRow("权限与特殊关联", "权限要求、特殊打开机制及恢复注意事项。", helpBtn, page));
    body->addStretch();
    pageLayout->addWidget(scrollPage(page), 1);
    pages->addWidget(settingsPage);
    connect(themeBox, &QComboBox::currentIndexChanged, this,
            [this](int index) { colors.setMode(static_cast<themeMode>(index)); });
    connect(languageBox, &QComboBox::currentIndexChanged, this,
            [this](int index)
            {
                language::instance().setMode(static_cast<langMode>(languageBox->itemData(index).toInt()));
            });
    connect(&language::instance(), &language::changed, this, &mainWindow::updateText);
    connect(&colors, &theme::changed, this,
            [this]
            {
                QSignalBlocker blocker(themeBox);
                themeBox->setCurrentIndex(static_cast<int>(colors.mode()));
            });
    connect(restoreBtn, &QPushButton::clicked, this, &mainWindow::restoreAssoc);
    connect(openDir, &QPushButton::clicked, this, &mainWindow::openBackup);
    connect(defaults, &QPushButton::clicked, this, &mainWindow::openDefaults);
    connect(helpBtn, &QPushButton::clicked, this, &mainWindow::showHelp);
}

void mainWindow::updateText()
{
    QSignalBlocker themeGuard(themeBox);
    themeBox->setItemText(0, QObject::tr("跟随系统"));
    themeBox->setItemText(1, QObject::tr("浅色"));
    themeBox->setItemText(2, QObject::tr("深色"));
    QSignalBlocker langGuard(languageBox);
    languageBox->setCurrentIndex(languageBox->findData(static_cast<int>(language::instance().mode())));
}

void mainWindow::changeEvent(QEvent* event)
{
    QMainWindow::changeEvent(event);
    if (event->type() == QEvent::LanguageChange)
    {
        updateText();
        // 子控件处理完 LanguageChange 后再刷新版本列表。
        QTimer::singleShot(0, this,
                           [this]
                           {
                               render();
                               if (!busy)
                               {
                                   refresh();
                               }
                           });
    }
}

void mainWindow::connectTasks()
{
    connect(&scan, &QFutureWatcher<refreshData>::finished, this,
            [this]
            {
                const refreshData result = scan.result();
                versions = result.versions;
                assocs = result.assocs;
                setBusy(false);
                render();
            });
    connect(&work, &QFutureWatcher<workData>::finished, this,
            [this]
            {
                const workData output = work.result();
                assocs = output.assocs;
                render();
                finish(output.result);
            });
}

void mainWindow::showHelp()
{
    appDialog dialog("权限与特殊关联", this);
    dialog.resize(660, 440);
    QPlainTextEdit* content = new QPlainTextEdit(&dialog);
    content->setReadOnly(true);
    content->setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    content->setPlainText(
        QObject::tr(
            "权限\n系统范围 HKLM 关联通常需要管理员权限。访问拒绝时，请右键以管理员身份运行后重试。\n\n"
            "特殊打开机制\n含 DDE 或 COM 的关联会停止修改，避免仅替换 EXE 后仍进入错误版本。"
            "各文件类型的实际命令与失败原因可在关联详情中查看。\n\n"
            "恢复\n恢复会检查外部改动，无法确认时停止覆盖。意外中断的 pending 备份也可在恢复配置中处理。\n\n"
            "备份位置\n") +
        QDir::toNativeSeparators(backupDir()));
    dialog.body->addWidget(content, 1);
    dialog.addButtons("确定", false)->setFocus();
    dialog.exec();
}

void mainWindow::openBackup()
{
    QDir().mkpath(backupDir());
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(backupDir())))
    {
        message("无法打开目录", "请通过资源管理器打开备份路径。", backupDir(), true);
    }
}

void mainWindow::openDefaults()
{
    if (!QDesktopServices::openUrl(QUrl("ms-settings:defaultapps")))
    {
        message("无法打开设置", "请在 Windows 设置 → 应用 → 默认应用中选择 Aspen Plus。", {}, true);
    }
}

void mainWindow::showDetails(const QString& info)
{
    if (!details)
    {
        details = new detailDialog(this);
    }
    details->setAssocs(assocs, info);
    details->show();
    details->raise();
    details->activateWindow();
}

void mainWindow::message(const QString& title, const QString& text, const QString& info, bool error)
{
    if (showMessage(this, title, text, info, error))
    {
        showDetails(info);
    }
}

mainWindow::~mainWindow()
{
    scan.waitForFinished();
    work.waitForFinished();
}

void mainWindow::closeEvent(QCloseEvent* event)
{
    if (busy)
    {
        bindText(notice, "正在完成操作，请稍后关闭。");
        notice->show();
        event->ignore();
        return;
    }
    QSettings settings;
    settings.setValue("windowGeometry", saveGeometry());
    QMainWindow::closeEvent(event);
}

void mainWindow::setBusy(bool value, const char* text)
{
    busy = value;
    refreshBtn->setEnabled(!value);
    addBtn->setEnabled(!value);
    restoreBtn->setEnabled(!value);
    for (QPushButton* button : pages->findChildren<QPushButton*>("applyBtn"))
        button->setEnabled(!value && button->property("available").toBool());
    if (text)
        bindText(notice, QString::fromUtf8(text));
    notice->setVisible(value);
}

void mainWindow::refresh()
{
    if (busy)
        return;
    setBusy(true, "正在扫描安装版本并查询实际文件关联…");
    const std::shared_ptr<registry> backend = reg;
    const QList<versionInfo> added = manual;
    scan.setFuture(QtConcurrent::run(
        [backend, added]
        {
            associations assoc(*backend, {});
            return refreshData{scanVersions(*backend, added), assoc.query()};
        }));
}

void mainWindow::render()
{
    clearLayout(cards);
    const QString current = assocs.isEmpty() ? QString() : assocs.first().exePath;
    bool consistent = assocs.size() == 3 && !current.isEmpty();
    bool writable = assocs.size() == 3;
    for (const shellAssoc& item : assocs)
    {
        consistent = consistent && samePath(current, item.exePath);
        writable = writable && item.issue.isEmpty();
    }

    QString label;
    for (const versionInfo& item : versions)
    {
        if (samePath(current, item.exePath))
        {
            label = item.name;
            break;
        }
    }
    if (consistent)
    {
        summary->setText(
            QObject::tr("当前默认：%1").arg(label.isEmpty() ? QObject::tr("已关联程序") : label));
        assocState->setText(QObject::tr(".apw / .apwz / .bkp 一致") +
                            (writable ? QString() : QObject::tr(" · 当前关联需要处理，查看详情")));
    }
    else
    {
        const bool unlinked = std::all_of(assocs.begin(), assocs.end(),
                                          [](const shellAssoc& item) { return item.exePath.isEmpty(); });
        summary->setText(unlinked ? QObject::tr("尚未关联 Aspen Plus") : QObject::tr("当前关联不一致"));
        assocState->setText(".apw / .apwz / .bkp · " + (unlinked ? QObject::tr("请查看详情确认关联状态")
                                                                 : QObject::tr("未指向同一版本，查看详情")));
    }
    assocState->setProperty("role", writable && consistent ? "muted" : "warning");
    assocState->style()->unpolish(assocState);
    assocState->style()->polish(assocState);
    count->setText(QObject::tr("已安装版本 · %1").arg(versions.size()));
    if (versions.isEmpty())
    {
        cards->addWidget(textLabel("未发现 Aspen Plus。通过“手动添加”选择安装目录或程序。", "muted"));
    }

    for (const versionInfo& item : versions)
    {
        const bool selected = consistent && samePath(current, item.exePath);
        versionRow* row = new versionRow(item, selected, writable && !busy, pages);
        cards->addWidget(row);
        connect(row, &versionRow::useRequested, this, [this, item] { applyAssoc(item); });
    }
}

void mainWindow::applyAssoc(const versionInfo& item)
{
    if (busy)
        return;
    setBusy(true, "正在备份并更新关联，请等待验证完成…");
    const std::shared_ptr<registry> backend = reg;
    const QString folder = backupDir();
    work.setFuture(QtConcurrent::run(
        [backend, folder, item]
        {
            associations assoc(*backend, folder);
            const assocResult result = assoc.applyAssoc(item);
            return workData{result, assoc.query()};
        }));
}

void mainWindow::finish(const assocResult& result)
{
    setBusy(false);
    QString info = result.details.trimmed();
    if (!result.backupPath.isEmpty())
    {
        if (!info.isEmpty())
        {
            info += '\n';
        }
        info += QObject::tr("备份：%1").arg(QDir::toNativeSeparators(result.backupPath));
    }
    message(result.ok ? "配置完成" : "配置未完成", result.message, info, !result.ok);
    refresh();
}

void mainWindow::addVersion()
{
    appDialog dialog("手动添加 Aspen Plus", this);
    dialog.setObjectName("addDialog");
    dialog.resize(640, 320);
    QVBoxLayout* body = dialog.body;
    body->addWidget(
        textLabel("选择安装目录、apwn.exe 或 aspenplus.exe。名称可自定义，版本按名称和安装信息识别。",
                  "muted", &dialog));
    QLineEdit* name = new QLineEdit(&dialog);
    name->setObjectName("nameInput");
    name->setMinimumHeight(uiSize::controlHeight);
    bindText(name, "例如 Aspen Plus V15（可留空）", "placeholderText");
    QLineEdit* path = new QLineEdit(&dialog);
    path->setObjectName("pathInput");
    path->setMinimumHeight(uiSize::controlHeight);
    bindText(path, "安装目录或 Aspen Plus 程序的绝对路径", "placeholderText");
    QFormLayout* form = new QFormLayout;
    form->setHorizontalSpacing(uiSize::space16);
    form->setVerticalSpacing(uiSize::space12);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    form->addRow(textLabel("版本名称", {}, &dialog), name);
    form->addRow(textLabel("程序路径", {}, &dialog), path);
    body->addLayout(form);
    QHBoxLayout* choices = new QHBoxLayout;
    actionButton* file = new actionButton("选择程序", false, &dialog);
    actionButton* folder = new actionButton("选择目录", false, &dialog);
    file->setObjectName("chooseFileBtn");
    folder->setObjectName("chooseDirBtn");
    choices->addWidget(file);
    choices->addWidget(folder);
    choices->addStretch();
    body->addLayout(choices);

    const auto choosePath = [&dialog, path](const QString& title, bool folder)
    {
        fileDialog chooser(title, folder, &dialog);
        if (chooser.exec() == QDialog::Accepted)
        {
            path->setText(chooser.selectedFiles().value(0));
        }
    };
    connect(file, &QPushButton::clicked, &dialog,
            [choosePath] { choosePath("选择 Aspen Plus 程序", false); });
    connect(folder, &QPushButton::clicked, &dialog,
            [choosePath] { choosePath("选择 Aspen Plus 安装目录", true); });
    actionButton* accept = dialog.addButtons("添加");
    accept->setEnabled(false);
    connect(path, &QLineEdit::textChanged, &dialog,
            [accept](const QString& text) { accept->setEnabled(!text.trimmed().isEmpty()); });
    name->setFocus();
    if (dialog.exec() != QDialog::Accepted || path->text().trimmed().isEmpty())
        return;
    const std::shared_ptr<registry> backend = reg;
    const QString location = cleanPath(path->text());
    const QString label = name->text();
    setBusy(true, "正在检查手动指定的安装目录…");
    QFutureWatcher<QList<versionInfo>>* finder = new QFutureWatcher<QList<versionInfo>>(this);
    connect(finder, &QFutureWatcher<QList<versionInfo>>::finished, this,
            [this, finder]
            {
                const QList<versionInfo> found = finder->result();
                finder->deleteLater();
                for (const versionInfo& item : found)
                {
                    bool present = false;
                    for (versionInfo& old : manual)
                        if (samePath(old.exePath, item.exePath))
                        {
                            old.name = item.name;
                            present = true;
                        }
                    if (!present)
                        manual.append(item);
                }
                saveManual(manual);
                setBusy(false);
                refresh();
            });
    finder->setFuture(QtConcurrent::run(
        [backend, location, label]
        {
            QList<versionInfo> found;
            if (!QFileInfo(location).isDir())
                found.append(inspectVersion(location, label, *backend));
            else
            {
                QList<QPair<QString, int>> pending{{location, 0}};
                int count = 0;
                while (!pending.isEmpty() && count++ < 1500)
                {
                    const QPair<QString, int> item = pending.takeFirst();
                    QDir dir(item.first);
                    for (const QFileInfo& file :
                         dir.entryInfoList({"apwn.exe", "aspenplus.exe"}, QDir::Files))
                    {
                        versionInfo version = inspectVersion(file.absoluteFilePath(), label, *backend);
                        version.installPath = location;
                        found.append(version);
                    }
                    if (item.second < 5)
                        for (const QFileInfo& sub :
                             dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks))
                            pending.append({sub.absoluteFilePath(), item.second + 1});
                }
                if (found.isEmpty())
                    found.append(inspectVersion(location + "/apwn.exe", label, *backend));
            }
            return found;
        }));
}

void mainWindow::restoreAssoc()
{
    if (busy)
        return;
    const QStringList paths = listBackups();
    if (paths.isEmpty())
    {
        message("没有可恢复备份", "首次修改关联时会自动创建备份。");
        return;
    }
    appDialog dialog("恢复关联配置", this);
    dialog.setObjectName("restoreDialog");
    dialog.resize(760, 480);
    QVBoxLayout* body = dialog.body;
    body->addWidget(textLabel("选择修改前的配置。恢复前会备份当前命令，并检查外部改动。", "muted", &dialog));
    QListWidget* list = new QListWidget(&dialog);
    list->setObjectName("backupList");
    list->setWordWrap(true);
    body->addWidget(list, 1);
    const QMap<QString, QString> states{{"pending", QObject::tr("操作中断，可尝试恢复")},
                                        {"applied", QObject::tr("已应用")},
                                        {"restored", QObject::tr("恢复前备份")},
                                        {"rolledBack", QObject::tr("已回滚")},
                                        {"recoveryNeeded", QObject::tr("需要恢复")}};
    for (const QString& path : paths)
    {
        assocBackup backup;
        QString error;
        const bool valid = loadBackup(path, backup, error);
        QListWidgetItem* item = new QListWidgetItem(
            QFileInfo(path).fileName() + "\n" + (valid ? states.value(backup.state, backup.state) : error),
            list);
        item->setData(Qt::UserRole, path);
        if (!valid)
            item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
    }
    list->setCurrentRow(0);
    actionButton* acceptBtn = dialog.addButtons("恢复所选配置");
    const auto updateAccept = [list, acceptBtn]
    {
        acceptBtn->setEnabled(list->currentItem() &&
                              list->currentItem()->flags().testFlag(Qt::ItemIsEnabled));
    };
    connect(list, &QListWidget::currentRowChanged, &dialog, updateAccept);
    updateAccept();
    list->setFocus();
    if (dialog.exec() != QDialog::Accepted || !list->currentItem())
        return;
    const QString path = list->currentItem()->data(Qt::UserRole).toString();
    setBusy(true, "正在恢复备份并验证关联…");
    const std::shared_ptr<registry> backend = reg;
    const QString folder = backupDir();
    work.setFuture(QtConcurrent::run(
        [backend, folder, path]
        {
            associations assoc(*backend, folder);
            const assocResult result = assoc.restoreAssoc(path);
            return workData{result, assoc.query()};
        }));
}
