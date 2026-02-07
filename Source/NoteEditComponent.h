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
    NoteEditPopup() { setSize(240, 150); setAlwaysOnTop(true); }
    
    void showForNote(const NoteHitInfo& hitInfo, const juce::Array<int>& tuning, 
                     juce::Component* parent, NoteDuration currentDuration = NoteDuration::Quarter,
                     bool currentDotted = false)
    {
        currentHitInfo = hitInfo;
        this->tuning = tuning;
        this->beatDuration = currentDuration;
        this->beatDotted = currentDotted;
        hoveredIndex = -1;  // Reset hover state
        deleteHovered = false;
        durationHovered = -1;
        
        int itemHeight = 30;
        int headerHeight = 40;
        int numItems = hitInfo.alternatives.size() + 1;
        int durationSectionHeight = 70;  // Duration selector section
        int deleteSectionHeight = 36;    // Delete button
        int width = 240;
        int height = headerHeight + numItems * itemHeight + durationSectionHeight + deleteSectionHeight + 30;
        
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
    
    /** Callback when hovering over an alternative position - for ghost preview. */
    std::function<void(const NoteHitInfo&, const AlternatePosition&)> onHoverPositionChanged;
    
    /** Callback when the delete button is clicked */
    std::function<void(const NoteHitInfo&)> onNoteDeleteRequested;
    
    /** Callback when a duration is selected: hitInfo, newDuration, isDotted */
    std::function<void(const NoteHitInfo&, NoteDuration, bool)> onDurationChangeRequested;
    
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
        g.drawText("Edit: " + noteName + " (Fret " + juce::String(currentHitInfo.fret) + ")", 
                   getLocalBounds().removeFromTop(35).reduced(10, 5),
                   juce::Justification::centredLeft, true);
        
        g.setColour(juce::Colour(0xFF555555));
        g.drawHorizontalLine(38, 10, getWidth() - 10);
        
        // Position options
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
        
        // Separator
        y += 5;
        g.setColour(juce::Colour(0xFF555555));
        g.drawHorizontalLine(y, 10, getWidth() - 10);
        y += 8;
        
        // Duration section
        durationSectionY = y;
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(juce::FontOptions(11.0f)));
        g.drawText("Duration:", 10, y, 60, 18, juce::Justification::centredLeft, false);
        y += 20;
        
        // Duration buttons row
        struct DurInfo { NoteDuration dur; const char* label; const char* key; };
        DurInfo durations[] = {
            { NoteDuration::Whole, "W", "1" },
            { NoteDuration::Half, "H", "2" },
            { NoteDuration::Quarter, "Q", "3" },
            { NoteDuration::Eighth, "8", "4" },
            { NoteDuration::Sixteenth, "16", "5" },
            { NoteDuration::ThirtySecond, "32", "6" }
        };
        
        int btnWidth = 28, btnSpacing = 3;
        int totalBtnsWidth = 6 * btnWidth + 5 * btnSpacing;
        int dotBtnWidth = 22;
        int dotGap = 6;
        int totalRowWidth = totalBtnsWidth + dotGap + dotBtnWidth;
        int btnX = (getWidth() - totalRowWidth) / 2;
        
        for (int i = 0; i < 6; ++i)
        {
            juce::Rectangle<int> btnRect(btnX + i * (btnWidth + btnSpacing), y, btnWidth, 24);
            bool isCurrent = (durations[i].dur == beatDuration && !beatDotted);
            bool isHover = (durationHovered == i);
            
            if (isCurrent)
            {
                g.setColour(juce::Colour(0xFF4A90D9));
                g.fillRoundedRectangle(btnRect.toFloat(), 3.0f);
            }
            else if (isHover)
            {
                g.setColour(juce::Colour(0xFF4A90D9).withAlpha(0.4f));
                g.fillRoundedRectangle(btnRect.toFloat(), 3.0f);
            }
            else
            {
                g.setColour(juce::Colour(0xFF444448));
                g.fillRoundedRectangle(btnRect.toFloat(), 3.0f);
            }
            
            g.setColour(juce::Colours::white);
            g.setFont(juce::Font(juce::FontOptions(10.0f)).boldened());
            g.drawText(durations[i].label, btnRect, juce::Justification::centred, false);
            
            durationBtnRects[i] = btnRect;
        }
        
        // Dot toggle button
        int dotBtnX = btnX + totalBtnsWidth + 6;
        juce::Rectangle<int> dotRect(dotBtnX, y, 22, 24);
        bool dotCurrent = beatDotted;
        bool dotHover = (durationHovered == 6);
        
        if (dotCurrent)
        {
            g.setColour(juce::Colour(0xFFD9904A));
            g.fillRoundedRectangle(dotRect.toFloat(), 3.0f);
        }
        else if (dotHover)
        {
            g.setColour(juce::Colour(0xFFD9904A).withAlpha(0.4f));
            g.fillRoundedRectangle(dotRect.toFloat(), 3.0f);
        }
        else
        {
            g.setColour(juce::Colour(0xFF444448));
            g.fillRoundedRectangle(dotRect.toFloat(), 3.0f);
        }
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(juce::FontOptions(14.0f)).boldened());
        g.drawText(".", dotRect, juce::Justification::centred, false);
        dotBtnRect = dotRect;
        
        y += 30;
        
        // Separator
        g.setColour(juce::Colour(0xFF555555));
        g.drawHorizontalLine(y, 10, getWidth() - 10);
        y += 6;
        
        // Delete button
        deleteBtnY = y;
        juce::Rectangle<int> delRect(10, y, getWidth() - 20, 28);
        deleteBtnRect = delRect;
        
        if (deleteHovered)
        {
            g.setColour(juce::Colour(0xFFCC3333));
            g.fillRoundedRectangle(delRect.toFloat(), 4.0f);
        }
        else
        {
            g.setColour(juce::Colour(0xFF993333).withAlpha(0.6f));
            g.fillRoundedRectangle(delRect.toFloat(), 4.0f);
        }
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(juce::FontOptions(12.0f)).boldened());
        g.drawText(juce::CharPointer_UTF8("\xf0\x9f\x97\x91 Delete Note  (Del)"), delRect, juce::Justification::centred, false);
    }
    
    void mouseMove(const juce::MouseEvent& event) override
    {
        // Check position items
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
        
        // Check duration buttons
        int newDurationHovered = -1;
        for (int i = 0; i < 6; ++i)
        {
            if (durationBtnRects[i].contains(event.getPosition()))
            {
                newDurationHovered = i;
                break;
            }
        }
        if (dotBtnRect.contains(event.getPosition()))
            newDurationHovered = 6;
        
        // Check delete button
        bool newDeleteHovered = deleteBtnRect.contains(event.getPosition());
        
        if (newHoveredIndex != hoveredIndex || newDurationHovered != durationHovered || newDeleteHovered != deleteHovered)
        {
            hoveredIndex = newHoveredIndex;
            durationHovered = newDurationHovered;
            deleteHovered = newDeleteHovered;
            repaint();
            
            // Notify parent about hover change for ghost preview
            if (onHoverPositionChanged)
            {
                AlternatePosition hoveredPos;
                if (hoveredIndex > 0 && hoveredIndex - 1 < currentHitInfo.alternatives.size())
                {
                    hoveredPos = currentHitInfo.alternatives[hoveredIndex - 1];
                }
                else
                {
                    hoveredPos.string = -1;
                    hoveredPos.fret = -1;
                }
                onHoverPositionChanged(currentHitInfo, hoveredPos);
            }
        }
    }
    
    void mouseDown(const juce::MouseEvent& event) override
    {
        // Check delete button first
        if (deleteBtnRect.contains(event.getPosition()))
        {
            if (onNoteDeleteRequested)
                onNoteDeleteRequested(currentHitInfo);
            hide();
            return;
        }
        
        // Check duration buttons
        for (int i = 0; i < 6; ++i)
        {
            if (durationBtnRects[i].contains(event.getPosition()))
            {
                NoteDuration durations[] = { NoteDuration::Whole, NoteDuration::Half, NoteDuration::Quarter,
                                              NoteDuration::Eighth, NoteDuration::Sixteenth, NoteDuration::ThirtySecond };
                if (onDurationChangeRequested)
                    onDurationChangeRequested(currentHitInfo, durations[i], false);
                // Update local state and repaint (don't hide)
                beatDuration = durations[i];
                beatDotted = false;
                repaint();
                return;
            }
        }
        
        // Check dot toggle
        if (dotBtnRect.contains(event.getPosition()))
        {
            beatDotted = !beatDotted;
            if (onDurationChangeRequested)
                onDurationChangeRequested(currentHitInfo, beatDuration, beatDotted);
            repaint();
            return;
        }
        
        // Position selection
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
        
        // Delete key
        if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
        {
            if (onNoteDeleteRequested)
                onNoteDeleteRequested(currentHitInfo);
            hide();
            return true;
        }
        
        // Duration keys 1-6
        NoteDuration durations[] = { NoteDuration::Whole, NoteDuration::Half, NoteDuration::Quarter,
                                      NoteDuration::Eighth, NoteDuration::Sixteenth, NoteDuration::ThirtySecond };
        for (int i = 0; i < 6; ++i)
        {
            if (key.getTextCharacter() == ('1' + i))
            {
                if (onDurationChangeRequested)
                    onDurationChangeRequested(currentHitInfo, durations[i], false);
                beatDuration = durations[i];
                beatDotted = false;
                repaint();
                return true;
            }
        }
        
        // Dot toggle
        if (key.getTextCharacter() == '.')
        {
            beatDotted = !beatDotted;
            if (onDurationChangeRequested)
                onDurationChangeRequested(currentHitInfo, beatDuration, beatDotted);
            repaint();
            return true;
        }
        
        // + increase duration, - decrease duration
        if (key.getTextCharacter() == '+' || key.getTextCharacter() == '=')
        {
            auto newDur = getNextLongerDuration(beatDuration);
            if (newDur != beatDuration)
            {
                beatDuration = newDur;
                beatDotted = false;
                if (onDurationChangeRequested)
                    onDurationChangeRequested(currentHitInfo, beatDuration, beatDotted);
                repaint();
            }
            return true;
        }
        if (key.getTextCharacter() == '-')
        {
            auto newDur = getNextShorterDuration(beatDuration);
            if (newDur != beatDuration)
            {
                beatDuration = newDur;
                beatDotted = false;
                if (onDurationChangeRequested)
                    onDurationChangeRequested(currentHitInfo, beatDuration, beatDotted);
                repaint();
            }
            return true;
        }
        
        return false;
    }
    
