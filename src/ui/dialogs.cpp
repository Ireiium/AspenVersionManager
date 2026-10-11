#include "dialogs.h"
#include "widgets.h"
#include "settings/theme.h"
#include "settings/language.h"

#include <QHBoxLayout>
#include <QTextBrowser>
#include <QTextOption>
#include <QDialogButtonBox>
#include <QEvent>

appDialog::appDialog(const QString& title, QWidget* parent) : QDialog(parent)
{
    bindText(this, title, "windowTitle");
    setWindowModality(Qt::WindowModal);
    setSizeGripEnabled(true);
    resize(620, 280);
    setMinimumWidth(440);
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(uiSize::space24, uiSize::space24, uiSize::space24, uiSize::space24);
    layout->setSpacing(uiSize::space16);
    layout->addWidget(textLabel(title, "heading", this));
    body = new QVBoxLayout;
    body->setSpacing(uiSize::space12);
    layout->addLayout(body, 1);
    footer = new QHBoxLayout;
    footer->setSpacing(uiSize::space8);
    footer->addStretch();
    layout->addLayout(footer);
}

actionButton* appDialog::addButtons(const QString& acceptText, bool cancel)
{
    if (cancel)
    {
        actionButton* cancelBtn = new actionButton("取消", false, this);
        cancelBtn->setObjectName("cancelBtn");
        cancelBtn->setMinimumWidth(uiSize::buttonWidth);
        connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
        footer->addWidget(cancelBtn);
    }
    actionButton* acceptBtn = new actionButton(acceptText, true, this);
    acceptBtn->setObjectName("acceptBtn");
    acceptBtn->setMinimumWidth(uiSize::buttonWidth);
    acceptBtn->setDefault(true);
    connect(acceptBtn, &QPushButton::clicked, this, &QDialog::accept);
    footer->addWidget(acceptBtn);
    return acceptBtn;
}

adminDialog::adminDialog() : appDialog("需要使用管理员启动")
{
    setObjectName("adminDialog");
    resize(620, 240);
    body->addWidget(textLabel("当前程序未以管理员身份运行。修改系统范围的文件关联需要管理员权限，"
                             "请选择以管理员启动，或继续使用当前权限。", {}, this));
    notice = textLabel({}, "muted", this);
    notice->setObjectName("adminError");
    notice->hide();
    body->addWidget(notice);

    actionButton* continueBtn = new actionButton("继续", false, this);
    continueBtn->setObjectName("continueBtn");
    continueBtn->setMinimumWidth(uiSize::buttonWidth);
    continueBtn->setAutoDefault(true);
    footer->addWidget(continueBtn);
    connect(continueBtn, &QPushButton::clicked, this,
            [this] { done(continueApp); });

    actionButton* adminBtn = addButtons("以管理员启动", false);
    adminBtn->setAutoDefault(true);
    adminBtn->setFocus();
}

void adminDialog::setError(const QString& text)
{
    notice->setText(text);
    notice->show();
}

detailDialog::detailDialog(QWidget* parent) : appDialog("关联详情", parent)
{
    setObjectName("detailDialog");
    resize(800, 560);
    setMinimumSize(540, 360);
    body->addWidget(textLabel("实际查询信息与操作详情，可选择文字并复制。", "muted", this));
    content = new QTextBrowser(this);
    content->setObjectName("detailText");
    content->setReadOnly(true);
    content->setOpenLinks(false);
    content->setOpenExternalLinks(false);
    // Word-boundary wrapping can strand a drive letter above the rest of a long path.
    content->setWordWrapMode(QTextOption::WrapAnywhere);
    body->addWidget(content, 1);
    actionButton* acceptBtn = addButtons("确定", false);
    acceptBtn->setFocus();
}

