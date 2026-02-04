/*
  ==============================================================================

    NoteEditComponent.h
    
    Komponenten für das Bearbeiten von einzelnen Noten:
    - NoteHitInfo: Informationen über eine angeklickte Note
    - NoteEditPopup: Popup-Menü zur Auswahl alternativer Positionen

  ==============================================================================
*/

#pragma once

#include "TabModels.h"
#include "FretPositionCalculator.h"
#include <juce_gui_basics/juce_gui_basics.h>

//==============================================================================
/**
 * NoteHitInfo - Informationen über eine angeklickte Note.
 */
struct NoteHitInfo
{
    bool valid = false;
    int measureIndex = -1;
    int beatIndex = -1;
    int noteIndex = -1;
    int stringIndex = 0;
    int fret = 0;
    int midiNote = -1;
    juce::Rectangle<float> noteBounds;
    TabNote* notePtr = nullptr;
    juce::Array<AlternatePosition> alternatives;
};

//==============================================================================
/**
 * NoteEditPopup - Popup zur Auswahl alternativer Positionen.
 */
class NoteEditPopup : public juce::Component
{
public:
    NoteEditPopup() { setSize(200, 150); setAlwaysOnTop(true); }
    
    void showForNote(const NoteHitInfo& hitInfo, const juce::Array<int>& tuning, juce::Component* parent)
    {
        currentHitInfo = hitInfo;
        this->tuning = tuning;
        
        int itemHeight = 30;
        int headerHeight = 40;
        int numItems = hitInfo.alternatives.size() + 1;
        int width = 180;
        int height = headerHeight + numItems * itemHeight + 20;
        
        setSize(width, height);
        
        if (parent != nullptr)
        {
            auto noteCentre = hitInfo.noteBounds.getCentre();
            int popupX = juce::jlimit(5, parent->getWidth() - width - 5, 
                                       static_cast<int>(noteCentre.x) - width / 2);
            int popupY = juce::jlimit(5, parent->getHeight() - height - 5,
                                       static_cast<int>(hitInfo.noteBounds.getBottom()) + 5);
            setBounds(popupX, popupY, width, height);
            parent->addAndMakeVisible(this);
            grabKeyboardFocus();
        }
        repaint();
    }
    
    void hide()
    {
        if (auto* parent = getParentComponent())
            parent->removeChildComponent(this);
        currentHitInfo = NoteHitInfo();
    }
    
    bool isShowing() const { return currentHitInfo.valid; }
    
    std::function<void(const NoteHitInfo&, const AlternatePosition&)> onPositionSelected;
    
    void paint(juce::Graphics& g) override
    {
        // Hintergrund
        g.setColour(juce::Colour(0xFF2D2D30));
        g.fillRoundedRectangle(getLocalBounds().toFloat(), 8.0f);
        g.setColour(juce::Colour(0xFF4A90D9));
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1), 8.0f, 2.0f);
        
        // Header
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(juce::FontOptions(14.0f)).boldened());
        juce::String noteName = FretPositionCalculator::getMidiNoteName(currentHitInfo.midiNote);
        g.drawText("Position for " + noteName, getLocalBounds().removeFromTop(35).reduced(10, 5),
                   juce::Justification::centredLeft, true);
        
        g.setColour(juce::Colour(0xFF555555));
        g.drawHorizontalLine(38, 10, getWidth() - 10);
        
        // Optionen
        int y = 45, itemHeight = 30;
        drawPositionItem(g, currentHitInfo.stringIndex, currentHitInfo.fret, y, true, hoveredIndex == 0);
        y += itemHeight;
        
        int altIndex = 1;
        for (const auto& alt : currentHitInfo.alternatives)
        {
            drawPositionItem(g, alt.string, alt.fret, y, false, hoveredIndex == altIndex);
            y += itemHeight;
            altIndex++;
        }
    }
    
    void mouseMove(const juce::MouseEvent& event) override
    {
        int itemHeight = 30, startY = 45;
        int newHoveredIndex = -1;
        int totalItems = 1 + currentHitInfo.alternatives.size();
        
        for (int i = 0; i < totalItems; ++i)
        {
            if (event.y >= startY + i * itemHeight && event.y < startY + (i + 1) * itemHeight)
            {
                newHoveredIndex = i;
                break;
            }
        }
        
        if (newHoveredIndex != hoveredIndex) { hoveredIndex = newHoveredIndex; repaint(); }
    }
    
    void mouseDown(const juce::MouseEvent&) override
    {
        if (hoveredIndex <= 0) { hide(); return; }
        
        int altIndex = hoveredIndex - 1;
        if (altIndex >= 0 && altIndex < currentHitInfo.alternatives.size() && onPositionSelected)
            onPositionSelected(currentHitInfo, currentHitInfo.alternatives[altIndex]);
        hide();
    }
    
    void mouseExit(const juce::MouseEvent&) override { hoveredIndex = -1; repaint(); }
    void focusLost(FocusChangeType) override { hide(); }
    bool keyPressed(const juce::KeyPress& key) override
    {
        if (key == juce::KeyPress::escapeKey) { hide(); return true; }
        return false;
    }
    
private:
    NoteHitInfo currentHitInfo;
    juce::Array<int> tuning;
    int hoveredIndex = -1;
    
    void drawPositionItem(juce::Graphics& g, int stringIdx, int fret, int y, bool isCurrent, bool isHovered)
    {
        juce::Rectangle<int> itemBounds(10, y, getWidth() - 20, 28);
        
        if (isHovered)
        {
            g.setColour(juce::Colour(0xFF4A90D9).withAlpha(0.3f));
            g.fillRoundedRectangle(itemBounds.toFloat(), 4.0f);
        }
        
        if (isCurrent)
        {
            g.setColour(juce::Colours::limegreen);
            g.fillEllipse(itemBounds.getX() + 5.0f, y + 9.0f, 10.0f, 10.0f);
        }
        
        g.setColour(isCurrent ? juce::Colours::white : juce::Colours::lightgrey);
        g.setFont(juce::Font(juce::FontOptions(13.0f)));
        
        juce::String saiteName = (stringIdx >= 0 && stringIdx < tuning.size()) 
            ? FretPositionCalculator::getMidiNoteName(tuning[stringIdx]) 
            : juce::String(stringIdx + 1);
        g.drawText("String " + saiteName + ", Fret " + juce::String(fret),
                   itemBounds.getX() + (isCurrent ? 20 : 5), y, itemBounds.getWidth() - 25, 28,
                   juce::Justification::centredLeft, false);
    }
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NoteEditPopup)
};

