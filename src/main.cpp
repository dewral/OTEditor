#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>
#include <QDir>
#include <QLocale>
#include "editorbackend.h"
int main(int argc,char **argv){
    QGuiApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QGuiApplication app(argc,argv);
    QLocale::setDefault(QLocale(QLocale::English, QLocale::UnitedStates));
    app.setOrganizationName("Dewral");app.setApplicationName("OTEditor");
    QQuickStyle::setStyle("Basic");
    EditorBackend backend;QQmlApplicationEngine engine;
    engine.addImageProvider("itempreview",new EditorImageProvider(&backend));
    engine.rootContext()->setContextProperty("Backend",&backend);
    const auto args=app.arguments();
    int folder=args.indexOf("--folder");int version=args.indexOf("--version");
    if(folder>=0&&folder+1<args.size())backend.openFolder(args[folder+1],version>=0&&version+1<args.size()?args[version+1].toInt():1098);
    engine.loadFromModule("OTEditor","Main");if(engine.rootObjects().isEmpty())return 1;
    int category=args.indexOf("--category");
    if(category>=0&&category+1<args.size())backend.setCategory(args[category+1].toInt());
    int selectId=args.indexOf("--select-id");
    if(selectId>=0&&selectId+1<args.size())backend.jump(args[selectId+1].toInt());
    int objectSize=args.indexOf("--object-size");
    if(objectSize>=0&&objectSize+1<args.size())
        if(auto control=engine.rootObjects().first()->findChild<QObject *>("objectSize"))
            control->setProperty("value",args[objectSize+1].toInt());
    int objectColumns=args.indexOf("--object-columns");
    if(objectColumns>=0&&objectColumns+1<args.size())
        if(auto control=engine.rootObjects().first()->findChild<QObject *>("objectColumns"))
            control->setProperty("value",args[objectColumns+1].toInt());
    int capture=args.indexOf("--screenshot");
    int inspect=args.indexOf("--inspect-tab");
    if(capture>=0 && inspect>=0 && inspect+1<args.size()) {
        if(auto dialog=engine.rootObjects().first()->findChild<QObject *>("objectInspector")) {
            dialog->setProperty("tabIndex",qBound(0,args[inspect+1].toInt(),2));
        }
    }
    if(capture>=0&&capture+1<args.size())QTimer::singleShot(1600,&app,[&]{
        auto window=qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        app.exit(window&&window->grabWindow().save(args[capture+1])?0:2);
    });
    return app.exec();
}
