#pragma once
#include <vector>

struct AutomationPoint
{
    float time = 0.0f;
    float value = 0.5f;
};

class AutomationEnvelope
{
public:
    void clear() { m_points.clear(); }
    void addPoint(float time, float value);
    float sample(float t) const;
    bool empty() const { return m_points.empty(); }

private:
    std::vector<AutomationPoint> m_points;
};
