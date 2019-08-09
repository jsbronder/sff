#include "qvterm.hpp"
#include "highlight.hpp"
#include "region.hpp"
#include "scrollback.hpp"

#include <QAbstractScrollArea>
#include <QApplication>
#include <QClipboard>
#include <QCoreApplication>
#include <QDebug>
#include <QKeyEvent>
#include <QPainter>
#include <QRegularExpression>
#include <QScrollBar>
#include <QSocketNotifier>

#include <QElapsedTimer>
#include <QTextLayout>

#include <csignal>

#include <fcntl.h>
#include <pty.h>
#include <sys/time.h>
#include <termios.h>
#include <unistd.h>

namespace {
VTermModifier vtermModifier(int mod)
{
    int ret = VTERM_MOD_NONE;

    if (mod & Qt::SHIFT)
        ret |= VTERM_MOD_SHIFT;

    if (mod & Qt::ALT)
        ret |= VTERM_MOD_ALT;

    if (mod & Qt::CTRL)
        ret |= VTERM_MOD_CTRL;

    return static_cast<VTermModifier>(ret);
}

VTermKey vtermKey(int key, bool keypad)
{
    if (key >= Qt::Key_F1 && key <= Qt::Key_F35)
        return static_cast<VTermKey>(VTERM_KEY_FUNCTION_0 + key - Qt::Key_F1 + 1);

    switch (key) {
        case Qt::Key_Return:
            return VTERM_KEY_ENTER;
        case Qt::Key_Tab:
            return VTERM_KEY_TAB;
        case Qt::Key_Backspace:
            return VTERM_KEY_BACKSPACE;
        case Qt::Key_Escape:
            return VTERM_KEY_ESCAPE;
        case Qt::Key_Up:
            return VTERM_KEY_UP;
        case Qt::Key_Down:
            return VTERM_KEY_DOWN;
        case Qt::Key_Left:
            return VTERM_KEY_LEFT;
        case Qt::Key_Right:
            return VTERM_KEY_RIGHT;
        case Qt::Key_Insert:
            return VTERM_KEY_INS;
        case Qt::Key_Delete:
            return VTERM_KEY_DEL;
        case Qt::Key_Home:
            return VTERM_KEY_HOME;
        case Qt::Key_End:
            return VTERM_KEY_END;
        case Qt::Key_PageUp:
            return keypad ? VTERM_KEY_PAGEUP : VTERM_KEY_NONE;
        case Qt::Key_PageDown:
            return keypad ? VTERM_KEY_PAGEDOWN : VTERM_KEY_NONE;
        case Qt::Key_multiply:
            return keypad ? VTERM_KEY_KP_MULT : VTERM_KEY_NONE;
        case Qt::Key_Plus:
            return keypad ? VTERM_KEY_KP_PLUS : VTERM_KEY_NONE;
        case Qt::Key_Comma:
            return keypad ? VTERM_KEY_KP_COMMA : VTERM_KEY_NONE;
        case Qt::Key_Minus:
            return keypad ? VTERM_KEY_KP_MINUS : VTERM_KEY_NONE;
        case Qt::Key_Period:
            return keypad ? VTERM_KEY_KP_PERIOD : VTERM_KEY_NONE;
        case Qt::Key_Slash:
            return keypad ? VTERM_KEY_KP_DIVIDE : VTERM_KEY_NONE;
        case Qt::Key_Enter:
            return keypad ? VTERM_KEY_KP_ENTER : VTERM_KEY_NONE;
        case Qt::Key_Equal:
            return keypad ? VTERM_KEY_KP_EQUAL : VTERM_KEY_NONE;
        default:
            return VTERM_KEY_NONE;
    }
}
} // namespace

