class DrawdioProcessor    : public juce::AudioProcessor
{
      // … rest of code...
     
    void setCurrentColor(PixelCanvasComponent::PixelColor c)   { m_currentColor = c; }
    
      // … rest of code...
};
