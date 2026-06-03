#include "ResourceManager.h"

#include <utility>

namespace
{
void fillVerticalGradient(juce::Graphics& g,
                          juce::Rectangle<float> bounds,
                          juce::Colour top,
                          juce::Colour bottom,
                          float radius)
{
    juce::ColourGradient gradient(top, bounds.getX(), bounds.getY(),
                                  bottom, bounds.getX(), bounds.getBottom(), false);
    g.setGradientFill(gradient);
    g.fillRoundedRectangle(bounds, radius);
}
}

ResourceManager::ResourceManager()
{
    loadAll();
}

const juce::Image& ResourceManager::getImage(ImageId id) const
{
    return m_images[static_cast<size_t>(id)];
}

const juce::Image& ResourceManager::getTexture(TextureId id) const
{
    return m_images[static_cast<size_t>(id)];
}

const ResourceManager::SpriteSheet& ResourceManager::getSpriteSheet(SpriteSheetId id) const
{
    return m_spriteSheets[static_cast<size_t>(id)];
}

const ResourceManager::SpriteFrame& ResourceManager::getSpriteFrame(SpriteId id) const
{
    return m_spriteFrames[static_cast<size_t>(id)];
}

juce::Rectangle<int> ResourceManager::getSpriteSourceRect(SpriteId id) const
{
    return getSpriteFrame(id).source;
}

juce::Typeface::Ptr ResourceManager::getTypeface(FontId id) const
{
    return m_typefaces[static_cast<size_t>(id)];
}

std::unique_ptr<juce::Drawable> ResourceManager::createSvgDrawable(SvgId id) const
{
    const auto& xml = m_svgXml[static_cast<size_t>(id)];
    if (xml.isEmpty())
        return {};

    auto document = juce::XmlDocument::parse(xml);
    if (document == nullptr)
        return {};

    return juce::Drawable::createFromSVG(*document);
}

void ResourceManager::loadAll()
{
    loadProceduralTextures();
    loadSpriteSheets();
}

void ResourceManager::loadProceduralTextures()
{
    m_images[static_cast<size_t>(ImageId::WorkspaceWood)] = makeWorkspaceWoodTexture();
    m_images[static_cast<size_t>(ImageId::PedalboardFelt)] = makePedalboardFeltTexture();
    m_images[static_cast<size_t>(ImageId::PalettePaint)] = makePalettePaintTexture();
    m_images[static_cast<size_t>(ImageId::PedalFaceGrain)] = makePedalFaceGrainTexture();
    m_images[static_cast<size_t>(ImageId::OverlayGloss)] = makeOverlayGlossTexture();
}

void ResourceManager::loadSpriteSheets()
{
    constexpr int pedalFrameW = 72;
    constexpr int pedalFrameH = 72;
    m_spriteSheets[static_cast<size_t>(SpriteSheetId::PedalParts)] =
        { makePedalPartsSpriteSheet(pedalFrameW, pedalFrameH), pedalFrameW, pedalFrameH };
    m_spriteFrames[static_cast<size_t>(SpriteId::PedalBody)] =
        { SpriteSheetId::PedalParts, spriteFrameRect(0, pedalFrameW, pedalFrameH) };
    m_spriteFrames[static_cast<size_t>(SpriteId::PedalFace)] =
        { SpriteSheetId::PedalParts, spriteFrameRect(1, pedalFrameW, pedalFrameH) };
    m_spriteFrames[static_cast<size_t>(SpriteId::PedalLcd)] =
        { SpriteSheetId::PedalParts, spriteFrameRect(2, pedalFrameW, pedalFrameH) };
    m_spriteFrames[static_cast<size_t>(SpriteId::PedalLed)] =
        { SpriteSheetId::PedalParts, spriteFrameRect(3, pedalFrameW, pedalFrameH) };
    m_spriteFrames[static_cast<size_t>(SpriteId::PedalJack)] =
        { SpriteSheetId::PedalParts, spriteFrameRect(4, pedalFrameW, pedalFrameH) };

    constexpr int placeholderFrame = 32;
    m_spriteSheets[static_cast<size_t>(SpriteSheetId::Buttons)] =
        { makePlaceholderSpriteSheet(juce::Colour(0xFF3A454B)), placeholderFrame, placeholderFrame };
    m_spriteSheets[static_cast<size_t>(SpriteSheetId::Meters)] =
        { makePlaceholderSpriteSheet(juce::Colour(0xFF36D987)), placeholderFrame, placeholderFrame };
    m_spriteSheets[static_cast<size_t>(SpriteSheetId::Overlays)] =
        { makePlaceholderSpriteSheet(juce::Colours::white.withAlpha(0.32f)), placeholderFrame, placeholderFrame };

    m_spriteFrames[static_cast<size_t>(SpriteId::ButtonDefault)] =
        { SpriteSheetId::Buttons, spriteFrameRect(0, placeholderFrame, placeholderFrame) };
    m_spriteFrames[static_cast<size_t>(SpriteId::MeterSegment)] =
        { SpriteSheetId::Meters, spriteFrameRect(0, placeholderFrame, placeholderFrame) };
    m_spriteFrames[static_cast<size_t>(SpriteId::OverlayGloss)] =
        { SpriteSheetId::Overlays, spriteFrameRect(0, placeholderFrame, placeholderFrame) };
}

