#include "aboutPage.h"
#include "settings/theme.h"
#include "settings/language.h"
#include "ui/dialogs.h"
#include "ui/widgets.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QScrollArea>
#include <QUrl>
#include <QVBoxLayout>
#include <windows.h>
#include <shlobj.h>

namespace
{
QLabel* aboutText(const QString& text, const QString& role, QWidget* parent)
{
    QLabel* label = new QLabel(text, parent);
    if (!text.isEmpty())
    {
        bindText(label, text);
    }
    label->setProperty("role", role);
    label->setTextFormat(Qt::PlainText);
    label->setWordWrap(true);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    label->setMinimumWidth(0);
    label->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    return label;
}

actionButton* textLink(const QString& text, QWidget* parent)
{
    actionButton* button = new actionButton(text, false, parent);
    button->setProperty("role", "link");
    button->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    return button;
}

bool sourceRoot(const QString& path)
{
    QDir folder(path);
    return QFileInfo(folder.filePath("CMakeLists.txt")).isFile() &&
           QFileInfo(folder.filePath("src/main.cpp")).isFile();
}

bool selectFile(const QString& path)
{
    const HRESULT init = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(init) && init != RPC_E_CHANGED_MODE)
    {
        return false;
    }
    PIDLIST_ABSOLUTE item = nullptr;
    const std::wstring file = QDir::toNativeSeparators(path).toStdWString();
    HRESULT result = SHParseDisplayName(file.c_str(), nullptr, &item, 0, nullptr);
    if (SUCCEEDED(result))
    {
        result = SHOpenFolderAndSelectItems(item, 0, nullptr, 0);
    }
    CoTaskMemFree(item);
    if (SUCCEEDED(init))
    {
        CoUninitialize();
    }
    return SUCCEEDED(result);
}
} // namespace

QString aboutPage::rootPath(const QString& appDir)
{
    QDir folder(appDir);
    // 发布包缺少许可证时，不能回退到上级开发目录。
    if (folder.dirName().compare("bin", Qt::CaseInsensitive) == 0)
    {
        folder.cdUp();
        return folder.absolutePath();
    }
    const bool devBuild = QFileInfo(folder.filePath("CMakeCache.txt")).isFile();
    if (!devBuild && (QFileInfo(folder.filePath("LICENSE")).isFile() || folder.exists("licenses") ||
                      QFileInfo(folder.filePath("qt.conf")).isFile()))
    {
        return folder.absolutePath();
    }
    for (int depth = 0; depth < 6; ++depth)
    {
        if (sourceRoot(folder.absolutePath()))
        {
            return folder.absolutePath();
        }
        const QString hint = folder.filePath(AVM_ROOT_HINT);
        if (sourceRoot(hint))
        {
            return QDir(hint).absolutePath();
        }
        if (!folder.cdUp())
        {
            break;
        }
    }
    return QDir(appDir).absolutePath();
}

aboutPage::aboutPage(theme& colors, QWidget* parent)
    : QWidget(parent), colors(colors), root(rootPath(QCoreApplication::applicationDirPath()))
{
    setObjectName("aboutPage");
    QVBoxLayout* pageLayout = new QVBoxLayout(this);
    pageLayout->setContentsMargins(0, 0, 0, 0);
    pageLayout->setSpacing(uiSize::space16);
    pageLayout->addWidget(aboutText("关于", "title", this));

    QWidget* content = new QWidget(this);
    QVBoxLayout* body = new QVBoxLayout(content);
    body->setContentsMargins(0, 0, uiSize::space8, uiSize::space8);
    body->setSpacing(uiSize::space8);
    initHeader(body);
    body->addSpacing(uiSize::space16);
    initCredits(body);
    initSource(body);
    body->addStretch();

    QScrollArea* scroll = scrollPage(content);
    scroll->setObjectName("aboutScroll");
    pageLayout->addWidget(scroll, 1);
}

void aboutPage::initHeader(QVBoxLayout* body)
{
    QLabel* iconLabel = new QLabel(this);
    iconLabel->setObjectName("aboutIcon");
    iconLabel->setPixmap(QIcon(":/images/logo.png").pixmap(64, 64));
    iconLabel->setFixedSize(64, 64);

    QVBoxLayout* titleLayout = new QVBoxLayout;
    titleLayout->setSpacing(uiSize::space4);
    titleLayout->addWidget(aboutText("AspenVersionManager", "heading", this));
    QLabel* versionLabel = aboutText({}, "muted", this);
    versionLabel->setObjectName("appVersion");
    const auto updateVersion = [versionLabel]
    { versionLabel->setText(QObject::tr("版本 %1").arg(AVM_VERSION)); };
    updateVersion();
    connect(&language::instance(), &language::changed, versionLabel, updateVersion);
    titleLayout->addWidget(versionLabel);

    QHBoxLayout* headerLayout = new QHBoxLayout;
    headerLayout->setSpacing(uiSize::space16);
    headerLayout->addWidget(iconLabel, 0, Qt::AlignTop);
    headerLayout->addLayout(titleLayout, 1);
    body->addLayout(headerLayout);
    body->addWidget(aboutText("配置双击 Aspen Plus 模型文件时使用的默认版本。", {}, this));
    body->addWidget(aboutText("独立工具，非 AspenTech 官方产品。", "muted", this));
    body->addWidget(aboutText("本项目采用 MIT 许可证。Copyright (c) 2026 Ireiium", {}, this));

    actionButton* mitLink = textLink("打开许可证位置", this);
    mitLink->setObjectName("mitLink");
    mitLink->setProperty("filePath", QDir(root).filePath("LICENSE"));
    body->addWidget(mitLink, 0, Qt::AlignLeft);
    connect(mitLink, &QPushButton::clicked, this, &aboutPage::openMit);
}

