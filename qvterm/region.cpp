#include "region.hpp"

Region::Region(QPoint start, QPoint end) :
    m_start(std::move(start)),
    m_end(std::move(end))
{
}

Region Region::fromPixelSpace(const QPoint &start, const QPoint &end, const QSize &cellSize)
{
    return {
            {start.x() % cellSize.width(), start.y() / cellSize.height()},
            {end.x() % cellSize.width(), end.y() / cellSize.height()}};
}

QString Region::dump(const QSize &termSize, const std::function<const VTermScreenCell *(int, int)> &fetchCell) const
{
    QString r{};
    const VTermScreenCell *cell = nullptr;

    for (int y = m_start.y(); y < m_end.y() + 1; ++y) {
        int xStart = y == m_start.y() ? m_start.x() : 0;
        int xEnd = y == m_end.y() ? m_end.x() + 1 : termSize.width();

        for (int x = xStart; x < xEnd; ++x) {
            cell = fetchCell(x, y);
            if (cell->chars[0])
                r.append(QString::fromUcs4(cell->chars, cell->width));
            else
                r.append('\0');
        }
    }
    return r;
}

QString Region::dumpString(const QSize &termSize, const std::function<const VTermScreenCell *(int, int)> &fetchCell) const
{
    QString r{};
    const VTermScreenCell *cell = nullptr;

    for (int y = m_start.y(); y < m_end.y() + 1; ++y) {
        int xStart = y == m_start.y() ? m_start.x() : 0;
        int xEnd = y == m_end.y() ? m_end.x() + 1 : termSize.width();
        int skipped = 0;

        for (int x = xStart; x < xEnd; ++x) {
            cell = fetchCell(x, y);
            if (cell->chars[0]) {
                for (; skipped; skipped--)
                    r.append(' ');
                r.append(QString::fromUcs4(cell->chars, cell->width));
            } else {
                skipped++;
            }
        }

        if (!cell->chars[0])
            r.append('\n');
    }
    return r;
}

QRect Region::pixelRect(const QSize &termSize, const QSize &cellSize) const
{
    bool oneLine = m_start.y() == m_end.y();
    QPoint topLeft{
            oneLine ? m_start.x() * cellSize.width() : 0,
            m_start.y() * cellSize.height()};

    // Essentially rounding up to make sure we create a bounding box that
    // fully captures the text.
    QPoint bottomRight{
            (oneLine ? m_end.x() + 1 : termSize.width()) * cellSize.width(),
            (1 + m_end.y()) * cellSize.height()};

    return {topLeft, bottomRight};
}
