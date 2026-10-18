#include "portfolio_controller.hpp"
#include "chart_item.hpp"
#include "window_geometry.hpp"
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
#include "portfolio_transfer_controller.hpp"
#include "portfolio_dividends_controller.hpp"
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QQuickItem>
#include <QTemporaryDir>
#include <QTimer>
#include <QImage>
#include <cstdio>
#include <algorithm>

int main(int argc,char* argv[]) {
    QApplication app(argc,argv);
    app.setQuitOnLastWindowClosed(true);
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
    PortfolioTransferController transfer(&backend);
    PortfolioDividendsController dividends(&backend);
    StrategyController strategies(&backend);
    strategies.setDividendSource([&dividends](std::string_view ticker){return dividends.backtestDividends(std::string(ticker));});
    QObject::connect(&dividends,&PortfolioDividendsController::changed,&strategies,&StrategyController::invalidateDividendData);
    QObject::connect(&events,&EventsController::snapshotChanged,&dividends,&PortfolioDividendsController::reload);
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("backend",&backend);
    engine.rootContext()->setContextProperty("signalBackend",&signalModule);
    engine.rootContext()->setContextProperty("decisionBackend",&decisions);
    engine.rootContext()->setContextProperty("researchBackend",&research);
    engine.rootContext()->setContextProperty("eventsBackend",&events);
    engine.rootContext()->setContextProperty("strategyBackend",&strategies);
    engine.rootContext()->setContextProperty("transferBackend",&transfer);
    engine.rootContext()->setContextProperty("dividendPortfolioBackend",&dividends);
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
            normalGeometry=restoreWindowGeometry(QRect(savedX,savedY,savedWidth,savedHeight),area,mainWindow->frameMargins());
            mainWindow->setGeometry(normalGeometry);
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
    QSystemTrayIcon tray(appIcon);tray.setToolTip("MOEXWatch 0.8.0");
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
        if(!(window->flags()&Qt::WindowCloseButtonHint)||!(window->flags()&Qt::FramelessWindowHint))++errors;
        // Repeater delegates are owned by the visual tree, not the window QObject tree.
        const auto findVisual = [](auto&& self,QQuickItem* parent,const QString& name)->QQuickItem* {
            if(parent->objectName()==name)return parent;
            for(auto* child:parent->childItems())if(auto* found=self(self,child,name))return found;
            return nullptr;
        };
        for(int edge=0;edge<8;++edge) {
            auto* grip=findVisual(findVisual,window->contentItem(),"windowResize"+QString::number(edge));
            if(!grip||!grip->property("enabled").toBool()||grip->width()<=0||grip->height()<=0)++errors;
        }
        QTimer::singleShot(150,&app,[window,&errors,findVisual]{
            for(const auto* name:{"headerDragArea","logoDragArea","topDragArea"}) {
                auto* drag=findVisual(findVisual,window->contentItem(),QString::fromLatin1(name));
                if(!drag||!drag->isVisible()||drag->width()<=0||drag->height()<=0) {
                    qWarning()<<"Invalid window drag area"<<name;++errors;continue;
                }
                const QRectF dragBounds(drag->mapToScene(QPointF()),drag->size());
                for(const auto* buttonName:{"windowMinimize","windowMaximize","windowClose"}) {
                    auto* button=window->findChild<QQuickItem*>(QString::fromLatin1(buttonName));
                    if(!button||dragBounds.intersects(QRectF(button->mapToScene(QPointF()),button->size()))) {
                        qWarning()<<"Window drag area overlaps controls"<<name<<buttonName;++errors;
                    }
                }
            }
        });
        QTimer::singleShot(20,&app,[window,&errors]{auto* button=window->findChild<QObject*>("windowMaximize");if(!button||!QMetaObject::invokeMethod(button,"clicked"))++errors;});
        QTimer::singleShot(40,&app,[window,&errors,findVisual]{
            if(window->visibility()!=QWindow::Maximized)++errors;
            for(int edge=0;edge<8;++edge) {
                auto* grip=findVisual(findVisual,window->contentItem(),"windowResize"+QString::number(edge));
                if(!grip||grip->property("enabled").toBool()) {
                    qWarning()<<"Resize enabled while maximized"<<edge;++errors;
                }
            }
            if(auto* button=window->findChild<QObject*>("windowMaximize"))QMetaObject::invokeMethod(button,"clicked");
        });
        QTimer::singleShot(60,&app,[window,&errors]{if(window->visibility()!=QWindow::Windowed)++errors;auto* button=window->findChild<QObject*>("windowMinimize");if(!button||!QMetaObject::invokeMethod(button,"clicked"))++errors;});
        QTimer::singleShot(80,&app,[window,&errors]{if(window->visibility()!=QWindow::Minimized)++errors;window->showNormal();});
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
        QTimer::singleShot(1810,&app,[window,&errors]{
            auto* dialog=window->findChild<QObject*>("portfolioTransferDialog");
            if(!dialog||!QMetaObject::invokeMethod(dialog,"open"))++errors;
        });
        QTimer::singleShot(1930,&app,[window]{if(auto* dialog=window->findChild<QObject*>("portfolioTransferDialog"))QMetaObject::invokeMethod(dialog,"close");});
        QTimer::singleShot(2000,&app,[window,&app,&errors]{
            auto* close=window->findChild<QObject*>("windowClose");
            if(errors||!close||!QMetaObject::invokeMethod(close,"clicked"))app.exit(4);
        });
        QTimer::singleShot(4000,&app,[&app]{app.exit(6);});
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