QVTerm::QVTerm(QWidget *parent) :
    QAbstractScrollArea(parent),
    m_vterm(vterm_new(size().height(), size().width())),
    m_vtermScreen(vterm_obtain_screen(m_vterm)),
    m_highlight(std::make_unique<Highlight>()),
    m_scrollback(std::make_unique<Scrollback>(5000))
{
    vterm_set_utf8(m_vterm, true);

    // clang-format off
    static const VTermScreenCallbacks vtcbs = {
        .damage = [](VTermRect rect, void *user) {
            auto p = static_cast<QVTerm*>(user);
            return p->damage(rect);
        },
        .moverect = [](VTermRect dest, VTermRect src, void *user) {
            auto p = static_cast<QVTerm*>(user);
            return p->moverect(dest, src);
        },
        .movecursor = [](VTermPos pos, VTermPos oldpos, int visible, void *user) {
            auto p = static_cast<QVTerm*>(user);
            return p->movecursor(pos, oldpos, visible);
        },
        .settermprop = [](VTermProp prop, VTermValue *val, void *user) {
            auto p = static_cast<QVTerm*>(user);
            return p->settermprop(prop, val);
        },
        .bell = nullptr,
        .resize = nullptr,
        .sb_pushline = [](int cols, const VTermScreenCell *cells, void *user) {
            auto p = static_cast<QVTerm*>(user);
            return p->sb_pushline(cols, cells);
        },
        .sb_popline = [](int cols, VTermScreenCell *cells, void *user) {
            auto p = static_cast<QVTerm*>(user);
            return p->sb_popline(cols, cells);
        },
    };
    // clang-format on
    vterm_screen_set_callbacks(m_vtermScreen, &vtcbs, this);
    vterm_screen_set_damage_merge(m_vtermScreen, VTERM_DAMAGE_SCROLL);
    vterm_screen_enable_altscreen(m_vtermScreen, true);

    static auto on_output = [](const char *s, size_t len, void *user) {
        auto p = static_cast<QVTerm *>(user);
        p->m_ptyPending.append(s, static_cast<int>(len));
    };
    vterm_output_set_callback(m_vterm, on_output, this);

    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setFrameStyle(QFrame::NoFrame);
    setAttribute(Qt::WA_OpaquePaintEvent);

    setFont(QFont("Monospace", 8));
    setFocus();

    VTermState *vts = vterm_obtain_state(m_vterm);
    vterm_state_set_bold_highbright(vts, true);

    auto setColor = [vts](int index, uint8_t r, uint8_t g, uint8_t b) {
        VTermColor col;
        vterm_color_rgb(&col, r, g, b);
        vterm_state_set_palette_color(vts, index, &col);
    };
    setColor(0, 0x66, 0x66, 0x66);
    setColor(8, 0x66, 0x66, 0x66);
    setColor(1, 0x9e, 0x18, 0x28);
    setColor(9, 0xcf, 0x61, 0x71);
    setColor(2, 0xae, 0xce, 0x92);
    setColor(10, 0xc5, 0xf7, 0x79);
    setColor(3, 0x96, 0x8a, 0x38);
    setColor(11, 0xff, 0xf7, 0x96);
    setColor(4, 0x4e, 0x78, 0xa0);
    setColor(12, 0x41, 0x86, 0xbe);
    setColor(5, 0x96, 0x3c, 0x59);
    setColor(13, 0xcf, 0x9e, 0xbe);
    setColor(6, 0x41, 0x81, 0x79);
    setColor(14, 0x71, 0xbe, 0xbe);
    setColor(7, 0xbe, 0xbe, 0xbe);
    setColor(15, 0xff, 0xff, 0xff);

    VTermColor fg;
    VTermColor bg;
    vterm_color_rgb(&fg, 0xbe, 0xbe, 0xbe);
    vterm_color_rgb(&bg, 0x26, 0x26, 0x26);
    vterm_state_set_default_colors(vts, &fg, &bg);

    vterm_screen_reset(m_vtermScreen, 1);
}

QVTerm::~QVTerm()
{
    vterm_free(m_vterm);
}

