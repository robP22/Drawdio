#include <catch2/catch_test_macros.hpp>

#include <array>

#include "Resources/ScaledAssetProvider.h"

namespace
{
class TestResources final : public IResourceProvider
{
public:
    TestResources()
    {
        for (auto& image : m_images)
            image = juce::Image(juce::Image::ARGB, 8, 8, true);

        m_images[static_cast<size_t>(ImageId::PedalLedImage)] = juce::Image(juce::Image::ARGB, 8, 4, true);
        m_images[static_cast<size_t>(ImageId::PedalLedImage)].clear(
            m_images[static_cast<size_t>(ImageId::PedalLedImage)].getBounds(),
            juce::Colours::transparentBlack);
        m_images[static_cast<size_t>(ImageId::PedalLedImage)].setPixelAt(1, 1, juce::Colours::red);
        m_images[static_cast<size_t>(ImageId::PedalLedImage)].setPixelAt(5, 1, juce::Colours::blue);
    }

    const juce::Image& getImage(ImageId id) const override
    {
        ++m_imageCalls;
        return m_images[static_cast<size_t>(id)];
    }

    const juce::Image& getTexture(TextureId id) const override
    {
        return m_images[static_cast<size_t>(id)];
    }

    const SpriteSheet& getSpriteSheet(SpriteSheetId) const override { return m_sheet; }
    const SpriteFrame& getSpriteFrame(SpriteId) const override { return m_frame; }

    mutable int m_imageCalls = 0;

private:
    std::array<juce::Image, static_cast<size_t>(ImageId::Count)> m_images;
    SpriteSheet m_sheet;
    SpriteFrame m_frame;
};
}

TEST_CASE("Scaled asset provider caches separate sprite frames", "[ui][assets]")
{
    TestResources resources;
    ScaledAssetProvider provider(resources);

    const auto off = provider.getScaledFrame(
        IResourceProvider::ImageId::PedalLedImage, { 0, 0, 4, 4 }, 16, 16,
        ScaledAssetProvider::ResamplingPolicy::Continuous);
    const auto on = provider.getScaledFrame(
        IResourceProvider::ImageId::PedalLedImage, { 4, 0, 4, 4 }, 16, 16,
        ScaledAssetProvider::ResamplingPolicy::Continuous);
    const auto offAgain = provider.getScaledFrame(
        IResourceProvider::ImageId::PedalLedImage, { 0, 0, 4, 4 }, 16, 16,
        ScaledAssetProvider::ResamplingPolicy::Continuous);

    REQUIRE(off.isValid());
    REQUIRE(on.isValid());
    REQUIRE(offAgain.isValid());
    REQUIRE(resources.m_imageCalls == 2);
    REQUIRE(offAgain.getPixelAt(8, 8).getRed() > offAgain.getPixelAt(8, 8).getBlue());
}

TEST_CASE("Scaled asset provider reuses cached imagery during resize", "[ui][assets]")
{
    TestResources resources;
    ScaledAssetProvider provider(resources);

    const auto initial = provider.getScaledImage(
        IResourceProvider::ImageId::PedalKnobImage, 20, 20,
        ScaledAssetProvider::ResamplingPolicy::Continuous);
    provider.setResizeActive(true);
    const auto live = provider.getScaledImage(
        IResourceProvider::ImageId::PedalKnobImage, 25, 25,
        ScaledAssetProvider::ResamplingPolicy::Continuous);
    provider.setResizeActive(false);
    const auto settled = provider.getScaledImage(
        IResourceProvider::ImageId::PedalKnobImage, 25, 25,
        ScaledAssetProvider::ResamplingPolicy::Continuous);

    REQUIRE(initial.getWidth() == 20);
    REQUIRE(live.getWidth() == 20);
    REQUIRE(settled.getWidth() == 25);
}

TEST_CASE("Scaled asset provider separates full images from identical frame bounds", "[ui][assets]")
{
    TestResources resources;
    ScaledAssetProvider provider(resources);
    const auto full = provider.getScaledImage(
        IResourceProvider::ImageId::PedalKnobImage, 16, 16,
        ScaledAssetProvider::ResamplingPolicy::Continuous);
    provider.setResizeActive(true);
    const auto frame = provider.getScaledFrame(
        IResourceProvider::ImageId::PedalKnobImage, { 0, 0, 8, 8 }, 20, 20,
        ScaledAssetProvider::ResamplingPolicy::Continuous);

    REQUIRE(full.isValid());
    REQUIRE(frame.isValid());
    REQUIRE(frame.getWidth() == 20);
}
