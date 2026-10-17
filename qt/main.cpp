#include "portfolio_controller.hpp"
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
    if(smoke||demo)root=temporary.path().toStdString();
    int errors=0;
    QObject::connect(&app,&QCoreApplication::aboutToQuit,[]{});
    PortfolioController backend(root);
    if(smoke||demo)backend.loadDemo();
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
        QTimer::singleShot(100,&app,[window,&backend]{window->resize(760,620);window->setProperty("pageIndex",1);backend.setDark(false);});
        QTimer::singleShot(200,&app,[window]{window->setProperty("pageIndex",2);});
        QTimer::singleShot(300,&app,[window,&backend]{window->resize(1440,920);window->setProperty("pageIndex",0);backend.setDark(true);});
        QTimer::singleShot(600,&app,[&]{app.exit(errors?4:0);});
    }
    const int screenshot=arguments.indexOf("--screenshot");
    if(screenshot>=0&&screenshot+1<arguments.size()) {
        const auto path=arguments[screenshot+1];
        QTimer::singleShot(800,&app,[&,path]{
            auto* window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());
            const bool saved=window&&!window->grabWindow().isNull()&&window->grabWindow().save(path);
            app.exit(saved&&errors==0?0:5);
        });
    }
    return app.exec();
}