const VTermScreenCell *QVTerm::fetchCell(int x, int y) const
{
    static VTermScreenCell emptyCell{};
    emptyCell.width = 1;
    emptyCell.chars[0] = '\0';

    if (y < 0) {
        // row -1 == m_sb[0], row -2 == m_sb[1]
        size_t sbrow = (y + 1) * -1;
        if (sbrow >= m_scrollback->size()) {
            qDebug() << "Scrollback fetch row" << sbrow
                     << "greater than scrollback size" << m_scrollback->size();
            return &emptyCell;
        }

        const auto &sbl = m_scrollback->line(sbrow);
        if (x < sbl.cols()) {
            return sbl.cell(x);
        } else {
            return &emptyCell;
        }
    }

    static VTermScreenCell refCell{};
    VTermPos vtp{y, x};
    vterm_screen_get_cell(m_vtermScreen, vtp, &refCell);
    vterm_screen_convert_color_to_rgb(m_vtermScreen, &refCell.fg);
    vterm_screen_convert_color_to_rgb(m_vtermScreen, &refCell.bg);
    return &refCell;
};

void QVTerm::match(const QRegularExpression *regexp)
{
    int width = termSize().width();
    int height = termSize().height();
    QString dump = Region({0, 0}, {width, height}).dump(termSize(), [this](int x, int y) {
        return fetchCell(x, y);
    });

    m_matches.clear();
    auto matchIter = regexp->globalMatch(dump);
    while (matchIter.isValid() && matchIter.hasNext()) {
        auto match = matchIter.next();
        int capturedEnd = match.capturedEnd() - 1;

        int yStart = match.capturedStart() / width;
        int yEnd = capturedEnd / width;
        int xStart = match.capturedStart() % width;
        int xEnd = capturedEnd % width;
        m_matches.emplace_back(QPoint{xStart, yStart}, QPoint{xEnd, yEnd});
    }
    m_match = m_matches.crbegin();

    if (m_match != m_matches.crend()) {
        auto *cb = QApplication::clipboard();
        QString matched = m_match->dumpString(termSize(), [this](int x, int y) {
            return fetchCell(x, y);
        });
        cb->setText(matched, QClipboard::Selection);
        viewport()->update(m_match->pixelRect(termSize(), m_cellSize));
    }
}

void QVTerm::matchClear()
{
    if (m_match != m_matches.crend())
        viewport()->update(m_match->pixelRect(termSize(), m_cellSize));
    m_matches.clear();
    m_match = m_matches.crend();
}

void QVTerm::matchNext()
{
    if (m_matches.empty())
        return;

    viewport()->update(m_match->pixelRect(termSize(), m_cellSize));
    m_match++;

    if (m_match == m_matches.crend())
        m_match = m_matches.crbegin();

    if (m_match != m_matches.crend()) {
        auto *cb = QApplication::clipboard();
        QString matched = m_match->dumpString(termSize(), [this](int x, int y) {
            return fetchCell(x, y);
        });
        cb->setText(matched, QClipboard::Selection);
        viewport()->update(m_match->pixelRect(termSize(), m_cellSize));
    }
}

void QVTerm::scrollPage(int pages)
{
    int delta = size().height() * pages / m_cellSize.height() / 2;
    scrollContentsBy(0, delta);
}

void QVTerm::setFont(const QFont &font)
{
    m_font = font;
    QFontMetrics qfm{m_font};
    m_cellSize = {qfm.averageCharWidth(), qfm.ascent() + qfm.descent()};
    QAbstractScrollArea::setFont(m_font);
}

