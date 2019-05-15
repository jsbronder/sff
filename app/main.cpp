#include <QApplication>
#include <QMainWindow>
#include <QShortcut>

#include <qvterm.hpp>

int main(int argc, char **argv)
{
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling, true);
    QApplication app(argc, argv);

    QMainWindow win{};
    QVTerm qvterm{};

    win.setCentralWidget(&qvterm);
    win.show();

    qvterm.start();
    qvterm.setFocus();

    win.connect(&qvterm, &QVTerm::titleChanged, [&win](QString title) {
        win.setWindowTitle(title);
    });
    win.connect(&qvterm, &QVTerm::iconTextChanged, [&win](QString iconText) {
        win.setWindowIconText(iconText);
    });

    QShortcut pgup{QKeySequence{Qt::SHIFT + Qt::Key_PageUp}, &win};
    win.connect(&pgup, &QShortcut::activated, [&qvterm]() {
        qvterm.scrollPage(1);
    });
    QShortcut pgdown{QKeySequence{Qt::SHIFT + Qt::Key_PageDown}, &win};
    win.connect(&pgdown, &QShortcut::activated, [&qvterm]() {
        qvterm.scrollPage(-1);
    });

    app.exec();
}