void aboutPage::addEntry(QVBoxLayout* body, const QString& name, const QUrl& url, const QString& license,
                         const QString& note)
{
    body->addWidget(aboutText(name, "heading", this));
    QLabel* webLabel = new QLabel(this);
    webLabel->setWordWrap(true);
    webLabel->setMinimumWidth(0);
    webLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    webLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
    webLabel->setOpenExternalLinks(false);
    const QString address = url.toString().toHtmlEscaped();
    const auto updateLink = [webLabel, address]
    {
        webLabel->setText(
            QString("<a href=\"%1\" style=\"color:%2\">%1</a>").arg(address, theme::tokens().focus.name()));
    };
    updateLink();
    body->addWidget(webLabel);
    QLabel* licenseLabel = aboutText({}, {}, this);
    const auto updateLicense = [licenseLabel, license]
    {
        licenseLabel->setText(QObject::tr("使用许可证：%1").arg(license));
    };
    updateLicense();
    connect(&language::instance(), &language::changed, licenseLabel, updateLicense);
    body->addWidget(licenseLabel);
    body->addWidget(aboutText(note, "muted", this));
    body->addSpacing(uiSize::space16);

    connect(&colors, &theme::changed, webLabel, updateLink);
    connect(webLabel, &QLabel::linkActivated, this,
            [this](const QString& address) { openWeb(QUrl(address)); });
}

void aboutPage::initCredits(QVBoxLayout* body)
{
    body->addWidget(aboutText("第三方开源信息", "heading", this));
    addEntry(body, "Qt(" QT_VERSION_STR ")", QUrl("https://www.qt.io/"), "LGPLv3",
             "Copyright (C) The Qt Company Ltd. and other contributors.");
    addEntry(body, "QtEntryPoint(" QT_VERSION_STR ")",
             QUrl("https://code.qt.io/cgit/qt/qtbase.git/tree/src/entrypoint/qtentrypoint_win.cpp?h=v6.8.3"),
             "BSD-3-Clause", "Copyright (C) 2019 The Qt Company Ltd.");
    addEntry(body, "Qt Translation", QUrl("https://code.qt.io/cgit/qt/qttranslations.git/?h=v6.8.3"),
             "BSD-3-Clause", "Copyright (C) The Qt Company Ltd. and other contributors.");

    actionButton* licensesLink = textLink("打开第三方许可证文件夹", this);
    licensesLink->setObjectName("licensesLink");
    licensesLink->setProperty("filePath", QDir(root).filePath("licenses"));
    body->addWidget(licensesLink, 0, Qt::AlignLeft);
    connect(licensesLink, &QPushButton::clicked, this, &aboutPage::openLicenses);
}

void aboutPage::initSource(QVBoxLayout* body)
{
    body->addSpacing(uiSize::space16);
    body->addWidget(aboutText("Qt源码", "heading", this));
    actionButton* sourceLink = textLink("获取 QtBase 6.8.3 源码", this);
    sourceLink->setObjectName("qtSourceLink");
    body->addWidget(sourceLink, 0, Qt::AlignLeft);
    connect(
        sourceLink, &QPushButton::clicked, this,
        [this]
        {
            openWeb(QUrl(
                "https://download.qt.io/archive/qt/6.8/6.8.3/submodules/qtbase-everywhere-src-6.8.3.zip"));
        });
    actionButton* transLink = textLink("获取 QtTranslations 6.8.3 源码", this);
    transLink->setObjectName("qtTransSource");
    body->addWidget(transLink, 0, Qt::AlignLeft);
    connect(transLink, &QPushButton::clicked, this,
            [this]
            {
                openWeb(QUrl("https://download.qt.io/archive/qt/6.8/6.8.3/submodules/"
                             "qttranslations-everywhere-src-6.8.3.zip"));
            });
}

void aboutPage::openWeb(const QUrl& url)
{
    if (!QDesktopServices::openUrl(url))
    {
        showMessage(this, "无法打开链接",
                    QObject::tr("请使用系统浏览器打开以下地址：\n%1").arg(url.toString()), {}, true);
    }
}

void aboutPage::openMit()
{
    const QString path = QDir(root).filePath("LICENSE");
    if (!QFileInfo(path).isFile())
    {
        showMessage(this, "许可证文件缺失",
                    QObject::tr("没有找到本项目的 MIT 许可证：\n%1").arg(QDir::toNativeSeparators(path)), {},
                    true);
        return;
    }
    if (!selectFile(path))
    {
        showMessage(this, "无法打开许可证位置",
                    QObject::tr("无法在文件资源管理器中选中：\n%1").arg(QDir::toNativeSeparators(path)), {},
                    true);
    }
}

void aboutPage::openLicenses()
{
    const QString path = QDir(root).filePath("licenses");
    if (!QFileInfo(path).isDir())
    {
        showMessage(this, "第三方许可证目录缺失",
                    QObject::tr("没有找到随包许可证目录：\n%1").arg(QDir::toNativeSeparators(path)), {},
                    true);
        return;
    }
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(path)))
    {
        showMessage(this, "无法打开许可证目录",
                    QObject::tr("无法打开：\n%1").arg(QDir::toNativeSeparators(path)), {}, true);
    }
}
