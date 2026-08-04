#include "State/AutomationEnvelope.h"
#include <algorithm>

void AutomationEnvelope::addPoint(float time, float value)
{
    value = std::max(0.0f, std::min(1.0f, value));
    time = std::max(0.0f, std::min(1.0f, time));

    auto it = std::upper_bound(m_points.begin(), m_points.end(), time,
        [](float t, const AutomationPoint& p) { return t < p.time; });
    m_points.insert(it, {time, value});
}

float AutomationEnvelope::sample(float t) const
{
    if (m_points.empty()) return 0.5f;
    if (m_points.size() == 1) return m_points[0].value;

    int n = static_cast<int>(m_points.size());

    for (int i = 0; i < n; ++i)
    {
        int j = (i + 1) % n;
        float t1 = m_points[i].time;
        float t2 = m_points[j].time;
        if (j == 0) t2 += 1.0f;

        if (t >= t1 - 1e-6f && t <= t2 + 1e-6f)
        {
            float frac = (t2 > t1) ? (t - t1) / (t2 - t1) : 0.0f;
            float v1 = m_points[i].value;
            float v2 = m_points[j].value;
            return v1 + (v2 - v1) * frac;
        }
    }

    return m_points[0].value;
}