void QVTerm::start()
{
    struct termios termios;
    bzero(&termios, sizeof(struct termios));
    termios.c_iflag = ICRNL | IXON | IUTF8,
    termios.c_oflag = OPOST | ONLCR | TAB0 | NL0 | CR0 | BS0 | VT0 | FF0,
    termios.c_cflag = CS8 | CREAD,
    termios.c_lflag = ISIG | ICANON | IEXTEN | ECHO | ECHOE | ECHOK,
    termios.c_cc[VINTR] = 0x1f & 'C';
    termios.c_cc[VQUIT] = 0x1f & '\\';
    termios.c_cc[VERASE] = 0x7f;
    termios.c_cc[VKILL] = 0x1f & 'U';
    termios.c_cc[VEOF] = 0x1f & 'D';
    termios.c_cc[VEOL] = _POSIX_VDISABLE;
    termios.c_cc[VEOL2] = _POSIX_VDISABLE;
    termios.c_cc[VSTART] = 0x1f & 'Q';
    termios.c_cc[VSTOP] = 0x1f & 'S';
    termios.c_cc[VSUSP] = 0x1f & 'Z';
    termios.c_cc[VREPRINT] = 0x1f & 'R';
    termios.c_cc[VWERASE] = 0x1f & 'W';
    termios.c_cc[VLNEXT] = 0x1f & 'V';
    termios.c_cc[VMIN] = 1;
    termios.c_cc[VTIME] = 0;

    cfsetspeed(&termios, 38400);

    struct winsize wsz = {
            .ws_row = static_cast<short unsigned int>(size().height()),
            .ws_col = static_cast<short unsigned int>(size().width()),
            .ws_xpixel = 0,
            .ws_ypixel = 0,
    };

    pid_t child = forkpty(&m_pty, nullptr, &termios, &wsz);
    if (child == 0) {
        signal(SIGINT, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);
        signal(SIGSTOP, SIG_DFL);
        signal(SIGCONT, SIG_DFL);

        putenv(const_cast<char *>("TERM=xterm-256color"));
        putenv(const_cast<char *>("COLORTERM=truecolor"));
        char *shell = getenv("SHELL");
        char *const args[] = {shell, nullptr};
        execvp(shell, args);
    }
    fcntl(m_pty, F_SETFL, fcntl(m_pty, F_GETFL) | O_NONBLOCK);
    m_ptysn = new QSocketNotifier(m_pty, QSocketNotifier::Read, this);
    connect(m_ptysn, &QSocketNotifier::activated, [this](int fd) {
        onPtyInput(fd);
    });
}

QSize QVTerm::termSize() const
{
    return {
            size().width() / m_cellSize.width(),
            size().height() / m_cellSize.height()};
}

void QVTerm::focusInEvent(QFocusEvent *event)
{
    event->accept();

    VTermState *state = vterm_obtain_state(m_vterm);
    vterm_state_focus_in(state);
    repaintCursor();
}

void QVTerm::focusOutEvent(QFocusEvent *event)
{
    event->accept();

    VTermState *state = vterm_obtain_state(m_vterm);
    vterm_state_focus_out(state);
    repaintCursor();
}

void QVTerm::keyPressEvent(QKeyEvent *event)
{
    event->accept();

    if (event->key() == Qt::Key_Insert && event->modifiers() & Qt::SHIFT) {
        pasteFromClipboard();
        return;
    }

    bool keypad = event->modifiers() & Qt::KeypadModifier;
    VTermModifier mod = vtermModifier(event->modifiers());
    VTermKey key = vtermKey(event->key(), keypad);

    if (key != VTERM_KEY_NONE) {
        vterm_keyboard_key(m_vterm, key, mod);
        m_scrollback->unscroll();
    } else if (event->text().length()) {
        // This maps to delete word and is way to easy to mistakenly type
        if (event->key() == Qt::Key_Space && mod == VTERM_MOD_SHIFT)
            mod = VTERM_MOD_NONE;

        // Default to nativeVirtualKey which will send <modifier>+<key> rather
        // than text() which will map the combination (meaning that the shell
        // will get <modifier>+<<modifier>+<key>>.
        if (event->nativeVirtualKey()) {
            vterm_keyboard_unichar(
                    m_vterm,
                    event->nativeVirtualKey(),
                    mod);
        } else {
            vterm_keyboard_unichar(
                    m_vterm,
                    event->text().toUcs4()[0],
                    mod);
        }
        m_scrollback->unscroll();
    }

    flushToPty();
}

void QVTerm::mouseMoveEvent(QMouseEvent *event)
{
    event->accept();
    if (!QRect(QPoint(), size()).contains(event->pos()))
        return;

    m_highlight->update(
            event->pos().x() / m_cellSize.width(),
            (event->pos().y() / m_cellSize.height()) - static_cast<int>(m_scrollback->offset()));
    viewport()->update();
}

