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
        hoveredIndex = -1;  // Reset hover state
        
        int itemHeight = 30;
        int headerHeight = 40;
        int numItems = hitInfo.alternatives.size() + 1;
        int width = 180;
        int height = headerHeight + numItems * itemHeight + 20;
        
        setSize(width, height);
        
        if (parent != nullptr)
        {
            // Position popup to the right of the note, with a clear offset
            int popupX = static_cast<int>(hitInfo.noteBounds.getRight()) + 15;  // 15px offset to the right
            
            // If it would go off the right edge, put it to the left of the note
            if (popupX + width > parent->getWidth() - 5)
            {
                popupX = static_cast<int>(hitInfo.noteBounds.getX()) - width - 15;
            }
            
            // Ensure X is within bounds
            popupX = juce::jlimit(5, parent->getWidth() - width - 5, popupX);
            
            // Vertically center the popup relative to the note
            int popupY = static_cast<int>(hitInfo.noteBounds.getCentreY()) - height / 2;
            popupY = juce::jlimit(5, parent->getHeight() - height - 5, popupY);
            
            setBounds(popupX, popupY, width, height);
            parent->addAndMakeVisible(this);
            grabKeyboardFocus();
        }
        repaint();
    }
    
    void hide()
    {
        // Clear ghost preview before hiding
        if (onHoverPositionChanged && currentHitInfo.valid)
        {
            AlternatePosition noHover;
            noHover.string = -1;
            noHover.fret = -1;
            onHoverPositionChanged(currentHitInfo, noHover);
        }
        
        if (auto* parent = getParentComponent())
            parent->removeChildComponent(this);
        currentHitInfo = NoteHitInfo();
        hoveredIndex = -1;
    }
    
    bool isShowing() const { return currentHitInfo.valid; }
    
    std::function<void(const NoteHitInfo&, const AlternatePosition&)> onPositionSelected;
    
    /** Callback when hovering over an alternative position - for ghost preview.
     *  The AlternatePosition contains the hovered string/fret, or string=-1 if nothing hovered.
     */
    std::function<void(const NoteHitInfo&, const AlternatePosition&)> onHoverPositionChanged;
    
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
        
        if (newHoveredIndex != hoveredIndex)
        {
            hoveredIndex = newHoveredIndex;
            repaint();
            
            // Notify parent about hover change for ghost preview
            if (onHoverPositionChanged)
            {
                AlternatePosition hoveredPos;
                if (hoveredIndex > 0 && hoveredIndex - 1 < currentHitInfo.alternatives.size())
                {
                    // Hovering over an alternative position
                    hoveredPos = currentHitInfo.alternatives[hoveredIndex - 1];
                }
                else
                {
                    // Not hovering over any alternative (index 0 is current position, or nothing)
                    hoveredPos.string = -1;
                    hoveredPos.fret = -1;
                }
                onHoverPositionChanged(currentHitInfo, hoveredPos);
            }
        }
    }
    
    void mouseDown(const juce::MouseEvent&) override
    {
        if (hoveredIndex <= 0) { hide(); return; }
        
        int altIndex = hoveredIndex - 1;
        if (altIndex >= 0 && altIndex < currentHitInfo.alternatives.size() && onPositionSelected)
            onPositionSelected(currentHitInfo, currentHitInfo.alternatives[altIndex]);
        hide();
    }
    
    void mouseExit(const juce::MouseEvent&) override
    {
        if (hoveredIndex != -1)
        {
            hoveredIndex = -1;
            repaint();
            // Clear ghost preview
            if (onHoverPositionChanged)
            {
                AlternatePosition noHover;
                noHover.string = -1;
                noHover.fret = -1;
                onHoverPositionChanged(currentHitInfo, noHover);
            }
        }
    }
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

//==============================================================================
/**
 * GroupNoteEditPopup - Popup zur Auswahl alternativer Positionen für eine Gruppe von Noten.
 */
class GroupNoteEditPopup : public juce::Component
{
public:
    GroupNoteEditPopup() { setSize(280, 200); setAlwaysOnTop(true); }
    
    void showForGroup(const juce::Array<NoteHitInfo>& notes,
                      const juce::Array<FretPositionCalculator::GroupAlternative>& alternatives,
                      const juce::Array<int>& tuning,
                      juce::Component* parent,
                      juce::Rectangle<float> groupBounds)
    {
        selectedNotes = notes;
        groupAlternatives = alternatives;
        this->tuning = tuning;
        hoveredIndex = -1;
        
        int itemHeight = 35;
        int headerHeight = 45;
        int numItems = juce::jmin(5, alternatives.size()) + 1;  // +1 für aktuelle Position
        int width = 280;
        int height = headerHeight + numItems * itemHeight + 15;
        
        setSize(width, height);
        
        if (parent != nullptr)
        {
            // Position popup to the right of the group
            int popupX = static_cast<int>(groupBounds.getRight()) + 15;
            
            if (popupX + width > parent->getWidth() - 5)
            {
                popupX = static_cast<int>(groupBounds.getX()) - width - 15;
            }
            
            popupX = juce::jlimit(5, parent->getWidth() - width - 5, popupX);
            
            int popupY = static_cast<int>(groupBounds.getCentreY()) - height / 2;
            popupY = juce::jlimit(5, parent->getHeight() - height - 5, popupY);
            
            setBounds(popupX, popupY, width, height);
            parent->addAndMakeVisible(this);
            grabKeyboardFocus();
        }
        repaint();
    }
    
