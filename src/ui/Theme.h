#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "core/ThemePalette.h"

// LYNX design tokens: the single source of colours, type sizes, spacing and radii for the whole UI,
// plus the LookAndFeel that draws every stock JUCE widget with them.
//
// Identity: dark, flat, high contrast, with one red accent used sparingly (selection, active
// states, playhead/record, primary actions). Surfaces get lighter as they rise ("elevation").
namespace lynx::theme
{
// --- colours -----------------------------------------------------------------------------------
// (aliases repeat the hex value: juce::Colour is not constexpr, and copying another inline
// variable during static initialisation is not reliably ordered across translation units)
namespace colour
{
// backgrounds by elevation (0 = deepest)
inline juce::Colour bg0 { 0xff0a0d14 };   // editor canvases, insets, meter wells
inline juce::Colour bg1 { 0xff111520 };   // app background, menu bar, status bar
inline juce::Colour bg2 { 0xff171c28 };   // panels (browser, rack, mixer strips)
inline juce::Colour bg3 { 0xff1f2636 };   // raised surfaces: toolbar, headers, popups
inline juce::Colour bg4 { 0xff283044 };   // controls at rest
inline juce::Colour bg5 { 0xff333d54 };   // controls hovered
inline juce::Colour bg6 { 0xff404c68 };   // controls pressed / neutral "on"

inline juce::Colour border { 0xff2c354a };       // hairlines between areas
inline juce::Colour borderStrong { 0xff424f6b }; // control outlines

inline juce::Colour textPrimary { 0xfff4f6fb };
inline juce::Colour textSecondary { 0xffc6cddb };
inline juce::Colour textDisabled { 0xff8a94a8 };

// the red accent
inline juce::Colour accent { 0xffe02844 };
inline juce::Colour accentHover { 0xfff5425d };
inline juce::Colour accentPressed { 0xffba1e36 };
inline juce::Colour accentMuted { 0xff5e1c2b };  // tinted backgrounds (armed, selected rows)
inline juce::Colour onAccent { 0xffffffff };     // text/icons drawn on the accent

// semantic
inline juce::Colour selection { 0xffe02844 }; // = accent: selected notes/clips outline
inline juce::Colour selectionFill { 0x38e02844 };               // selection rectangles
inline juce::Colour playhead { 0xffe02844 }; // = accent
inline juce::Colour record { 0xffff3b3f };
inline juce::Colour automation { 0xff4cc9c0 };                   // automation curves and clips
inline juce::Colour info { 0xff6ea8fe };                         // secondary values (pan, links)
inline juce::Colour ledOn { 0xff3ddc84 };                        // "active/unmuted" LEDs
inline juce::Colour ledOff { 0xff3b4357 };
inline juce::Colour warning { 0xfff5b942 };                      // markers, missing items
inline juce::Colour meterLow { 0xff3ddc84 };
inline juce::Colour meterMid { 0xfff5c542 };
inline juce::Colour meterClip { 0xffff3b3f };

// step sequencer / editors
inline juce::Colour knobArc { 0xffdce2ee };     // rotary value arcs at rest (accent while touched)
inline juce::Colour knobTrack { 0xff48536b };   // the unlit part of a knob's ring (reads on every panel surface)
inline juce::Colour stepOff { 0xff282e3d };
inline juce::Colour stepOffAlt { 0xff4c3e48 };   // every other group of 4 steps (a dull reddish grey, round 170a2)
inline juce::Colour stepOn { 0xffe02844 }; // = accent
inline juce::Colour stepNote { 0xfff29aa7 };     // a step that only has a Piano Roll note
inline juce::Colour gridBar { 0xff3f485e };      // bar lines
inline juce::Colour gridBeat { 0xff293040 };     // beat lines
inline juce::Colour gridStep { 0xff1d2330 };     // step lines
inline juce::Colour keyWhite { 0xffe2e6f0 };
inline juce::Colour keyBlack { 0xff131722 };
inline juce::Colour rowWhite { 0xff191f2c };     // Piano Roll rows of white keys
inline juce::Colour rowBlack { 0xff131823 };     // rows of black keys
inline juce::Colour noteSoft { 0xffbf4b5c };     // note colour at velocity 0
inline juce::Colour noteHard { 0xffff5c72 };     // note colour at velocity 1
inline juce::Colour noteMuted { 0xff4c5569 };
inline juce::Colour audioClip { 0xff5a87b8 };
} // namespace colour

// Vivid accent colour per workspace window type (0=Playlist, 1=Channel Rack, 2=Piano Roll, 3=Mixer, 4=Browser).
juce::Colour panelAccent (int panelIndex) noexcept;

// User themes (round 162d): the tokens above hold LYNX's built-in look until a theme replaces them (ui/ThemeSettings.h
// applies one live). Code reads the tokens when it paints, so never copy one into a member or a static.
void setPalette (const themes::Palette& palette); // the tokens only (ThemeSettings also refreshes the windows)
themes::Palette currentPalette();

// --- typography ----------------------------------------------------------------------------------
namespace type
{
inline constexpr float caption = 11.0f;  // grid numbers, small captions
inline constexpr float small = 12.0f;    // secondary labels, status bar, tooltips
inline constexpr float body = 13.0f;     // default control text
inline constexpr float title = 15.0f;    // panel titles
inline constexpr float display = 20.0f;  // the transport position
} // namespace type

inline juce::FontOptions font (float size, bool bold = false)
{
    return juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain);
}

