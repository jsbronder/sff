#include <QApplication>
#include <QClipboard>
#include <QDebug>
#include <QMainWindow>
#include <QRegularExpression>
#include <QShortcut>

#include <qvterm.hpp>

// clang-format off
constexpr const char *urlMatch =
    R"((?:https?://|ftp://|news://|mailto:|file://|\bwww\.))"
    R"([\w\-\@;\/?:&=%\$.+!*\x27,~#]*)"
    "("
        R"(\([\w\-\@;\/?:&=%\$.+!*\x27,~#]*\))"
        "|"
        R"([\w\-\@;\/?:&=%\$+*~])"
    ")+";
// clang-format on

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

    bool matching = false;
    QShortcut esc{QKeySequence{Qt::Key_Escape}, &win};
    win.connect(&esc, &QShortcut::activated, [&qvterm, &esc, &matching]() {
        qvterm.matchClear();
        esc.setEnabled(false);
        matching = false;
    });
    esc.setEnabled(false);

    QShortcut findUrl{QKeySequence{Qt::CTRL + Qt::Key_M}, &win};
    win.connect(&findUrl, &QShortcut::activated, [&qvterm, &matching, &esc]() {
        static QRegularExpression re(urlMatch,
                QRegularExpression::DotMatchesEverythingOption | QRegularExpression::CaseInsensitiveOption);

        esc.setEnabled(true);
        if (matching) {
            qvterm.matchNext();
        } else {
            matching = true;
            qvterm.match(&re);
        }
    });

    app.exec();
}
