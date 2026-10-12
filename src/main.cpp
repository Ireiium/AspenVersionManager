#include "pages/mainWindow.h"
#include "settings/theme.h"
#include "settings/language.h"
#include "ui/dialogs.h"

#include <QApplication>
#include <QFont>
#include <QIcon>
#include <QDir>
#include <windows.h>
#include <shellapi.h>

namespace
{
bool isAdmin()
{
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
    {
        return false;
    }
    TOKEN_ELEVATION elevation{};
    DWORD size = 0;
    const bool valid = GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &size);
    CloseHandle(token);
    return valid && elevation.TokenIsElevated;
}

bool startApp()
{
    if (isAdmin())
    {
        return true;
    }

    adminDialog dialog;
    while (true)
    {
        const int choice = dialog.exec();
        if (choice == adminDialog::continueApp)
        {
            return true;
        }
        if (choice != QDialog::Accepted)
        {
            return false;
        }

        const std::wstring path =
            QDir::toNativeSeparators(QCoreApplication::applicationFilePath()).toStdWString();
        const std::wstring folder = QDir::toNativeSeparators(QDir::currentPath()).toStdWString();
        SHELLEXECUTEINFOW launch{};
        launch.cbSize = sizeof(launch);
        launch.fMask = SEE_MASK_NOASYNC | SEE_MASK_FLAG_NO_UI;
        launch.hwnd = reinterpret_cast<HWND>(dialog.winId());
        launch.lpVerb = L"runas";
        launch.lpFile = path.c_str();
        launch.lpDirectory = folder.c_str();
        launch.nShow = SW_SHOWNORMAL;
        if (ShellExecuteExW(&launch))
        {
            return false;
        }
        const DWORD error = GetLastError();
        dialog.setError(
            error == ERROR_CANCELLED
                ? QObject::tr("已取消管理员授权。可以重试，或点击“继续”。")
                : QObject::tr("无法以管理员启动（错误 %1）。可以重试，或点击“继续”。").arg(error));
    }
}
} // namespace

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("AspenVersionManager");
    QCoreApplication::setApplicationName("AspenVersionManager");
    QCoreApplication::setApplicationVersion(AVM_VERSION);
    app.setWindowIcon(QIcon(":/images/logo.png"));
    QApplication::setStyle("Fusion");
    language::instance();
    app.setFont(QFont("Microsoft YaHei UI", uiSize::bodyFont));
    theme colors;
    // Closing the startup prompt must not queue a quit before the main window exists.
    app.setQuitOnLastWindowClosed(false);
    if (!startApp())
    {
        return 0;
    }
    app.setQuitOnLastWindowClosed(true);
    mainWindow window(colors);
    window.show();
    return QApplication::exec();
}