// The width a column of labels needs: `minWidth` when every text fits in it (the English layout), else the widest
// text (+ the label borders), up to `maxWidth` (round 165o: translated labels are often longer).
int labelColumnWidth (std::initializer_list<const juce::Label*> labels, int minWidth, int maxWidth);

// A menu item's text with FL's accelerator letter `key` (core/MenuMnemonic.h, round 170a): the letter is underlined in
// the menu and typing it picks the item, e.g. theme::mnemonic (tr ("Delete..."), 'D').
juce::String mnemonic (const juce::String& text, char key);
// Japanese / Korean / Chinese: the installed UI font family LYNX uses for that language (empty: none installed, or a
// language that needs no font of its own — see needsOwnFont).
juce::String uiFontForLanguage (const juce::String& languageId);
bool needsOwnFont (const juce::String& languageId);

// Same for a text button (its padding included).
int buttonWidth (const juce::TextButton& button, int minWidth, int maxWidth);

// --- menus -------------------------------------------------------------------------------------------
// Options › General › Menu density (round 163): compact rows close to native Windows / FL menus (the default),
// or the roomier rows of LYNX 0.1.17 and older.
namespace menus
{
void setCompact (bool compact);
bool compact();
int itemHeight();    // a normal row
int headerHeight();  // a section header (the "DOCUMENTS" titles)
int separatorHeight();
} // namespace menus

inline juce::FontOptions monoFont (float size)
{
    return juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), size, juce::Font::plain);
}

// --- spacing and radii ---------------------------------------------------------------------------
namespace space
{
inline constexpr int xs = 2, sm = 4, md = 8, lg = 12, xl = 16;
}

namespace radius
{
inline constexpr float sm = 3.0f, md = 4.0f, lg = 6.0f;
}

// --- helpers -------------------------------------------------------------------------------------
inline juce::Colour stepOff (int step)
{
    return (step / 4) % 2 == 0 ? colour::stepOff : colour::stepOffAlt; // alternating groups of 4
}

inline juce::Colour muteLed (bool muted)
{
    return muted ? colour::ledOff : colour::ledOn;
}

// Piano Roll note colour by velocity (soft = dark rose, hard = bright red).
inline juce::Colour noteColour (float velocity)
{
    return colour::noteSoft.interpolatedWith (colour::noteHard, juce::jlimit (0.0f, 1.0f, velocity));
}

// Note colour groups (REA round 153): group 0 is LYNX's note colour, groups 1..15 LYNX's own hue wheel; the velocity
// sets the brightness, as for group 0.
inline juce::Colour noteGroupHue (int group)
{
    if (group <= 0)
        return colour::noteHard;
    return juce::Colour::fromHSV (std::fmod (0.02f + (float) group / 16.0f, 1.0f), 0.62f, 0.95f, 1.0f);
}

inline juce::Colour noteGroupColour (int group, float velocity)
{
    if (group <= 0)
        return noteColour (velocity);
    const auto hue = noteGroupHue (group);
    return hue.withMultipliedBrightness (0.68f + 0.32f * juce::jlimit (0.0f, 1.0f, velocity));
}

// A user/channel colour toned down to sit on the dark surfaces without shouting.
inline juce::Colour tame (juce::Colour c)
{
    return c.withMultipliedSaturation (0.8f).withBrightness (juce::jmin (c.getBrightness(), 0.72f));
}

