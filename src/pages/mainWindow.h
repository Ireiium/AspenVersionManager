#pragma once
#include "assoc/associations.h"
#include <QMainWindow>
#include <QFutureWatcher>
#include <QPointer>
#include <memory>

class QStackedWidget;
class QVBoxLayout;
class QLabel;
class QPushButton;
class comboBox;
class detailDialog;
class theme;

struct refreshData
{
    QList<versionInfo> versions;
    QList<shellAssoc> assocs;
};

struct workData
{
    assocResult result;
    QList<shellAssoc> assocs;
};

class mainWindow : public QMainWindow
{
    Q_OBJECT
    public:
    mainWindow(theme& colors, std::shared_ptr<registry> backend = {}, QWidget* parent = nullptr);
    ~mainWindow() override;

    protected:
    void closeEvent(QCloseEvent* event) override;
    void changeEvent(QEvent* event) override;

    private:
    theme& colors;
    bool demo;
    bool busy = false;
    std::shared_ptr<registry> reg;
    QList<versionInfo> versions;
    QList<versionInfo> manual;
    QList<shellAssoc> assocs;
    QFutureWatcher<refreshData> scan;
    QFutureWatcher<workData> work;
    QStackedWidget* pages;
    QVBoxLayout* cards;
    QLabel* summary;
    QLabel* assocState;
    QLabel* notice;
    QLabel* count;
    QPushButton* refreshBtn;
    QPushButton* addBtn;
    QPushButton* restoreBtn;
    comboBox* themeBox;
    QPointer<detailDialog> details;
    void initWindow();
    void initVersions();
    void initSettings();
    void connectTasks();
    void updateText();
    void showDetails(const QString& info = {});
    void showHelp();
    void openBackup();
    void openDefaults();
    void message(const QString& title, const QString& text, const QString& info = {}, bool error = false);
    void refresh();
    void render();
    void setBusy(bool value, const char* text = nullptr);
    void addVersion();
    void applyAssoc(const versionInfo& item);
    void restoreAssoc();
    void finish(const assocResult& result);
};
