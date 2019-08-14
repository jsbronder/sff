#pragma once

#include "region.hpp"

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
     * Reset the highlight
     **/
    void reset();

    /**
     * Region being highlighted
     **/
    Region &region() { return m_region; };

    /**
     * Update the highlighted region
     *
     * @param x - x coordinate in VTerm space
     * @param y - y coordinate in VTerm space
     **/
    void update(int x, int y);

private:
    QPoint m_anchor{};
    Region m_region{};
};