// Upper case for small caption headers, including accented Latin letters (ã -> Ã), which the C
// runtime's towupper leaves alone on Windows.
inline juce::String caps (const juce::String& text)
{
    juce::String out;
    for (auto c : text)
    {
        if ((c >= 0xe0 && c <= 0xfe && c != 0xf7))
            c -= 0x20;
        else
            c = juce::CharacterFunctions::toUpperCase (c);
        out += juce::String::charToString (c);
    }
    return out;
}

// g.setColour (colour) + g.fillRoundedRectangle, faster where the frame is drawn on the CPU (Linux): nothing outside the
// clip, a plain rectangle when the clip misses the corners, the shape (or a big one's corners) from a cached sprite.
void fillRoundRect (juce::Graphics& g, juce::Rectangle<float> r, float corner, juce::Colour colour);

// FL's number wheel ("digiwheel": channel RANGE / TRACK, the rack's insert box) in the LYNX skin: a dark inset display,
// red only while hovered or dragged. Draws the box (with FL's small up / down marks at its right when `marks`); returns
// the colour for its value text, which goes in box.withTrimmedRight (digitMarksWidth) when marked (round 168s).
constexpr float digitMarksWidth = 9.0f;
juce::Colour drawDigitDisplay (juce::Graphics& g, juce::Rectangle<float> box, bool hot, bool dimmed = false, bool marks = false);

// Draws a rotary value arc in the LYNX style (used by custom-painted knobs too).
void drawKnob (juce::Graphics& g, juce::Rectangle<float> bounds, float value01, bool bipolar, juce::Colour arc, bool hover = false);

// --- legacy names (kept so code written against the old palette keeps compiling) ---------------
inline juce::Colour window { 0xff111520 }; // = colour::bg1
inline juce::Colour panel { 0xff171c28 }; // = colour::bg2
inline juce::Colour toolbar { 0xff1f2636 }; // = colour::bg3
inline juce::Colour channelButton { 0xff283044 }; // = colour::bg4
inline juce::Colour channelButtonSelected { 0xff404c68 }; // = colour::bg6
inline juce::Colour knobBackground { 0xff0a0d14 }; // = colour::bg0
inline juce::Colour stepOn { 0xffe02844 }; // = colour::stepOn
inline juce::Colour stepOnOffGrid { 0xfff29aa7 }; // = colour::stepNote
inline juce::Colour accentOrange { 0xffe02844 }; // = colour::accent
inline juce::Colour accentGreen { 0xff3ddc84 }; // = colour::ledOn
inline juce::Colour accentBlue { 0xff6ea8fe }; // = colour::info
inline juce::Colour playhead { 0xffe02844 }; // = colour::playhead
inline juce::Colour text { 0xfff4f6fb }; // = colour::textPrimary

// --- LookAndFeel ---------------------------------------------------------------------------------
class LookAndFeel final : public juce::LookAndFeel_V4
{
public:
    LookAndFeel();

    // re-reads the colour tokens into the stock widgets' colours (after a theme change)
    void refreshColours();

    // Japanese, Korean and Chinese: the OS's UI font for that script becomes the default sans-serif (Segoe UI and the
    // other Latin UI fonts have no CJK glyphs; the font fallback would otherwise pick a CJK face per run, which may be
    // a Chinese face for Japanese text). Returns the chosen family, or empty when the default is kept.
    juce::String applyLanguageFont (const juce::String& languageId);

    // property set on a Button / TabBarButton to draw one of lynx::Icon next to (or instead of) its text
    static constexpr const char* iconProperty = "lynxIcon";

