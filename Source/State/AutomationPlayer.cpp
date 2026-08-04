#include "AutomationPlayer.h"
#include <cmath>

void AutomationPlayer::tick(double ppqPosition, float bpm, bool isPlaying)
{
    if (!isPlaying)
    {
        m_playheadTime = 0.0f;
        float sectionFrac = static_cast<float>(m_sectionStartBar) / 8.0f;
        float widthFrac = static_cast<float>(m_barCount) / 8.0f;
        float t = std::fmod(sectionFrac + widthFrac * 0.0f, 1.0f);
        m_currentValue.store(m_envelope.sample(t), std::memory_order_relaxed);
        return;
    }

    double totalBeats = 4.0 * static_cast<double>(m_barCount);
    m_playheadTime = static_cast<float>(std::fmod(ppqPosition / totalBeats, 1.0));
    if (m_playheadTime < 0.0f) m_playheadTime += 1.0f;

    float sectionFrac = static_cast<float>(m_sectionStartBar) / 8.0f;
    float widthFrac = static_cast<float>(m_barCount) / 8.0f;
    float t = std::fmod(sectionFrac + m_playheadTime * widthFrac, 1.0f);
    m_currentValue.store(m_envelope.sample(t), std::memory_order_relaxed);
}