bool ResourceManager::loadImageFromBinaryData(ImageId id, const void* data, size_t sizeInBytes)
{
    auto image = decodeImageFromBinaryData(data, sizeInBytes);
    if (!image.isValid())
        return false;

    m_images[static_cast<size_t>(id)] = image;
    return true;
}

bool ResourceManager::loadTextureFromBinaryData(TextureId id, const void* data, size_t sizeInBytes)
{
    return loadImageFromBinaryData(static_cast<ImageId>(static_cast<size_t>(id)), data, sizeInBytes);
}

juce::Image ResourceManager::decodeImageFromBinaryData(const void* data, size_t sizeInBytes)
{
    if (data == nullptr || sizeInBytes == 0)
        return {};

    return juce::ImageFileFormat::loadFrom(data, sizeInBytes);
}

juce::Image ResourceManager::makeWorkspaceWoodTexture()
{
    constexpr int width = 1024;
    constexpr int height = 1024;
    juce::Image image(juce::Image::RGB, width, height, false);
    juce::Graphics g(image);

    juce::ColourGradient base(juce::Colour(0xFF4A2E1C), 0.0f, 0.0f,
                              juce::Colour(0xFF17100B), 0.0f,
                              static_cast<float>(height), false);
    base.addColour(0.36, juce::Colour(0xFF302014));
    base.addColour(0.68, juce::Colour(0xFF24170F));
    g.setGradientFill(base);
    g.fillAll();

    juce::Random random(0x44726177);
    for (int i = 0; i < 1200; ++i)
    {
        const float y = random.nextFloat() * static_cast<float>(height);
        const float x = -40.0f + random.nextFloat() * 80.0f;
        const float length = static_cast<float>(width) + 120.0f;
        const float wobble = 10.0f + random.nextFloat() * 24.0f;

        juce::Path grain;
        grain.startNewSubPath(x, y);
        grain.cubicTo(length * 0.3f, y - wobble,
                      length * 0.62f, y + wobble,
                      length, y + random.nextFloat() * 18.0f - 9.0f);

        const auto alpha = 0.025f + random.nextFloat() * 0.05f;
        g.setColour((i % 5 == 0 ? juce::Colour(0xFF8B5A35)
                                 : juce::Colour(0xFF0E0906)).withAlpha(alpha));
        g.strokePath(grain, juce::PathStrokeType(0.7f + random.nextFloat() * 1.7f));
    }

    for (int i = 0; i < 48; ++i)
    {
        const float cx = random.nextFloat() * static_cast<float>(width);
        const float cy = random.nextFloat() * static_cast<float>(height);
        const float rw = 80.0f + random.nextFloat() * 180.0f;
        const float rh = 10.0f + random.nextFloat() * 26.0f;
        g.setColour(juce::Colour(0xFF6C4529).withAlpha(0.045f));
        g.fillEllipse(cx - rw * 0.5f, cy - rh * 0.5f, rw, rh);
    }

    return image;
}

juce::Image ResourceManager::makePedalboardFeltTexture()
{
    constexpr int width = 768;
    constexpr int height = 512;
    juce::Image image(juce::Image::RGB, width, height, false);
    juce::Graphics g(image);

    juce::ColourGradient feltGradient(juce::Colour(0xFF24292A), 0.0f, 0.0f,
                                      juce::Colour(0xFF070909), 0.0f,
                                      static_cast<float>(height), false);
    feltGradient.addColour(0.45, juce::Colour(0xFF151A1A));
    g.setGradientFill(feltGradient);
    g.fillAll();

    juce::Random random(0xF317);
    for (int i = 0; i < 5200; ++i)
    {
        const int x = random.nextInt(width);
        const int y = random.nextInt(height);
        g.setColour((i % 3 == 0 ? juce::Colours::white : juce::Colours::black)
                        .withAlpha(0.018f + random.nextFloat() * 0.028f));
        g.fillRect(x, y, 1, 1);
    }

    for (int y = 0; y < height; y += 4)
    {
        g.setColour(juce::Colours::white.withAlpha(0.012f));
        g.drawHorizontalLine(y, 0.0f, static_cast<float>(width));
    }

    return image;
}

