#include "portfolio_controller.hpp"
#include "chart_item.hpp"
#include <qqml.h>
#include <QApplication>
#include <QSystemTrayIcon>
#include <QPainter>
#include <QScreen>
#include <QSettings>
#include "signals_controller.hpp"
#include "decision_controller.hpp"
#include "research_controller.hpp"
#include "events_controller.hpp"
#include "strategy_controller.hpp"
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QTimer>
#include <QImage>
#include <cstdio>
#include <algorithm>

int main(int argc,char* argv[]) {
    QApplication app(argc,argv);
    QCoreApplication::setOrganizationName("MOEXWatch");
    QCoreApplication::setApplicationName("MOEXWatch Qt");
    QQuickStyle::setStyle("Basic");
    const auto arguments=app.arguments();
    const bool smoke=arguments.contains("--smoke-test"),demo=arguments.contains("--demo");
    QTemporaryDir temporary;
    std::filesystem::path root;
    if(smoke||demo) {
#ifdef _WIN32
        root=temporary.path().toStdWString();
#else
        root=temporary.path().toStdString();
#endif
    }
    int errors=0;
    PortfolioController backend(root);
    if(smoke||demo)backend.loadDemo();
    if(demo&&arguments.contains("--light"))backend.setDark(false);
    qmlRegisterType<FinancialChart>("MOEXWatch.Charts",1,0,"FinancialChart");
    SignalsController signalModule(&backend);
    DecisionController decisions(&backend);
    ResearchController research(&backend);
    EventsController events(&backend);
    StrategyController strategies(&backend);
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("backend",&backend);
    engine.rootContext()->setContextProperty("signalBackend",&signalModule);
    engine.rootContext()->setContextProperty("decisionBackend",&decisions);
    engine.rootContext()->setContextProperty("researchBackend",&research);
    engine.rootContext()->setContextProperty("eventsBackend",&events);
    engine.rootContext()->setContextProperty("strategyBackend",&strategies);
    QObject::connect(&engine,&QQmlApplicationEngine::warnings,&app,[&](const QList<QQmlError>& warnings){
        for(const auto& warning:warnings){std::fprintf(stderr,"%s\n",qPrintable(warning.toString()));++errors;}
    });
    engine.loadFromModule("MOEXWatch","Main");
    if(engine.rootObjects().isEmpty())return 2;
    auto* mainWindow=qobject_cast<QQuickWindow*>(engine.rootObjects().first());
    if(!mainWindow)return 3;
    std::unique_ptr<QSettings> windowSettings;
    QRect normalGeometry=mainWindow->geometry();
    const QStringList pageKeys={"overview","screener","signals","plan","company","compare","events","strategy","portfolio","journal","diary","settings"};
    if(!smoke&&!demo&&backend.writable()) {
#ifdef _WIN32
        const auto settingsPath=QString::fromStdWString((backend.dataRoot()/"qt-window-v1.ini").wstring());
#else
        const auto settingsPath=QString::fromStdString((backend.dataRoot()/"qt-window-v1.ini").string());
#endif
        windowSettings=std::make_unique<QSettings>(settingsPath,QSettings::IniFormat);
        if(windowSettings->value("formatVersion",1).toInt()!=1) {
            mainWindow->setProperty("notice","Формат настроек окна не поддерживается; файл сохранён без изменений.");
            windowSettings.reset();
        }else {
            auto* screen=QGuiApplication::primaryScreen();
            const int savedWidth=std::clamp(windowSettings->value("width",1440).toInt(),640,10000);
            const int savedHeight=std::clamp(windowSettings->value("height",920).toInt(),480,10000);
            const int savedX=std::clamp(windowSettings->value("x",0).toInt(),-100000,100000);
            const int savedY=std::clamp(windowSettings->value("y",0).toInt(),-100000,100000);
            const auto center=QPoint(savedX+savedWidth/2,savedY+savedHeight/2);
            if(auto* found=QGuiApplication::screenAt(center))screen=found;
            const auto area=screen->availableGeometry();
            const int width=std::clamp(savedWidth,640,std::max(640,area.width()));
            const int height=std::clamp(savedHeight,480,std::max(480,area.height()));
            const int x=std::clamp(savedX,area.left(),std::max(area.left(),area.right()-width+1));
            const int y=std::clamp(savedY,area.top(),std::max(area.top(),area.bottom()-height+1));
            normalGeometry=QRect(x,y,width,height);mainWindow->setGeometry(normalGeometry);
            const int page=static_cast<int>(pageKeys.indexOf(windowSettings->value("page","overview").toString()));
            if(page>=0)mainWindow->setProperty("pageIndex",page);
            if(windowSettings->value("maximized",false).toBool())mainWindow->showMaximized();
        }
    }
    auto rememberGeometry=[&]{if(mainWindow->visibility()==QWindow::Windowed)normalGeometry=mainWindow->geometry();};
    QObject::connect(mainWindow,&QWindow::widthChanged,&app,rememberGeometry);
    QObject::connect(mainWindow,&QWindow::heightChanged,&app,rememberGeometry);
    QObject::connect(mainWindow,&QWindow::xChanged,&app,rememberGeometry);
    QObject::connect(mainWindow,&QWindow::yChanged,&app,rememberGeometry);
    QObject::connect(&app,&QCoreApplication::aboutToQuit,&app,[&]{
        if(!windowSettings)return;
        windowSettings->setValue("formatVersion",1);
        windowSettings->setValue("x",normalGeometry.x());windowSettings->setValue("y",normalGeometry.y());
        windowSettings->setValue("width",normalGeometry.width());windowSettings->setValue("height",normalGeometry.height());
        windowSettings->setValue("maximized",mainWindow->visibility()==QWindow::Maximized);
        const int page=mainWindow->property("pageIndex").toInt();
        windowSettings->setValue("page",pageKeys.value(page,"overview"));windowSettings->sync();
    });
    QPixmap iconPixmap(48,48);iconPixmap.fill(QColor("#1762c0"));
    {QPainter painter(&iconPixmap);painter.setPen(Qt::white);auto font=painter.font();font.setBold(true);font.setPixelSize(21);painter.setFont(font);painter.drawText(iconPixmap.rect(),Qt::AlignCenter,"MW");}
    const QIcon appIcon(iconPixmap);app.setWindowIcon(appIcon);
    QSystemTrayIcon tray(appIcon);tray.setToolTip("MOEXWatch 0.7.0");
    if(!smoke&&QSystemTrayIcon::isSystemTrayAvailable())tray.show();
    QObject::connect(&signalModule,&SignalsController::notification,&app,[&](const QString& title,const QString& body){
        mainWindow->setProperty("notice",title+" · "+body);
        if(tray.isVisible()&&QSystemTrayIcon::supportsMessages())tray.showMessage(title,body,QSystemTrayIcon::Information,10000);
    });
    QObject::connect(&app,&QCoreApplication::aboutToQuit,&events,&EventsController::shutdown);
    QObject::connect(&app,&QCoreApplication::aboutToQuit,&backend,&PortfolioController::shutdown);
    if(smoke) {
        auto* window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());
        if(!window)return 3;
        // Exercise all pages, dialogs, themes and both narrow/wide layouts.
        QTimer::singleShot(100,&app,[window,&backend]{window->resize(760,620);window->setProperty("pageIndex",9);backend.setDark(false);});
        QTimer::singleShot(200,&app,[window]{window->setProperty("pageIndex",11);});
        QTimer::singleShot(300,&app,[window,&backend]{window->resize(1440,920);window->setProperty("pageIndex",0);backend.setDark(true);});
        QTimer::singleShot(400,&app,[window,&errors]{
            auto* dialog=window->findChild<QObject*>("operationDialog");
            if(!dialog||!QMetaObject::invokeMethod(dialog,"open"))++errors;
        });
        QTimer::singleShot(500,&app,[window]{if(auto* dialog=window->findChild<QObject*>("operationDialog"))QMetaObject::invokeMethod(dialog,"close");});
        QTimer::singleShot(600,&app,[window,&backend,&errors]{
            auto* dialog=window->findChild<QObject*>("levelsDialog");
            if(!dialog){++errors;return;}
            dialog->setProperty("positionId",backend.positions()->rows().first()["id"]);
            if(!QMetaObject::invokeMethod(dialog,"open"))++errors;
        });
        QTimer::singleShot(700,&app,[window,&backend,&errors]{
            auto* chart=window->findChild<FinancialChart*>("overviewChart");
            if(!chart||chart->visibleCount()<2){++errors;return;}
            chart->zoomAt(1.5,chart->width()/2);
            chart->panBars(-5);
            chart->inspectAt(chart->width()/2);
            if(chart->hovered().isEmpty())++errors;
            backend.setChartComparison(true);
            chart->resetView();
        });
        QTimer::singleShot(810,&app,[window,&errors]{
            if(auto* levels=window->findChild<QObject*>("levelsDialog"))QMetaObject::invokeMethod(levels,"close");
            auto* dialog=window->findChild<QObject*>("companyEditDialog");
            if(!dialog||!QMetaObject::invokeMethod(dialog,"open"))++errors;
        });
        QTimer::singleShot(890,&app,[window]{if(auto* dialog=window->findChild<QObject*>("companyEditDialog"))QMetaObject::invokeMethod(dialog,"close");});
        QTimer::singleShot(1220,&app,[window,&errors]{
            const char* fields[]={"purchaseEntry","purchaseLot","purchaseStop","purchaseTarget"};
            const char* values[]={"100","10","90","120"};
            for(int i=0;i<4;++i){auto* field=window->findChild<QObject*>(fields[i]);if(field)field->setProperty("text",values[i]);else++errors;}
            auto* calculate=window->findChild<QObject*>("purchaseCalculate");
            if(!calculate||!QMetaObject::invokeMethod(calculate,"clicked"))++errors;
        });
        for(int page=1;page<12;++page)QTimer::singleShot(900+page*80,&app,[window,page]{window->setProperty("pageIndex",page);});
        QTimer::singleShot(2000,&app,[&]{app.exit(errors?4:0);});
    }
    const int screenshot=arguments.indexOf("--screenshot");
    if(screenshot>=0&&screenshot+1<arguments.size()) {
        if(auto* window=qobject_cast<QQuickWindow*>(engine.rootObjects().first())) {
            window->resize(arguments.contains("--narrow")?760:1440,arguments.contains("--narrow")?620:920);
            if(arguments.contains("--journal"))window->setProperty("pageIndex",9);
            if(arguments.contains("--portfolio"))window->setProperty("pageIndex",8);
            if(arguments.contains("--comparison"))backend.setChartComparison(true);
            const int pageFlag=arguments.indexOf("--page");
            if(pageFlag>=0&&pageFlag+1<arguments.size()) {
                bool ok=false;const int page=arguments[pageFlag+1].toInt(&ok);
                if(ok&&page>=0&&page<12)window->setProperty("pageIndex",page);
            }
            if(arguments.contains("--operation-dialog"))QTimer::singleShot(100,&app,[window]{
                if(auto* dialog=window->findChild<QObject*>("operationDialog"))QMetaObject::invokeMethod(dialog,"open");
            });
        }
        const auto path=arguments[screenshot+1];
        QTimer::singleShot(800,&app,[&,path]{
            auto* window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());
            const bool saved=window&&!window->grabWindow().isNull()&&window->grabWindow().save(path);
            app.exit(saved&&errors==0?0:5);
        });
    }
    const int exitCode=app.exec();
    QObject::disconnect(mainWindow,nullptr,&app,nullptr);
    QObject::disconnect(&signalModule,nullptr,&app,nullptr);
    return exitCode;
}