void QVTerm::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::MidButton) {
        event->accept();
        pasteFromClipboard();
        return;
    }

    if (event->button() == Qt::LeftButton) {
        event->accept();
        m_highlight->anchor(
                event->pos().x() / m_cellSize.width(),
                (event->pos().y() / m_cellSize.height()) - static_cast<int>(m_scrollback->offset()));
        viewport()->update();
    }
}

void QVTerm::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_highlight->active()) {
        event->accept();
        copyToClipboard();
    }
}

void QVTerm::paintEvent(QPaintEvent *event)
{
    event->accept();

    //    QElapsedTimer timer;
    //    timer.start();
    QPainter p(viewport());
    p.setCompositionMode(QPainter::CompositionMode_Source);

    int startCol = event->rect().x() / m_cellSize.width();
    int endCol = startCol + event->rect().width() / m_cellSize.width();
    int startRow = event->rect().y() / m_cellSize.height();
    int endRow = startRow + event->rect().height() / m_cellSize.height();

    //qDebug() << "sc:" << startCol
    //    << "ec:" << endCol
    //    << "sr:" << startRow
    //    << "er:" << endRow;

    static auto colorEqual = [](const QColor &qc, const VTermColor &vc) {
        return (qc.red() == vc.rgb.red
                && qc.green() == vc.rgb.green
                && qc.blue() == vc.rgb.blue);
    };

    static auto toQColor = [](const VTermColor &c) -> QColor {
        return QColor(qRgb(c.rgb.red, c.rgb.green, c.rgb.blue));
    };

    VTermColor defaultBg;
    VTermColor defaultFg;
    vterm_state_get_default_colors(vterm_obtain_state(m_vterm), &defaultFg, &defaultBg);
    // We want to compare the cell bg against this later and cells don't set DEFAULT_BG
    defaultBg.type = VTERM_COLOR_RGB;

    QFont fnt{m_font};
    p.setFont(fnt);
    p.setPen(toQColor(defaultFg));
    p.fillRect(event->rect(), toQColor(defaultBg));

    for (int row = startRow; row < endRow; row++) {
        int phyrow = row - static_cast<int>(m_scrollback->offset());
        int y = row * m_cellSize.height();

        for (int col = startCol; col < endCol; ++col) {
            const VTermScreenCell *cell = fetchCell(col, phyrow);
            const VTermColor *bg = &cell->bg;
            const VTermColor *fg = &cell->fg;
            int x = col * m_cellSize.width();
            bool highlight = (m_highlight->contains(col, phyrow)
                    || (m_match != m_matches.crend() && m_match->contains(col, phyrow)));

            if (static_cast<bool>(cell->attrs.reverse) || highlight) {
                bg = &cell->fg;
                fg = &cell->bg;
            }

            if (!vterm_color_is_equal(bg, &defaultBg)) {
                p.fillRect(
                        x,
                        y,
                        cell->width * m_cellSize.width(),
                        m_cellSize.height(),
                        toQColor(*bg));
            }

            if (!cell->chars[0])
                continue;

            if (!colorEqual(p.pen().color(), *fg)) {
                p.setPen(toQColor(*fg));
            }

            if (static_cast<bool>(cell->attrs.bold) != fnt.bold()) {
                fnt.setBold(static_cast<bool>(cell->attrs.bold));
            }

            if (static_cast<bool>(cell->attrs.underline) != fnt.underline()) {
                fnt.setUnderline(static_cast<bool>(cell->attrs.underline));
            }

            if (static_cast<bool>(cell->attrs.italic) != fnt.italic()) {
                fnt.setItalic(static_cast<bool>(cell->attrs.italic));
            }

            // No blink.

            if (static_cast<bool>(cell->attrs.strike) != fnt.strikeOut()) {
                fnt.setStrikeOut(static_cast<bool>(cell->attrs.strike));
            }

            p.drawText(
                    x,
                    y,
                    cell->width * m_cellSize.width(),
                    m_cellSize.height(),
                    0,
                    QString::fromUcs4(cell->chars, cell->width));
        }
    }

    if (hasFocus() && m_cursor.visible) {
        int x = m_cursor.col * m_cellSize.width();
        int y = m_cursor.row * m_cellSize.height();
        p.save();
        p.setCompositionMode(QPainter::CompositionMode_SourceOver);
        p.fillRect(
                x,
                y,
                m_cellSize.width(),
                m_cellSize.height(),
                QColor(qRgb(0x40, 0x40, 0x40)));
        const VTermScreenCell *cell = fetchCell(m_cursor.col, m_cursor.row);
        if (cell->chars[0]) {
            p.setPen(toQColor(defaultBg));
            p.drawText(
                    x,
                    y,
                    cell->width * m_cellSize.width(),
                    m_cellSize.height(),
                    0,
                    QString::fromUcs4(cell->chars, cell->width));
        }
        p.restore();
    }

    //qDebug() << "repaint took" << timer.elapsed() << "ms";
}

