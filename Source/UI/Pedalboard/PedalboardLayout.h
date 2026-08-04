#pragma once
#include <JuceHeader.h>
#include <array>
#include "Core/DrawdioConstants.h"
#include "GridLayout.h"
#include "Core/Contracts/IComponentBounds.h"

class PedalboardLayout
{
public:
    void computeSlotBounds(juce::Rectangle<int> componentBounds,
                           std::array<IComponentBounds*, PedalSlotCount> pedals)
    {
        const float sidePad = componentBounds.getWidth() * GridLayout::GridSidePaddingRatio;
        const float topPad = componentBounds.getHeight() * GridLayout::GridTopPaddingRatio;
        auto bounds = componentBounds.withTrimmedLeft(juce::roundToInt(sidePad))
                                        .withTrimmedRight(juce::roundToInt(sidePad))
                                        .withTrimmedTop(juce::roundToInt(topPad))
                                        .withTrimmedBottom(juce::roundToInt(topPad));

        const float colGap = bounds.getWidth() * GridLayout::ColumnGapRatio;
        const float rowGap = bounds.getHeight() * GridLayout::RowGapRatio;

        const float pedalWUnclamped = (bounds.getWidth() / GridLayout::ColCount - colGap) * GridLayout::PedalShrinkRatio;
        const float pedalHUnclamped = (bounds.getHeight() / GridLayout::RowCount - rowGap) * GridLayout::PedalShrinkRatio;

        const int pedalW = juce::roundToInt(juce::jlimit(
            bounds.getWidth() * GridLayout::PedalWidthMinRatio,
            bounds.getWidth() * GridLayout::PedalWidthMaxRatio,
            pedalWUnclamped));
        const int pedalH = juce::roundToInt(juce::jlimit(
            bounds.getHeight() * GridLayout::PedalHeightMinRatio,
            bounds.getHeight() * GridLayout::PedalHeightMaxRatio,
            pedalHUnclamped));

        const int groupW  = pedalW * GridLayout::ColCount + juce::roundToInt(colGap * (GridLayout::ColCount - 1));
        const int xOrigin = bounds.getX() + (bounds.getWidth() - groupW) / 2;

        const int groupH  = pedalH * GridLayout::RowCount + juce::roundToInt(rowGap * (GridLayout::RowCount - 1));
        const int yOrigin = bounds.getY() + (bounds.getHeight() - groupH) / 2
                            + juce::roundToInt(bounds.getHeight() * GridLayout::VerticalGroupOffsetRatio);

        for (int slot = 0; slot < PedalSlotCount; ++slot)
        {
            const int row = slot / GridLayout::ColCount;
            const int col = slot % GridLayout::ColCount;

            const int x = xOrigin + col * (pedalW + juce::roundToInt(colGap));
            const int y = yOrigin + row * (pedalH + juce::roundToInt(rowGap));

            pedals[static_cast<size_t>(slot)]->setBounds({x, y, pedalW, pedalH});
        }
    }
};