juce::Image ResourceManager::makePalettePaintTexture()
{
    constexpr int width = 256;
    constexpr int height = 96;
    juce::Image image(juce::Image::ARGB, width, height, true);
    juce::Graphics g(image);
    juce::Random random(0xA11CE);

    for (int i = 0; i < 120; ++i)
    {
        const float size = 8.0f + random.nextFloat() * 34.0f;
        const float x = random.nextFloat() * static_cast<float>(width);
        const float y = random.nextFloat() * static_cast<float>(height);
        const uint32_t colour = (i % 4 == 0) ? 0xFFFFFFFF
                              : (i % 4 == 1) ? 0xFF47C9A2
                              : (i % 4 == 2) ? 0xFFE54235
                                             : 0xFF2F73D8;
        g.setColour(juce::Colour(colour).withAlpha(0.018f + random.nextFloat() * 0.045f));
        g.fillEllipse(x - size * 0.5f, y - size * 0.5f, size, size * (0.55f + random.nextFloat() * 0.5f));
    }

    return image;
}

juce::Image ResourceManager::makePedalFaceGrainTexture()
{
    constexpr int size = 256;
    juce::Image image(juce::Image::ARGB, size, size, true);
    juce::Graphics g(image);
    juce::Random random(0xD0A0);

    for (int i = 0; i < 520; ++i)
    {
        const auto x = random.nextFloat() * static_cast<float>(size);
        const auto y = random.nextFloat() * static_cast<float>(size);
        g.setColour(juce::Colours::white.withAlpha(random.nextFloat() * 0.038f));
        g.fillRect(x, y, 1.0f + random.nextFloat() * 2.0f, 0.7f);
    }

    return image;
}

juce::Image ResourceManager::makeOverlayGlossTexture()
{
    constexpr int size = 128;
    juce::Image image(juce::Image::ARGB, size, size, true);
    juce::Graphics g(image);

    juce::ColourGradient gloss(juce::Colours::white.withAlpha(0.30f), 0.0f, 0.0f,
                               juce::Colours::transparentWhite,
                               static_cast<float>(size) * 0.8f,
                               static_cast<float>(size) * 0.9f,
                               true);
    g.setGradientFill(gloss);
    g.fillAll();

    return image;
}

juce::Image ResourceManager::makePedalPartsSpriteSheet(int frameWidth, int frameHeight)
{
    juce::Image image(juce::Image::ARGB, frameWidth * 5, frameHeight, true);
    juce::Graphics g(image);

    for (int i = 0; i < 5; ++i)
    {
        auto frame = spriteFrameRect(i, frameWidth, frameHeight).toFloat().reduced(6.0f);

        if (i == 0)
        {
            fillVerticalGradient(g, frame, juce::Colours::white.withAlpha(0.24f),
                                 juce::Colours::black.withAlpha(0.20f), 12.0f);
        }
        else if (i == 1)
        {
            g.setColour(juce::Colours::white.withAlpha(0.16f));
            g.drawRoundedRectangle(frame.reduced(1.0f), 9.0f, 1.2f);
        }
        else if (i == 2)
        {
            fillVerticalGradient(g, frame.reduced(9.0f, 20.0f),
                                 juce::Colours::white.withAlpha(0.18f),
                                 juce::Colours::transparentWhite,
                                 5.0f);
        }
        else if (i == 3)
        {
            g.setColour(juce::Colours::white.withAlpha(0.36f));
            g.fillEllipse(frame.withSizeKeepingCentre(18.0f, 18.0f).translated(-3.0f, -3.0f));
        }
        else
        {
            auto jack = frame.withSizeKeepingCentre(30.0f, 30.0f);
            g.setColour(juce::Colours::white.withAlpha(0.24f));
            g.fillEllipse(jack.reduced(2.0f));
            g.setColour(juce::Colours::black.withAlpha(0.50f));
            g.fillEllipse(jack.reduced(10.0f));
        }
    }

    return image;
}

juce::Image ResourceManager::makePlaceholderSpriteSheet(juce::Colour colour)
{
    constexpr int size = 32;
    juce::Image image(juce::Image::ARGB, size, size, true);
    juce::Graphics g(image);
    g.setColour(colour);
    g.fillRoundedRectangle(4.0f, 4.0f, 24.0f, 24.0f, 5.0f);
    g.setColour(juce::Colours::white.withAlpha(0.18f));
    g.drawRoundedRectangle(4.5f, 4.5f, 23.0f, 23.0f, 4.0f, 1.0f);
    return image;
}

juce::Rectangle<int> ResourceManager::spriteFrameRect(int frameIndex, int frameWidth, int frameHeight)
{
    return { frameIndex * frameWidth, 0, frameWidth, frameHeight };
}