    // buttons
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour, bool highlighted, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool highlighted, bool down) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawTickBox (juce::Graphics&, juce::Component&, float x, float y, float w, float h, bool ticked, bool enabled, bool highlighted, bool down) override;

    // combo boxes and menus
    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown, int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    void drawPopupMenuBackground (juce::Graphics&, int width, int height) override;
    void drawPopupMenuItem (juce::Graphics&, const juce::Rectangle<int>& area, bool isSeparator, bool isActive, bool isHighlighted,
                            bool isTicked, bool hasSubMenu, const juce::String& text, const juce::String& shortcutKeyText,
                            const juce::Drawable* icon, const juce::Colour* textColour) override;
    void drawPopupMenuSectionHeader (juce::Graphics&, const juce::Rectangle<int>& area, const juce::String& sectionName) override;
    juce::Font getPopupMenuFont() override;
    void getIdealPopupMenuItemSize (const juce::String& text, bool isSeparator, int standardMenuItemHeight, int& idealWidth, int& idealHeight) override;
    int getPopupMenuBorderSize() override { return 4; }
    // keeps a submenu from opening over its own multi-column parent menu (round 162d: JUCE then misses the click on
    // the submenu's items — the mixer track menu's "Change color" did nothing, issue #9)
    void preparePopupMenuWindow (juce::Component& newWindow) override;
    void drawMenuBarBackground (juce::Graphics&, int width, int height, bool isMouseOverBar, juce::MenuBarComponent&) override;
    void drawMenuBarItem (juce::Graphics&, int width, int height, int itemIndex, const juce::String& itemText, bool isMouseOverItem,
                          bool isMenuOpen, bool isMouseOverBar, juce::MenuBarComponent&) override;
    juce::Font getMenuBarFont (juce::MenuBarComponent&, int itemIndex, const juce::String& itemText) override;
    int getMenuBarItemWidth (juce::MenuBarComponent&, int itemIndex, const juce::String& itemText) override;

    // sliders
    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos, float minSliderPos, float maxSliderPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
    juce::Button* createSliderButton (juce::Slider&, bool isIncrement) override;
    juce::Slider::SliderLayout getSliderLayout (juce::Slider&) override;

    // scrollbars
    void drawScrollbar (juce::Graphics&, juce::ScrollBar&, int x, int y, int width, int height, bool isScrollbarVertical, int thumbStartPosition,
                        int thumbSize, bool isMouseOver, bool isMouseDown) override;
    int getDefaultScrollbarWidth() override { return 10; }
    bool areScrollbarButtonsVisible() override { return false; }

    // tabs
    int getTabButtonBestWidth (juce::TabBarButton&, int tabDepth) override;
    void drawTabButton (juce::TabBarButton&, juce::Graphics&, bool isMouseOver, bool isMouseDown) override;
    void drawTabbedButtonBarBackground (juce::TabbedButtonBar&, juce::Graphics&) override;
    void drawTabAreaBehindFrontButton (juce::TabbedButtonBar&, juce::Graphics&, int w, int h) override;

    // text
    void fillTextEditorBackground (juce::Graphics&, int width, int height, juce::TextEditor&) override;
    void drawTextEditorOutline (juce::Graphics&, int width, int height, juce::TextEditor&) override;
    void drawLabel (juce::Graphics&, juce::Label&) override;

    // tooltips, alerts, windows
    juce::Rectangle<int> getTooltipBounds (const juce::String& tipText, juce::Point<int> screenPos, juce::Rectangle<int> parentArea) override;
    void drawTooltip (juce::Graphics&, const juce::String& text, int width, int height) override;
    void drawAlertBox (juce::Graphics&, juce::AlertWindow&, const juce::Rectangle<int>& textArea, juce::TextLayout&) override;
    int getAlertWindowButtonHeight() override { return 30; }
    juce::Font getAlertWindowTitleFont() override;
    juce::Font getAlertWindowMessageFont() override;
    juce::Font getAlertWindowFont() override;
    void drawDocumentWindowTitleBar (juce::DocumentWindow&, juce::Graphics&, int w, int h, int titleSpaceX, int titleSpaceW,
                                     const juce::Image* icon, bool drawTitleTextOnLeft) override;
    juce::Button* createDocumentWindowButton (int buttonType) override;
    void drawCornerResizer (juce::Graphics&, int w, int h, bool isMouseOver, bool isMouseDragging) override;
    void drawProgressBar (juce::Graphics&, juce::ProgressBar&, int width, int height, double progress, const juce::String& textToShow) override;
    void drawGroupComponentOutline (juce::Graphics&, int w, int h, const juce::String& text, const juce::Justification&, juce::GroupComponent&) override;

    // file browser (the LYNX browser panel is a FileTreeComponent)
    void drawFileBrowserRow (juce::Graphics&, int width, int height, const juce::File& file, const juce::String& filename, juce::Image* icon,
                             const juce::String& fileSizeDescription, const juce::String& fileTimeDescription, bool isDirectory,
                             bool isItemSelected, int itemIndex, juce::DirectoryContentsDisplayComponent&) override;
    void drawTreeviewPlusMinusBox (juce::Graphics&, const juce::Rectangle<float>& area, juce::Colour backgroundColour, bool isOpen, bool isMouseOver) override;
};
} // namespace lynx::theme