private:
    NoteHitInfo currentHitInfo;
    juce::Array<int> tuning;
    int hoveredIndex = -1;
    
    // Duration state
    NoteDuration beatDuration = NoteDuration::Quarter;
    bool beatDotted = false;
    int durationSectionY = 0;
    int deleteBtnY = 0;
    juce::Rectangle<int> durationBtnRects[6];
    juce::Rectangle<int> dotBtnRect;
    juce::Rectangle<int> deleteBtnRect;
    int durationHovered = -1;  // -1 = none, 0-5 = duration buttons, 6 = dot
    bool deleteHovered = false;
    
    static NoteDuration getNextLongerDuration(NoteDuration d)
    {
        switch (d)
        {
            case NoteDuration::ThirtySecond: return NoteDuration::Sixteenth;
            case NoteDuration::Sixteenth:    return NoteDuration::Eighth;
            case NoteDuration::Eighth:       return NoteDuration::Quarter;
            case NoteDuration::Quarter:      return NoteDuration::Half;
            case NoteDuration::Half:         return NoteDuration::Whole;
            default: return d;
        }
    }
    
    static NoteDuration getNextShorterDuration(NoteDuration d)
    {
        switch (d)
        {
            case NoteDuration::Whole:        return NoteDuration::Half;
            case NoteDuration::Half:         return NoteDuration::Quarter;
            case NoteDuration::Quarter:      return NoteDuration::Eighth;
            case NoteDuration::Eighth:       return NoteDuration::Sixteenth;
            case NoteDuration::Sixteenth:    return NoteDuration::ThirtySecond;
            default: return d;
        }
    }
    
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
 * RestEditPopup - Popup zur Bearbeitung von Pausen (Dauer ändern, löschen).
 */
