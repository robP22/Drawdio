#pragma once
#include <JuceHeader.h>
#include <array>
#include "Core/DrawdioConstants.h"
#include "Core/EditorDesignMetrics.h"
#include "Core/Contracts/IComponentBounds.h"

class PedalboardLayout
{
public:
    void computeSlotBounds(juce::Rectangle<int> componentBounds,
                           std::array<IComponentBounds*, PedalSlotCount> pedals)
    {
        const float sidePad = componentBounds.getWidth() * EditorDesignMetrics::GridSidePaddingRatio;
        const float topPad = componentBounds.getHeight() * EditorDesignMetrics::GridTopPaddingRatio;
        auto bounds = componentBounds.withTrimmedLeft(juce::roundToInt(sidePad))
                                        .withTrimmedRight(juce::roundToInt(sidePad))
                                        .withTrimmedTop(juce::roundToInt(topPad))
                                        .withTrimmedBottom(juce::roundToInt(topPad));

        const float colGap = bounds.getWidth() * EditorDesignMetrics::ColumnGapRatio;
        const float rowGap = bounds.getHeight() * EditorDesignMetrics::RowGapRatio;

        const float pedalWUnclamped = (bounds.getWidth() / EditorDesignMetrics::ColCount - colGap) * EditorDesignMetrics::PedalShrinkRatio;
        const float pedalHUnclamped = (bounds.getHeight() / EditorDesignMetrics::RowCount - rowGap) * EditorDesignMetrics::PedalShrinkRatio;

        const int pedalW = juce::roundToInt(juce::jlimit(
            bounds.getWidth() * EditorDesignMetrics::PedalWidthMinRatio,
            bounds.getWidth() * EditorDesignMetrics::PedalWidthMaxRatio,
            pedalWUnclamped));
        const int pedalH = juce::roundToInt(juce::jlimit(
            bounds.getHeight() * EditorDesignMetrics::PedalHeightMinRatio,
            bounds.getHeight() * EditorDesignMetrics::PedalHeightMaxRatio,
            pedalHUnclamped));

        const int colGapPx = juce::roundToInt(colGap);
        const int groupW  = pedalW * EditorDesignMetrics::ColCount + colGapPx * (EditorDesignMetrics::ColCount - 1);
        const int xOrigin = bounds.getX() + juce::roundToInt((bounds.getWidth() - groupW) * 0.5f);

        const int rowGapPx = juce::roundToInt(rowGap);
        const int groupH  = pedalH * EditorDesignMetrics::RowCount + rowGapPx * (EditorDesignMetrics::RowCount - 1);
        const int yOrigin = bounds.getY() + juce::roundToInt((bounds.getHeight() - groupH) * 0.5f)
                            + juce::roundToInt(bounds.getHeight() * EditorDesignMetrics::VerticalGroupOffsetRatio);

        for (int slot = 0; slot < PedalSlotCount; ++slot)
        {
            const int row = slot / EditorDesignMetrics::ColCount;
            const int col = slot % EditorDesignMetrics::ColCount;

            const int x = xOrigin + col * (pedalW + colGapPx);
            const int y = yOrigin + row * (pedalH + rowGapPx);

            pedals[static_cast<size_t>(slot)]->setBounds({x, y, pedalW, pedalH});
        }
    }
};
