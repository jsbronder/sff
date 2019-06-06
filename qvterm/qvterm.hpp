#pragma once

#include <memory>

#include <QAbstractScrollArea>
#include <QContiguousCache>
#include <QString>

extern "C" {
#include <vterm.h>
}

class QKeyEvent;
class QResizeEvent;
class QSocketNotifier;
class QWidget;

class Highlight;
class Scrollback;

class QVTerm : public QAbstractScrollArea {
    Q_OBJECT
public:
    explicit QVTerm(QWidget *parent = nullptr);
    ~QVTerm();

    void scrollPage(int pages);
    void setFont(const QFont &font);
    void start();

signals:
    void iconTextChanged(QString iconText);
    void titleChanged(QString title);

protected:
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

    void scrollContentsBy(int dx, int dy) override;

private:
    // VTermScreenCallbacks
    int damage(VTermRect rect);
    int moverect(VTermRect dest, VTermRect src);
    int movecursor(VTermPos pos, VTermPos oldpos, int visible);
    int settermprop(VTermProp prop, VTermValue *val);
    int sb_pushline(int cols, const VTermScreenCell *cells);
    int sb_popline(int cols, VTermScreenCell *cells);

    void copyToClipboard();

    /**
     * Fetch a cell
     *
     * @param x - x coordinate in VTerm space
     * @param y - y coordinate in VTerm space
     *
     * @return  - VTermScreenCell for the requested location or an empty one if
     *            fetching failed.
     **/
    const VTermScreenCell *fetchCell(int x, int y) const;

    void flushToPty();
    void onPtyInput(int fd);
    void pasteFromClipboard();
    void repaintCursor();

private:
    VTerm *m_vterm;
    VTermScreen *m_vtermScreen;

    int m_pty{-1};
    QSocketNotifier *m_ptysn;
    QByteArray m_ptyPending{};

    QFont m_font;
    QSize m_cellSize;
    bool m_altscreen{false};
    bool m_ignoreScroll{false};

    struct {
        int row;
        int col;
        bool visible;
    } m_cursor;

    std::unique_ptr<Highlight> m_highlight;
    std::unique_ptr<Scrollback> m_scrollback;
};