class RestEditPopup : public juce::Component
{
public:
    RestEditPopup() { setSize(240, 130); setAlwaysOnTop(true); }
    
    void showForRest(const RenderedRestInfo& restInfo, juce::Component* parent)
    {
        currentRestInfo = restInfo;
        hoveredItem = HoverItem::None;
        
        // Get current beat duration info
        beatDuration = restInfo.duration;
        beatDotted = restInfo.isDotted;
        
        int headerHeight = 35;
        int durationSectionHeight = 65;  // Duration selector section
        int deleteSectionHeight = 36;
        int width = 240;
        int height = headerHeight + durationSectionHeight + deleteSectionHeight + 20;
        
        setSize(width, height);
        
        if (parent != nullptr)
        {
            int popupX = static_cast<int>(restInfo.bounds.getRight()) + 15;
            if (popupX + width > parent->getWidth() - 5)
                popupX = static_cast<int>(restInfo.bounds.getX()) - width - 15;
            popupX = juce::jlimit(5, parent->getWidth() - width - 5, popupX);
            
            int popupY = static_cast<int>(restInfo.bounds.getCentreY()) - height / 2;
            popupY = juce::jlimit(5, parent->getHeight() - height - 5, popupY);
            
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
        currentRestInfo = RenderedRestInfo();
    }
    
    bool isShowing() const { return currentRestInfo.measureIndex >= 0; }
    
    /** Callback: rest deletion requested (measureIndex, beatIndex) */
    std::function<void(int, int)> onRestDeleteRequested;
    
    /** Callback: rest duration changed (measureIndex, beatIndex, newDuration, isDotted) */
    std::function<void(int, int, NoteDuration, bool)> onRestDurationChangeRequested;
    
    void paint(juce::Graphics& g) override
    {
        // Background
        g.setColour(juce::Colour(0xFF2D2D30));
        g.fillRoundedRectangle(getLocalBounds().toFloat(), 8.0f);
        g.setColour(juce::Colour(0xFFD9904A));
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1), 8.0f, 2.0f);
        
