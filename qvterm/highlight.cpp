#include "highlight.hpp"

#include <QDebug>

bool Highlight::active() const
{
    return !m_anchor.isNull() && !m_start.isNull();
}

void Highlight::anchor(int x, int y)
{
    if (active())
        m_anchor = QPoint();
    else
        m_anchor = {x, y};

    m_start = QPoint();
    m_end = QPoint();
}

bool Highlight::contains(int x, int y) const
{
    return (active()
        && (y > m_start.y() || (y == m_start.y() && x >= m_start.x()))
        && (y < m_end.y() || (y == m_end.y() && x <= m_end.x())));
}

void Highlight::reset()
{
    m_anchor = QPoint();
    m_start = QPoint();
    m_end = QPoint();
}

void Highlight::update(int x, int y)
{
    if (m_anchor.x() == x && m_anchor.y() == y) {
        m_start = QPoint();
        m_end = QPoint();
        return;
    }

    static auto lt = [](int x1, int y1, int x2, int y2) {
        return y1 == y2 ? x1 <= x2 : y1 <= y2;
    };

    if (lt(x, y, m_anchor.x(), m_anchor.y())) {
        m_start = {x, y};
        m_end = m_anchor;
    } else {
        m_start = m_anchor;
        m_end = {x, y};
    }
}
