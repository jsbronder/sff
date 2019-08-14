#pragma once
#include <QPoint>
#include <QRect>
#include <QSize>
#include <QString>

#include <functional>

class QDebug;

extern "C" {
#include <vterm.h>
}

/**
 * Define a left-to-right, top-to-bottom selection of characters in VTerm space.
 **/
class Region {
public:
    Region() = default;

    /**
     * Create a new region
     *
     * @param start - Start position in VTerm space
     * @param end   - End position in VTerm space
     **/
    Region(QPoint start, QPoint end);

    /**
     * Create a new region
     *
     * @param start     - Start position in pixel space
     * @param end       - End position in pixel space
     * @param cellSize  - Size of a VTerm cell
     *
     * @return  - New region
     **/
    static Region fromPixelSpace(const QPoint &start, const QPoint &end, const QSize &cellSize);

    /**
     * Test if a point is inside of the region
     *
     * @param x - x coordinate
     * @param y - y coordinate
     *
     * @return  - true if the point is in the region, false otherwise
     **/
    bool contains(int x, int y) const
    {
        return (!isNull()
                && (y > m_start.y() || (y == m_start.y() && x >= m_start.x()))
                && (y < m_end.y() || (y == m_end.y() && x <= m_end.x())));
    }

    /**
     * Test if a region is inside of this one
     *
     * @param other - region to check
     *
     * @return - true if this region contains the other
     **/
    bool contains(const Region &other) const
    {
        return (
                (m_start.y() < other.m_start.y()
                        || (m_start.y() == other.m_start.y() && m_start.x() <= other.m_start.x()))
                && (m_end.y() > other.m_end.y()
                           || (m_end.y() == other.m_end.y() && m_end.x() >= other.m_end.x())));
    }

    /**
     * Test if a region contains a QPoint
     *
     * @param point - point to check for inclusion
     *
     * @return  - true if point is inside the region, false otherwise
     **/
    bool contains(const QPoint &point) const
    {
        return contains(point.x(), point.y());
    }

    /**
     * Dump the contents of the region exactly as it is stored
     *
     * @param termSize  - size of terminal
     * @param fetchCell - function that will return VTermScreenCells for a given
     *                    position
     **/
    QString dump(const QSize &termSize, const std::function<const VTermScreenCell *(int, int)> &fetchCell) const;

    /**
     * Dump the contents of the region.  Empty cells will be ignored unless they
     * are followed on the same line by non-empty cells.  That is, this performs
     * the expected behavior for a copy operation on a selected region.
     *
     * @param termSize  - size of terminal
     * @param fetchCell - function that will return VTermScreenCells for a given
     *                    position
     **/
    QString dumpString(const QSize &termSize, const std::function<const VTermScreenCell *(int, int)> &fetchCell) const;

    /**
     * Test if the region is Null
     **/
    bool isNull() const
    {
        return m_start.isNull() && m_end.isNull();
    }

    /**
     * Check if this region overlaps another.
     *
     * @param other - Region to compare against
     *
     * @return  - true if the regions overlap, false otherwise
     **/
    bool overlaps(const Region &other) const;

    /**
     * Translate region into a rectangle in pixel space
     *
     * @param termSize  - size of terminal
     * @param cellSize  - size of a VTerm cell
     **/
    QRect pixelRect(const QSize &termSize, const QSize &cellSize) const;

    /**
     * Shift the start and end points by the given amount
     *
     * @param delta - QPoint with amount to shift on each axis
     **/
    void shift(const QPoint &delta);

    /**
     * Update the region
     *
     * @param start - New start point
     * @param end   - New end point
     **/
    void update(QPoint start, QPoint end)
    {
        m_start = std::move(start);
        m_end = std::move(end);
    }

private:
    QPoint m_start;
    QPoint m_end;
};

QDebug operator<<(QDebug dbg, const Region &r);
