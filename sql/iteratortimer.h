#include<chrono>

class IteratorTimer {
  public:
    IteratorTimer(std::chrono::steady_clock::time_point::duration *cost_time,
                   bool enabled)
        : m_cost_time(cost_time), m_enabled(enabled) {
        if (m_enabled) m_begin = std::chrono::steady_clock::now();
    }
    ~IteratorTimer() {
        if (m_enabled) *m_cost_time += std::chrono::steady_clock::now() - m_begin;
    }

    std::chrono::steady_clock::time_point::duration *m_cost_time;
    bool m_enabled{false};

    std::chrono::steady_clock::time_point m_begin;
};