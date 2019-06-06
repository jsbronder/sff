#include "highlight.hpp"

#include <QDebug>

bool Highlight::active() const
{
    return !m_anchor.isNull() && !m_region.isNull();
}

void Highlight::anchor(int x, int y)
{
    if (active())
        reset();
    m_anchor = {x, y};
}

bool Highlight::contains(int x, int y) const
{
    return active() && m_region.contains(x, y);
}

void Highlight::reset()
{
    m_anchor = QPoint();
    m_region = Region();
}

void Highlight::update(int x, int y)
{
    if (m_anchor.x() == x && m_anchor.y() == y) {
        m_region = Region();
        return;
    }

    static auto lt = [](int x1, int y1, int x2, int y2) {
        return y1 == y2 ? x1 <= x2 : y1 <= y2;
    };

    if (lt(x, y, m_anchor.x(), m_anchor.y()))
        m_region.update({x, y}, m_anchor);
    else
        m_region.update(m_anchor, {x, y});
}
