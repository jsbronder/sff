#pragma once

#include "region.hpp"

#include <memory>

#include <QAbstractScrollArea>
#include <QContiguousCache>
#include <QString>

extern "C" {
#include <vterm.h>
}

class QKeyEvent;
class QRegularExpression;
class QRegularExpressionMatchIterator;
class QResizeEvent;
class QSocketNotifier;
class QWidget;

class Highlight;
class Region;
class Scrollback;

class QVTerm : public QAbstractScrollArea {
    Q_OBJECT
public:
    explicit QVTerm(QWidget *parent = nullptr);
    ~QVTerm();

    /**
     * Find matches for the given regular expression on the cells that are
     * currently visible (not in the scrollback buffer).
     *
     * If there are any matches, the first one (starting from the bottom of the
     * screen) will be highlighted.  Successive calls to matchNext() will cycle
     * through all visible matches.
     *
     * @param regexp    - Regular expression to search for
     **/
    void match(const QRegularExpression *regexp);

    /**
     * Clear the current match, if any
     **/
    void matchClear();

    /**
     * Cycle to the next match if the matcher is active
     **/
    void matchNext();

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

    /**
     * Convert VTerm coordinates to Qt pixel space
     **/
    QRect pixelRect(const VTermRect &rect) const;
    QRect pixelRect(int x, int y, int width, int height) const;
    int pixelCol(int x) const;
    int pixelRow(int y) const;

private:
    VTerm *m_vterm;
    VTermScreen *m_vtermScreen;
    QSize m_vtermSize;

    int m_pty{-1};
    QSocketNotifier *m_ptysn;
    QByteArray m_ptyPending{};

    QFont m_font;
    QSize m_cellSize;
    int m_cellBaseline;
    bool m_altscreen{false};
    bool m_ignoreScroll{false};

    struct {
        int row;
        int col;
        bool visible;
    } m_cursor;

    std::unique_ptr<Highlight> m_highlight;
    std::unique_ptr<Scrollback> m_scrollback;

    std::vector<Region> m_matches;
    std::vector<Region>::const_reverse_iterator m_match;
};
