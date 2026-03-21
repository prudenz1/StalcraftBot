#include <QApplication>
#include <QMessageBox>
#include "utils/Logger.h"
#include "core/Config.h"
#include "core/Database.h"
#include "core/ApiClient.h"
#include "core/Scheduler.h"
#include "core/PriceAnalyzer.h"
#include "core/DealDetector.h"
#include "ui/MainWindow.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("StalcraftBot");
    app.setOrganizationName("StalcraftBot");

    Logger::init("stalcraft_bot.log");
    LOG_INFO("Application starting...");

    Config config;

    Database db(&config);
    bool dbConnected = db.connect();
    if (dbConnected) {
        db.migrate();
        LOG_INFO("Database connected and migrated");
    } else {
        LOG_WARN("Database connection failed, app will start without DB. "
                 "Go to Settings tab to configure.");
    }

    ApiClient api(&config);
    PriceAnalyzer analyzer(&db, &config);
    DealDetector detector(&db, &config, &analyzer);
    Scheduler scheduler(&config, &db, &api, &analyzer, &detector);

    MainWindow window(&config, &db, &api, &scheduler, &analyzer, &detector);
    window.show();

    if (!dbConnected) {
        QMessageBox::warning(&window,
            QString::fromUtf8("Нет подключения к БД"),
            QString::fromUtf8(
                "Не удалось подключиться к PostgreSQL.\n\n"
                "Перейдите на вкладку \"Настройки\", укажите параметры БД "
                "(хост, порт, имя БД, пользователь, пароль) и перезапустите приложение."));
    } else {
        scheduler.start();
    }

    LOG_INFO("Application started successfully");
    return app.exec();
}
