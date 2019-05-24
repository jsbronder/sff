#pragma once

#include <QPoint>
#include <QRect>

class Highlight {
public:
    Highlight() = default;

    /**
     * True if highlight is currently active
     **/
    bool active() const;

    /**
     * Start highlighting at the given coordinates
     *
     * @param x - x coordinate in VTerm space
     * @param y - y coordinate in VTerm space
     **/
    void anchor(int x, int y);

    /**
     * Test if a point in VTerm space is in the highlighted region
     *
     * @param x - x coordinate in VTerm space
     * @param y - y coordinate in VTerm space
     **/
    bool contains(int x, int y) const;

    /**
     * End of the highlighted region in VTerm space
     **/
    const QPoint &end() const { return m_end; };

    /**
     * Reset the highlight
     **/
    void reset();

    /**
     * Start of the highlighted region in VTerm space
     **/
    const QPoint &start() const { return m_start; };

    /**
     * Update the highlighted region
     *
     * @param x - x coordinate in VTerm space
     * @param y - y coordinate in VTerm space
     **/
    void update(int x, int y);

private:
    QPoint m_anchor{};
    QPoint m_start{};
    QPoint m_end{};
};
