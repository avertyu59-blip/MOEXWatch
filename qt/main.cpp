#include "portfolio_controller.hpp"
#include "chart_item.hpp"
#include <qqml.h>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QTimer>
#include <QImage>
#include <cstdio>

int main(int argc,char* argv[]) {
    QGuiApplication app(argc,argv);
    QCoreApplication::setOrganizationName("MOEXWatch");
    QCoreApplication::setApplicationName("MOEXWatch Qt Preview");
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
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("backend",&backend);
    QObject::connect(&engine,&QQmlApplicationEngine::warnings,&app,[&](const QList<QQmlError>& warnings){
        for(const auto& warning:warnings){std::fprintf(stderr,"%s\n",qPrintable(warning.toString()));++errors;}
    });
    engine.loadFromModule("MOEXWatch","Main");
    if(engine.rootObjects().isEmpty())return 2;
    QObject::connect(&app,&QCoreApplication::aboutToQuit,&backend,&PortfolioController::shutdown);
    if(smoke) {
        auto* window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());
        if(!window)return 3;
        // Exercise all pages, dialogs, themes and both narrow/wide layouts.
        QTimer::singleShot(100,&app,[window,&backend]{window->resize(760,620);window->setProperty("pageIndex",2);backend.setDark(false);});
        QTimer::singleShot(200,&app,[window]{window->setProperty("pageIndex",3);});
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
        QTimer::singleShot(900,&app,[&]{app.exit(errors?4:0);});
    }
    const int screenshot=arguments.indexOf("--screenshot");
    if(screenshot>=0&&screenshot+1<arguments.size()) {
        if(auto* window=qobject_cast<QQuickWindow*>(engine.rootObjects().first())) {
            window->resize(arguments.contains("--narrow")?760:1440,arguments.contains("--narrow")?620:920);
            if(arguments.contains("--journal"))window->setProperty("pageIndex",2);
            if(arguments.contains("--portfolio"))window->setProperty("pageIndex",1);
            if(arguments.contains("--comparison"))backend.setChartComparison(true);
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
    return app.exec();
}