        // Header
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(juce::FontOptions(14.0f)).boldened());
        juce::String durName = getDurationName(beatDuration, beatDotted);
        g.drawText(juce::String("Edit Pause: ") + durName, 
                   getLocalBounds().removeFromTop(30).reduced(10, 5),
                   juce::Justification::centredLeft, true);
        
        g.setColour(juce::Colour(0xFF555555));
        g.drawHorizontalLine(33, 10, getWidth() - 10);
        
        // Duration section
        int y = 38;
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(juce::FontOptions(11.0f)));
        g.drawText("Duration:", 10, y, 60, 18, juce::Justification::centredLeft, false);
        y += 20;
        
        struct DurInfo { NoteDuration dur; const char* label; };
        DurInfo durations[] = {
            { NoteDuration::Whole, "W" },
            { NoteDuration::Half, "H" },
            { NoteDuration::Quarter, "Q" },
            { NoteDuration::Eighth, "8" },
            { NoteDuration::Sixteenth, "16" },
            { NoteDuration::ThirtySecond, "32" }
        };
        
        int btnWidth = 28, btnSpacing = 3;
        int totalBtnsWidth = 6 * btnWidth + 5 * btnSpacing;
        int dotBtnWidth = 22;
        int dotGap = 6;
        int totalRowWidth = totalBtnsWidth + dotGap + dotBtnWidth;
        int btnX = (getWidth() - totalRowWidth) / 2;
        
        for (int i = 0; i < 6; ++i)
        {
            juce::Rectangle<int> btnRect(btnX + i * (btnWidth + btnSpacing), y, btnWidth, 24);
            bool isCurrent = (durations[i].dur == beatDuration && !beatDotted);
            bool isHover = (hoveredItem == HoverItem::Duration && durationHoveredIdx == i);
            
            if (isCurrent)
            {
                g.setColour(juce::Colour(0xFFD9904A));
                g.fillRoundedRectangle(btnRect.toFloat(), 3.0f);
            }
            else if (isHover)
            {
                g.setColour(juce::Colour(0xFFD9904A).withAlpha(0.4f));
                g.fillRoundedRectangle(btnRect.toFloat(), 3.0f);
            }
            else
            {
                g.setColour(juce::Colour(0xFF444448));
                g.fillRoundedRectangle(btnRect.toFloat(), 3.0f);
            }
            
            g.setColour(juce::Colours::white);
            g.setFont(juce::Font(juce::FontOptions(10.0f)).boldened());
            g.drawText(durations[i].label, btnRect, juce::Justification::centred, false);
            durationBtnRects[i] = btnRect;
        }
        
        // Dot toggle
        int dotBtnX = btnX + totalBtnsWidth + 6;
        juce::Rectangle<int> dotRect(dotBtnX, y, 22, 24);
        bool dotCurrent = beatDotted;
        bool dotHover = (hoveredItem == HoverItem::Duration && durationHoveredIdx == 6);
        
        if (dotCurrent)
        {
            g.setColour(juce::Colour(0xFFD9904A));
            g.fillRoundedRectangle(dotRect.toFloat(), 3.0f);
        }
        else if (dotHover)
        {
            g.setColour(juce::Colour(0xFFD9904A).withAlpha(0.4f));
            g.fillRoundedRectangle(dotRect.toFloat(), 3.0f);
        }
        else
        {
            g.setColour(juce::Colour(0xFF444448));
            g.fillRoundedRectangle(dotRect.toFloat(), 3.0f);
        }
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(juce::FontOptions(14.0f)).boldened());
        g.drawText(".", dotRect, juce::Justification::centred, false);
        dotBtnRect = dotRect;
        
        y += 30;
        
        // Separator
        g.setColour(juce::Colour(0xFF555555));
        g.drawHorizontalLine(y, 10, getWidth() - 10);
        y += 6;
        
        // Delete button
        juce::Rectangle<int> delRect(10, y, getWidth() - 20, 28);
        deleteBtnRect = delRect;
        bool delHover = (hoveredItem == HoverItem::Delete);
        
        if (delHover)
        {
            g.setColour(juce::Colour(0xFFCC3333));
            g.fillRoundedRectangle(delRect.toFloat(), 4.0f);
        }
        else
        {
            g.setColour(juce::Colour(0xFF993333).withAlpha(0.6f));
            g.fillRoundedRectangle(delRect.toFloat(), 4.0f);
        }
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(juce::FontOptions(12.0f)).boldened());
        g.drawText(juce::CharPointer_UTF8("\xf0\x9f\x97\x91 Delete Rest  (Del)"), delRect, juce::Justification::centred, false);
    }
    
    void mouseMove(const juce::MouseEvent& event) override
    {
        HoverItem newHover = HoverItem::None;
        int newDurIdx = -1;
        
        // Check duration buttons
        for (int i = 0; i < 6; ++i)
        {
            if (durationBtnRects[i].contains(event.getPosition()))
            {
                newHover = HoverItem::Duration;
                newDurIdx = i;
                break;
            }
        }
        if (dotBtnRect.contains(event.getPosition()))
        {
            newHover = HoverItem::Duration;
            newDurIdx = 6;
        }
        
        // Check delete
        if (deleteBtnRect.contains(event.getPosition()))
            newHover = HoverItem::Delete;
        
        if (newHover != hoveredItem || newDurIdx != durationHoveredIdx)
        {
            hoveredItem = newHover;
            durationHoveredIdx = newDurIdx;
            repaint();
        }
    }
    
    void mouseDown(const juce::MouseEvent& event) override
    {
        // Delete button
        if (deleteBtnRect.contains(event.getPosition()))
        {
            int mi = currentRestInfo.measureIndex;
            int bi = currentRestInfo.beatIndex;
            hide();
            if (onRestDeleteRequested)
                onRestDeleteRequested(mi, bi);
            return;
        }
        
        // Duration buttons
        NoteDuration durations[] = { NoteDuration::Whole, NoteDuration::Half, NoteDuration::Quarter,
                                      NoteDuration::Eighth, NoteDuration::Sixteenth, NoteDuration::ThirtySecond };
        for (int i = 0; i < 6; ++i)
        {
            if (durationBtnRects[i].contains(event.getPosition()))
            {
                beatDuration = durations[i];
                beatDotted = false;
                if (onRestDurationChangeRequested)
                    onRestDurationChangeRequested(currentRestInfo.measureIndex, currentRestInfo.beatIndex, durations[i], false);
                repaint();
                return;
            }
        }
        
        // Dot toggle
        if (dotBtnRect.contains(event.getPosition()))
        {
            beatDotted = !beatDotted;
            if (onRestDurationChangeRequested)
                onRestDurationChangeRequested(currentRestInfo.measureIndex, currentRestInfo.beatIndex, beatDuration, beatDotted);
            repaint();
            return;
        }
        
        // Click outside items = close
        hide();
    }
    
    void mouseExit(const juce::MouseEvent&) override
    {
        if (hoveredItem != HoverItem::None)
        {
            hoveredItem = HoverItem::None;
            durationHoveredIdx = -1;
            repaint();
        }
    }
    
    void focusLost(FocusChangeType) override { hide(); }
    
    bool keyPressed(const juce::KeyPress& key) override
    {
        if (key == juce::KeyPress::escapeKey) { hide(); return true; }
        
        // Delete key
        if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
        {
            int mi = currentRestInfo.measureIndex;
            int bi = currentRestInfo.beatIndex;
            hide();
            if (onRestDeleteRequested)
                onRestDeleteRequested(mi, bi);
            return true;
        }
        
        // Duration keys 1-6
        NoteDuration durations[] = { NoteDuration::Whole, NoteDuration::Half, NoteDuration::Quarter,
                                      NoteDuration::Eighth, NoteDuration::Sixteenth, NoteDuration::ThirtySecond };
        for (int i = 0; i < 6; ++i)
        {
            if (key.getTextCharacter() == ('1' + i))
            {
                beatDuration = durations[i];
                beatDotted = false;
                if (onRestDurationChangeRequested)
                    onRestDurationChangeRequested(currentRestInfo.measureIndex, currentRestInfo.beatIndex, durations[i], false);
                repaint();
                return true;
            }
        }
        
        // Dot toggle
        if (key.getTextCharacter() == '.')
        {
            beatDotted = !beatDotted;
            if (onRestDurationChangeRequested)
                onRestDurationChangeRequested(currentRestInfo.measureIndex, currentRestInfo.beatIndex, beatDuration, beatDotted);
            repaint();
            return true;
        }
        
        return false;
    }
    
private:
    RenderedRestInfo currentRestInfo;
    NoteDuration beatDuration = NoteDuration::Quarter;
    bool beatDotted = false;
    
    enum class HoverItem { None, Duration, Delete };
    HoverItem hoveredItem = HoverItem::None;
    int durationHoveredIdx = -1;
    
    juce::Rectangle<int> durationBtnRects[6];
    juce::Rectangle<int> dotBtnRect;
    juce::Rectangle<int> deleteBtnRect;
    
    static juce::String getDurationName(NoteDuration d, bool dotted)
    {
        juce::String name;
        switch (d)
        {
            case NoteDuration::Whole:        name = "Whole"; break;
            case NoteDuration::Half:         name = "Half"; break;
            case NoteDuration::Quarter:      name = "Quarter"; break;
            case NoteDuration::Eighth:       name = "Eighth"; break;
            case NoteDuration::Sixteenth:    name = "16th"; break;
            case NoteDuration::ThirtySecond: name = "32nd"; break;
            default:                         name = "?"; break;
        }
        if (dotted) name += " dotted";
        return name;
    }
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RestEditPopup)
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