void detailDialog::setAssocs(const QList<shellAssoc>& assocs, const QString& info)
{
    items = assocs;
    operation = info.trimmed();
    formatted = true;
    QString html;
    if (!operation.isEmpty())
    {
        html += "<h3>" + QObject::tr("操作详情").toHtmlEscaped() + "</h3><p>" +
                operation.toHtmlEscaped().replace("\n", "<br>") + "</p>";
    }
    const auto field = [](const QString& name, const QString& value)
    {
        return "<tr><td width='112' valign='top'><b>" + name.toHtmlEscaped() + "</b></td><td>" +
               (value.isEmpty() ? QString("—") : value.toHtmlEscaped()) + "</td></tr>";
    };
    for (const shellAssoc& item : assocs)
    {
        html += "<h3>" + item.fileExt.toHtmlEscaped() + "</h3><table cellspacing='8' width='100%'>";
        html += field("ProgID", item.progId);
        html += field(QObject::tr("打开命令"), item.command);
        html += field(QObject::tr("程序路径"), item.exePath);
        html += field(QObject::tr("来源"), item.source.root + "\\" + item.source.path);
        html += field(QObject::tr("视图"), QObject::tr("%1 位").arg(item.source.view));
        html += field(QObject::tr("状态"), item.issue.isEmpty() ? QObject::tr("可安全修改") : item.issue);
        html += "</table>";
    }
    content->setHtml(html);
    content->moveCursor(QTextCursor::Start);
}

void detailDialog::changeEvent(QEvent* event)
{
    appDialog::changeEvent(event);
    if (event->type() == QEvent::LanguageChange && formatted)
    {
        setAssocs(items, operation);
    }
}

fileDialog::fileDialog(const QString& title, bool folder, QWidget* parent) : QFileDialog(parent, title)
{
    bindText(this, title, "windowTitle");
    setOption(QFileDialog::DontUseNativeDialog);
    setFileMode(folder ? QFileDialog::Directory : QFileDialog::ExistingFile);
    setOption(QFileDialog::ShowDirsOnly, folder);
    if (!folder)
    {
        setNameFilter(QObject::tr("程序 (*.exe)"));
    }
    setWindowModality(Qt::WindowModal);
    resize(760, 500);
    if (layout())
    {
        layout()->setContentsMargins(uiSize::space24, uiSize::space24, uiSize::space24, uiSize::space24);
        layout()->setSpacing(uiSize::space12);
    }
    for (QPushButton* button : findChildren<QPushButton*>())
    {
        button->setMinimumHeight(uiSize::controlHeight);
    }
    QDialogButtonBox* buttons = findChild<QDialogButtonBox*>();
    if (buttons)
    {
        for (QAbstractButton* button : buttons->buttons())
        {
            button->setMinimumWidth(uiSize::buttonWidth);
            button->setProperty(
                "role", buttons->buttonRole(button) == QDialogButtonBox::AcceptRole ? "primary" : "button");
        }
    }
}

void fileDialog::changeEvent(QEvent* event)
{
    QFileDialog::changeEvent(event);
    if (event->type() == QEvent::LanguageChange && fileMode() == QFileDialog::ExistingFile)
    {
        setNameFilter(QObject::tr("程序 (*.exe)"));
    }
}

bool showMessage(QWidget* parent, const QString& title, const QString& text, const QString& details,
                 bool error)
{
    appDialog dialog(title, parent);
    dialog.setObjectName("messageDialog");
    if (error)
    {
        dialog.body->addWidget(new statusTag("操作未完成", "error", &dialog), 0, Qt::AlignLeft);
    }
    dialog.body->addWidget(textLabel(text, {}, &dialog));
    bool requested = false;
    if (!details.isEmpty())
    {
        actionButton* more = new actionButton("查看详细", false, &dialog);
        more->setObjectName("detailsBtn");
        dialog.footer->addWidget(more);
        QObject::connect(more, &QPushButton::clicked, &dialog,
                         [&dialog, &requested]
                         {
                             requested = true;
                             dialog.accept();
                         });
    }
    actionButton* acceptBtn = dialog.addButtons("确定", false);
    acceptBtn->setFocus();
    dialog.exec();
    return requested;
}