void QVTerm::resizeEvent(QResizeEvent *event)
{
    event->accept();

    // If increasing in size, we'll trigger libvterm to call sb_popline in
    // order to pull lines out of the history.  This will cause the scrollback
    // to decrease in size which reduces the size of the verticalScrollBar.
    // That will trigger a scroll offset increase which we want to ignore.
    m_ignoreScroll = true;

    m_highlight->reset();
    int height = size().height() / m_cellSize.height();
    int width = size().width() / m_cellSize.width();
    struct winsize wsz = {
            .ws_row = static_cast<short unsigned int>(height),
            .ws_col = static_cast<short unsigned int>(width),
            .ws_xpixel = 0,
            .ws_ypixel = 0,
    };
    ioctl(m_pty, TIOCSWINSZ, &wsz);
    vterm_set_size(m_vterm, height, width);
    vterm_screen_flush_damage(m_vtermScreen);
    m_ignoreScroll = false;
}

void QVTerm::wheelEvent(QWheelEvent *event)
{
    event->accept();

    QPoint delta = event->angleDelta();
    scrollContentsBy(0, delta.y() / 24);
}

void QVTerm::scrollContentsBy(int dx, int dy)
{
    Q_UNUSED(dx);

    if (m_ignoreScroll)
        return;

    if (m_altscreen)
        return;

    size_t orig = m_scrollback->offset();
    size_t offset = m_scrollback->scroll(dy);
    if (orig == offset)
        return;

    m_cursor.visible = (offset == 0);
    viewport()->update();
}

int QVTerm::damage(VTermRect rect)
{
    auto topLeft = QPoint(
            rect.start_col * m_cellSize.width(),
            rect.start_row * m_cellSize.height());
    auto bottomRight = QPoint(
            rect.end_col * m_cellSize.width(),
            rect.end_row * m_cellSize.height());
    viewport()->update(QRect(topLeft, bottomRight));
    return 1;
}

int QVTerm::moverect(VTermRect dest, VTermRect src)
{
    auto topLeft = QPoint(
            std::min(dest.start_col, src.start_col) * m_cellSize.width(),
            std::min(dest.start_row, src.start_row) * m_cellSize.height());
    auto bottomRight = QPoint(
            std::max(dest.end_col, src.end_col) * m_cellSize.width(),
            std::max(dest.end_row, src.end_row) * m_cellSize.height());
    viewport()->update(QRect(topLeft, bottomRight));
    return 1;
}

int QVTerm::movecursor(VTermPos pos, VTermPos oldpos, int visible)
{
    if (pos.row == oldpos.row) {
        viewport()->update(
                std::min(pos.col, oldpos.col) * m_cellSize.width(),
                pos.row * m_cellSize.height(),
                (std::abs(pos.col - oldpos.col) + 1) * m_cellSize.width(),
                m_cellSize.height());
    } else {
        viewport()->update(
                0,
                std::min(pos.row, oldpos.row) * m_cellSize.height(),
                size().width(),
                2 * m_cellSize.height());
    }
    m_cursor.row = pos.row;
    m_cursor.col = pos.col;
    m_cursor.visible = visible;
    return 1;
}

