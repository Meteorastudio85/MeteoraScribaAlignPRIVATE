#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

// ============================================================================
//  Stile Scriba: blu notte, pannelli arrotondati, accento turchese
// ============================================================================
namespace Scriba
{
    namespace Col
    {
        const juce::Colour bgTop        { 0xff121926 };
        const juce::Colour bgMid        { 0xff1f2c40 };
        const juce::Colour bgBottom     { 0xff3f5b7e };
        const juce::Colour panel        { 0xbf141c29 };
        const juce::Colour panelStroke  { 0xff2c3a4f };
        const juce::Colour panelLow     { 0x8c2b3e58 };
        const juce::Colour panelLowLine { 0xff3a4f6b };
        const juce::Colour tile         { 0xff172130 };
        const juce::Colour tileStroke   { 0xff26344a };
        const juce::Colour tileOn       { 0xff14303a };
        const juce::Colour pill         { 0xff111823 };
        const juce::Colour track        { 0xff2a384d };
        const juce::Colour trackLine    { 0xff3a4a61 };
        const juce::Colour text         { 0xffeef2f7 };
        const juce::Colour textSoft     { 0xffc9d3df };
        const juce::Colour dim          { 0xff8696ab };
        const juce::Colour dimmer       { 0xff5f6f86 };
        const juce::Colour teal         { 0xff34e0c4 };
        const juce::Colour tealLight    { 0xffd9fbf3 };
        const juce::Colour tealInk      { 0xff0b4a52 };
        const juce::Colour tealDark     { 0xff1b5a5c };
        const juce::Colour other        { 0xff8fd5e6 };
        const juce::Colour amber        { 0xfff2c27a };
    }

    // Font in grassetto con spaziatura larga, come i titoli di Scriba
    inline juce::Font font (float size, bool bold = true, float tracking = 0.0f)
    {
        return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain)
                               .withKerningFactor (tracking));
    }

    inline float textWidth (const juce::Font& f, const juce::String& s)
    {
        return juce::GlyphArrangement::getStringWidth (f, s);
    }
}

class ScribaLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ScribaLookAndFeel()
    {
        using namespace Scriba;
        setColour (juce::Label::textColourId, Col::text);
        setColour (juce::Label::textWhenEditingColourId, Col::text);
        setColour (juce::Label::backgroundWhenEditingColourId, Col::pill);
        setColour (juce::Label::outlineWhenEditingColourId, Col::teal);
        setColour (juce::TextEditor::highlightColourId, Col::teal.withAlpha (0.35f));
        setColour (juce::TextEditor::focusedOutlineColourId, Col::teal);
        setColour (juce::CaretComponent::caretColourId, Col::teal);
        setColour (juce::TooltipWindow::backgroundColourId, Col::pill);
        setColour (juce::TooltipWindow::textColourId, Col::text);
        setColour (juce::TooltipWindow::outlineColourId, Col::panelStroke);
        setColour (juce::ComboBox::backgroundColourId, Col::tile);
        setColour (juce::ComboBox::outlineColourId, Col::panelLowLine);
        setColour (juce::ComboBox::textColourId, Col::text);
        setColour (juce::ComboBox::arrowColourId, Col::teal);
        setColour (juce::ComboBox::focusedOutlineColourId, Col::teal);
        setColour (juce::PopupMenu::backgroundColourId, Col::pill);
        setColour (juce::PopupMenu::textColourId, Col::text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, Col::tealDark);
        setColour (juce::PopupMenu::highlightedTextColourId, Col::text);
    }

    // Slider bipolare: traccia sottile, riempimento turchese dallo zero, pomello bianco
    void drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h,
                           float sliderPos, float, float,
                           juce::Slider::SliderStyle, juce::Slider& s) override
    {
        using namespace Scriba;
        const float cy = (float) y + (float) h * 0.5f;
        const auto  trackR = juce::Rectangle<float> ((float) x, cy - 2.0f, (float) w, 4.0f);

        g.setColour (Col::track);
        g.fillRoundedRectangle (trackR, 2.0f);

        const double range = s.getMaximum() - s.getMinimum();
        const float zeroX = (float) x + (float) w * (float) ((0.0 - s.getMinimum()) / range);

        g.setColour (Col::trackLine);
        g.drawLine (zeroX, cy - 8.0f, zeroX, cy + 8.0f, 1.5f);

        g.setColour (Col::teal);
        g.fillRect (juce::Rectangle<float>::leftTopRightBottom (juce::jmin (zeroX, sliderPos), cy - 2.0f,
                                                                juce::jmax (zeroX, sliderPos), cy + 2.0f));

        g.setColour (s.isMouseOverOrDragging() ? Col::tealLight : Col::text);
        g.fillEllipse (sliderPos - 9.0f, cy - 9.0f, 18.0f, 18.0f);
    }

    int getSliderThumbRadius (juce::Slider&) override { return 9; }

    juce::Font getComboBoxFont (juce::ComboBox&) override { return Scriba::font (12.5f, true, 0.02f); }
    juce::Font getPopupMenuFont() override                { return Scriba::font (13.0f, false); }

    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) override
    {
        using namespace Scriba;
        auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) w, (float) h).reduced (0.5f);
        g.setColour (box.isEnabled() ? Col::tile : Col::tile.withAlpha (0.5f));
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (box.hasKeyboardFocus (false) ? Col::teal : Col::panelLowLine);
        g.drawRoundedRectangle (r, 8.0f, 1.0f);

        juce::Path arrow;
        const float ax = (float) w - 18.0f, ay = (float) h * 0.5f;
        arrow.startNewSubPath (ax - 5.0f, ay - 2.5f);
        arrow.lineTo (ax, ay + 2.5f);
        arrow.lineTo (ax + 5.0f, ay - 2.5f);
        g.setColour (box.isEnabled() ? Col::teal : Col::dimmer);
        g.strokePath (arrow, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (8, 1, box.getWidth() - 34, box.getHeight() - 2);
        label.setFont (getComboBoxFont (box));
    }
};
