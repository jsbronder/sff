#include "scrollback.hpp"

#include <cassert>
#include <cstring>

ScrollbackLine::ScrollbackLine(int cols, const VTermScreenCell *cells, VTermState *vts) :
    m_cols(cols),
    m_cells(std::make_unique<VTermScreenCell[]>(cols))
{
    memcpy(m_cells.get(), cells, cols * sizeof(cells[0]));
    for (int i = 0; i < cols; ++i) {
        vterm_state_convert_color_to_rgb(vts, &m_cells[i].fg);
        vterm_state_convert_color_to_rgb(vts, &m_cells[i].bg);
    }
}

const VTermScreenCell *ScrollbackLine::cell(int i) const
{
    assert(i > 0 && i < m_cols);
    return &m_cells[i];
}

Scrollback::Scrollback(size_t capacity) :
    m_capacity(capacity)
{
}

void Scrollback::emplace(int cols, const VTermScreenCell *cells, VTermState *vts)
{
    m_deque.emplace_front(cols, cells, vts);
    while (m_deque.size() > m_capacity)
        m_deque.pop_back();
}

void Scrollback::popto(int cols, VTermScreenCell *cells)
{
    const auto &sbl = m_deque.front();

    int ncells = cols;
    if (ncells > sbl.cols())
        ncells = sbl.cols();

    memcpy(cells, sbl.cells(), sizeof(cells[0]) * ncells);
    for (size_t i = ncells; i < static_cast<size_t>(cols); ++i) {
        cells[i].chars[0] = '\0';
        cells[i].width = 1;
        cells[i].bg = cells[ncells - 1].bg;
    }

    m_deque.pop_front();
}

size_t Scrollback::scroll(int delta)
{
    m_offset = std::min(
            std::max(0, static_cast<int>(m_offset) + delta),
            static_cast<int>(m_deque.size()));
    return m_offset;
}
