#pragma once

#include <deque>
#include <memory>

extern "C" {
#include <vterm.h>
}

class ScrollbackLine {
public:
    ScrollbackLine(int cols, const VTermScreenCell *cells, VTermState *vts);
    ScrollbackLine(ScrollbackLine &&other) = default;
    ScrollbackLine() = delete;

    int cols() const { return m_cols; };
    const VTermScreenCell *cell(int i) const;
    const VTermScreenCell *cells() const { return &m_cells[0]; };

private:
    int m_cols;
    std::unique_ptr<VTermScreenCell[]> m_cells;
};

class Scrollback {
public:
    Scrollback(size_t capacity);
    Scrollback() = delete;

    size_t capacity() const { return m_capacity; };
    size_t size() const { return m_deque.size(); };
    size_t offset() const { return m_offset; };

    const ScrollbackLine &line(size_t index) const { return m_deque.at(index); };

    void emplace(int cols, const VTermScreenCell *cells, VTermState *vts);
    void popto(int cols, VTermScreenCell *cells);
    size_t scroll(int delta);
    void unscroll() { m_offset = 0; };

private:
    size_t m_capacity;
    size_t m_offset{0};
    std::deque<ScrollbackLine> m_deque;
};