    void hide()
    {
        // Clear ghost preview
        if (onGroupHoverChanged)
        {
            FretPositionCalculator::GroupAlternative noHover;
            onGroupHoverChanged(selectedNotes, noHover, false);
        }
        
        if (auto* parent = getParentComponent())
            parent->removeChildComponent(this);
        selectedNotes.clear();
        groupAlternatives.clear();
        hoveredIndex = -1;
    }
    
    bool isShowing() const { return !selectedNotes.isEmpty(); }
    
    /** Callback when a group alternative is selected */
    std::function<void(const juce::Array<NoteHitInfo>&, const FretPositionCalculator::GroupAlternative&)> onGroupSelected;
    
    /** Callback when hovering over a group alternative - for ghost preview */
    std::function<void(const juce::Array<NoteHitInfo>&, const FretPositionCalculator::GroupAlternative&, bool active)> onGroupHoverChanged;
    
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
        g.drawText("Group Position (" + juce::String(selectedNotes.size()) + " notes)",
                   getLocalBounds().removeFromTop(40).reduced(10, 8),
                   juce::Justification::centredLeft, true);
        
        g.setColour(juce::Colour(0xFF555555));
        g.drawHorizontalLine(42, 10, getWidth() - 10);
        
        // Aktuelle Position
        int y = 48, itemHeight = 35;
        drawGroupItem(g, -1, y, true, hoveredIndex == 0);
        y += itemHeight;
        
        // Alternativen
        int altIndex = 1;
        for (int i = 0; i < juce::jmin(5, groupAlternatives.size()); ++i)
        {
            drawGroupItem(g, i, y, false, hoveredIndex == altIndex);
            y += itemHeight;
            altIndex++;
        }
    }
    
    void mouseMove(const juce::MouseEvent& event) override
    {
        int itemHeight = 35, startY = 48;
        int newHoveredIndex = -1;
        int totalItems = 1 + juce::jmin(5, groupAlternatives.size());
        
        for (int i = 0; i < totalItems; ++i)
        {
            if (event.y >= startY + i * itemHeight && event.y < startY + (i + 1) * itemHeight)
            {
                newHoveredIndex = i;
                break;
            }
        }
        
        if (newHoveredIndex != hoveredIndex)
        {
            hoveredIndex = newHoveredIndex;
            repaint();
            
            // Notify parent about hover change for ghost preview
            if (onGroupHoverChanged)
            {
                if (hoveredIndex > 0 && hoveredIndex - 1 < groupAlternatives.size())
                {
                    onGroupHoverChanged(selectedNotes, groupAlternatives[hoveredIndex - 1], true);
                }
                else
                {
                    FretPositionCalculator::GroupAlternative noHover;
                    onGroupHoverChanged(selectedNotes, noHover, false);
                }
            }
        }
    }
    
    void mouseDown(const juce::MouseEvent&) override
    {
        if (hoveredIndex <= 0) { hide(); return; }
        
        int altIndex = hoveredIndex - 1;
        if (altIndex >= 0 && altIndex < groupAlternatives.size() && onGroupSelected)
            onGroupSelected(selectedNotes, groupAlternatives[altIndex]);
        hide();
    }
    
    void mouseExit(const juce::MouseEvent&) override
    {
        if (hoveredIndex != -1)
        {
            hoveredIndex = -1;
            repaint();
            if (onGroupHoverChanged)
            {
                FretPositionCalculator::GroupAlternative noHover;
                onGroupHoverChanged(selectedNotes, noHover, false);
            }
        }
    }
    
    void focusLost(FocusChangeType) override { hide(); }
    bool keyPressed(const juce::KeyPress& key) override
    {
        if (key == juce::KeyPress::escapeKey) { hide(); return true; }
        return false;
    }
    
private:
    juce::Array<NoteHitInfo> selectedNotes;
    juce::Array<FretPositionCalculator::GroupAlternative> groupAlternatives;
    juce::Array<int> tuning;
    int hoveredIndex = -1;
    
    void drawGroupItem(juce::Graphics& g, int altIndex, int y, bool isCurrent, bool isHovered)
    {
        juce::Rectangle<int> itemBounds(10, y, getWidth() - 20, 33);
        
        if (isHovered)
        {
            g.setColour(juce::Colour(0xFF4A90D9).withAlpha(0.3f));
            g.fillRoundedRectangle(itemBounds.toFloat(), 4.0f);
        }
        
        if (isCurrent)
        {
            g.setColour(juce::Colours::limegreen);
            g.fillEllipse(itemBounds.getX() + 5.0f, y + 11.0f, 10.0f, 10.0f);
        }
        
        g.setColour(isCurrent ? juce::Colours::white : juce::Colours::lightgrey);
        g.setFont(juce::Font(juce::FontOptions(12.0f)));
        
        juce::String text;
        if (isCurrent)
        {
            text = "Current: ";
            for (int i = 0; i < selectedNotes.size(); ++i)
            {
                if (i > 0) text += ", ";
                text += juce::String(selectedNotes[i].stringIndex) + "/" + juce::String(selectedNotes[i].fret);
            }
        }
        else if (altIndex >= 0 && altIndex < groupAlternatives.size())
        {
            const auto& alt = groupAlternatives[altIndex];
            text = "Frets " + juce::String(alt.averageFret - alt.fretSpan/2) + 
                   "-" + juce::String(alt.averageFret + alt.fretSpan/2 + 1) + ": ";
            for (int i = 0; i < alt.positions.size(); ++i)
            {
                if (i > 0) text += ", ";
                text += juce::String(alt.positions[i].string) + "/" + juce::String(alt.positions[i].fret);
            }
        }
        
        g.drawText(text, itemBounds.getX() + (isCurrent ? 20 : 5), y, 
                   itemBounds.getWidth() - 25, 33,
                   juce::Justification::centredLeft, false);
    }
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GroupNoteEditPopup)
};