int QVTerm::settermprop(VTermProp prop, VTermValue *val)
{
    switch (prop) {
        case VTERM_PROP_CURSORVISIBLE:
            m_cursor.visible = val->boolean;
            break;
        case VTERM_PROP_CURSORBLINK:
            qDebug() << "Ignoring VTERM_PROP_CURSORBLINK" << val->boolean;
            break;
        case VTERM_PROP_CURSORSHAPE:
            qDebug() << "Ignoring VTERM_PROP_CURSORSHAPE" << val->number;
            break;
        case VTERM_PROP_ICONNAME:
            emit iconTextChanged(val->string);
            break;
        case VTERM_PROP_TITLE:
            emit titleChanged(val->string);
            break;
        case VTERM_PROP_ALTSCREEN:
            m_altscreen = val->boolean;
            m_matches.clear();
            m_highlight->reset();
            break;
        case VTERM_PROP_MOUSE:
            qDebug() << "Ignoring VTERM_PROP_MOUSE" << val->number;
            break;
        case VTERM_PROP_REVERSE:
            qDebug() << "Ignoring VTERM_PROP_REVERSE" << val->boolean;
            break;
        case VTERM_N_PROPS:
            break;
    }
    return 1;
}

int QVTerm::sb_pushline(int cols, const VTermScreenCell *cells)
{
    m_scrollback->emplace(cols, cells, vterm_obtain_state(m_vterm));
    verticalScrollBar()->setRange(0, static_cast<int>(m_scrollback->size()));
    verticalScrollBar()->setValue(verticalScrollBar()->maximum());

    return 1;
}

int QVTerm::sb_popline(int cols, VTermScreenCell *cells)
{
    if (m_scrollback->size() == 0)
        return 0;

    m_scrollback->popto(cols, cells);

    verticalScrollBar()->setRange(0, static_cast<int>(m_scrollback->size()));
    verticalScrollBar()->setValue(verticalScrollBar()->maximum());

    return 1;
}

void QVTerm::copyToClipboard()
{
    if (!m_highlight->active())
        return;

    QString buf = m_highlight->region().dumpString(termSize(), [this](int x, int y) {
        return fetchCell(x, y);
    });
    auto *cb = QApplication::clipboard();
    cb->setText(buf, QClipboard::Selection);
}

void QVTerm::flushToPty()
{
    if (!m_ptyPending.isEmpty()) {
        ssize_t written = 0;
        while (written < m_ptyPending.size()) {
            ssize_t n = write(m_pty, m_ptyPending.data(), m_ptyPending.size());
            if (n < 0) {
                int e = errno;
                if (e == EAGAIN || e == EWOULDBLOCK || e == EINTR) {
                    continue;
                }

                // Give up or bail?
                qCritical("write to pty: %s", strerror(e));
                break;
            }

            written += n;
        }

        m_ptyPending.clear();
    }
}

void QVTerm::onPtyInput(int fd)
{
    auto now = []() -> qint64 {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        return ((qint64)tv.tv_sec) * 1000000UL + tv.tv_usec;
    };

    qint64 deadline = now() + 20 * 1000;
    while (1) {
        // Linux pty buffer is fixed on page size
        char buf[4096];

        ssize_t n = read(fd, buf, sizeof(buf));
        if (n == -1 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR))
            break;

        if (n == 0 || (n == -1 && errno == EIO)) {
            QCoreApplication::exit();
            return;
        }

        if (n < 0) {
            qFatal(strerror(errno));
            exit(1);
        }

        vterm_input_write(m_vterm, buf, n);

        if (now() >= deadline)
            break;
    }
    vterm_screen_flush_damage(m_vtermScreen);
    flushToPty();
}

void QVTerm::pasteFromClipboard()
{
    auto *cb = QApplication::clipboard();

    vterm_keyboard_start_paste(m_vterm);
    for (auto c : cb->text(QClipboard::Selection).toUcs4())
        vterm_keyboard_unichar(m_vterm, c, VTERM_MOD_NONE);
    vterm_keyboard_end_paste(m_vterm);

    flushToPty();
    return;
}

void QVTerm::repaintCursor()
{
    viewport()->update(
            m_cursor.col * m_cellSize.width(),
            m_cursor.row * m_cellSize.height(),
            m_cellSize.width(),
            m_cellSize.height());
}
