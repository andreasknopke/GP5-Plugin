/*
  ==============================================================================

    TabViewComponent.h
    
    JUCE Component für die Tabulatur-Darstellung
    Mit Scrolling und Zoom-Funktionalität

  ==============================================================================
*/

#pragma once

#include "TabModels.h"
#include "TabRenderer.h"
#include "TabLayoutEngine.h"
#include "FretPositionCalculator.h"
#include "NoteEditComponent.h"
#include <juce_gui_basics/juce_gui_basics.h>

//==============================================================================
/**
 * TabViewComponent
 * 
 * Ein scrollbares, zoombares JUCE Component zur Anzeige von Tabulaturen.
 */
class TabViewComponent : public juce::Component,
                         public juce::ScrollBar::Listener
{
public:
    // Structure for live MIDI notes to display
    struct LiveNote {
        int string = 0;
        int fret = 0;
        int velocity = 100;
        int fingerNumber = -1;  // Finger (0=open, 1-4=finger, -1=unassigned)
    };
    
    // Set chord name to display above live notes
    void setLiveChordName(const juce::String& name)
    {
        liveChordName = name;
        repaint();
    }
    
    // Set overlay message (e.g. "Audio-to-MIDI recording..." or "Processing...")
    // Empty string = no overlay
    void setOverlayMessage(const juce::String& msg)
    {
        overlayMessage = msg;
        repaint();
    }
    
    TabViewComponent()
    {
        // Horizontal scrollbar
        addAndMakeVisible(horizontalScrollbar);
        horizontalScrollbar.setRangeLimits(0.0, 1.0);
        horizontalScrollbar.addListener(this);
        horizontalScrollbar.setAutoHide(false);
    }
    
    ~TabViewComponent() override
    {
        horizontalScrollbar.removeListener(this);
    }
    
    void setTrack(const TabTrack& newTrack)
    {
        track = newTrack;
        recalculateLayout();
        repaint();
    }
    
    const TabTrack& getTrack() const { return track; }
    
    // Set live MIDI notes to display (for editor mode)
    void setLiveNotes(const std::vector<LiveNote>& notes)
    {
        liveNotes = notes;
        repaint();
    }
    
    // Set which strings are muted (dead notes) in the current chord
    void setLiveMutedStrings(const std::array<bool, 6>& muted)
    {
        liveMutedStrings = muted;
    }
    
    // Enable/disable editor mode (show empty tab with live notes)
    void setEditorMode(bool enabled)
    {
        editorMode = enabled;
        repaint();
    }
    
    bool isEditorMode() const { return editorMode; }
    
    void setShowFingerNumbers(bool show)
    {
        config.showFingerNumbers = show;
        repaint();
    }
    
    bool getShowFingerNumbers() const { return config.showFingerNumbers; }
    
    void setZoom(float newZoom)
    {
        zoom = juce::jlimit(0.5f, 3.0f, newZoom);
        recalculateLayout();
        repaint();
    }
    
    float getZoom() const { return zoom; }
    
    void setHighlightMeasure(int measureIndex)
    {
        highlightedMeasure = measureIndex;
        
        // Auto-scroll to highlighted measure
        if (measureIndex >= 0 && measureIndex < track.measures.size())
        {
            float measureX = track.measures[measureIndex].xPosition;
            float measureWidth = track.measures[measureIndex].calculatedWidth;
            float viewWidth = getWidth() - 20.0f;
            
            // If measure is not fully visible, scroll to it
            if (measureX < scrollOffset || measureX + measureWidth > scrollOffset + viewWidth)
            {
                scrollOffset = juce::jmax(0.0f, measureX - viewWidth * 0.1f);
                updateScrollbar();
            }
        }
        
        repaint();
    }
    
    // Neue Methoden für DAW-Sync
    void setCurrentMeasure(int measureIndex)
    {
        currentPlayingMeasure = measureIndex;
        repaint();
    }
    
    void setPlayheadPosition(double positionInMeasure)
    {
        playheadPositionInMeasure = positionInMeasure;
        repaint();
    }
    
    // Setze die exakte Playhead-Position für smooth scrolling
    void setExactPlayheadPosition(int measureIndex, double positionInMeasure)
    {
        currentPlayingMeasure = measureIndex;
        playheadPositionInMeasure = positionInMeasure;
        repaint();
    }
    
    int getCurrentMeasure() const { return currentPlayingMeasure; }
    
    // Smooth Scroll: Playhead läuft zur Mitte, dann scrollt Content
    void updateSmoothScroll(int measureIndex, double positionInMeasure, bool forceUpdate = false)
    {
        if (measureIndex < 0 || measureIndex >= track.measures.size())
            return;
        
        currentPlayingMeasure = measureIndex;
        playheadPositionInMeasure = positionInMeasure;
        
        // Berechne die exakte X-Position des Playheads
        // xPosition/calculatedWidth sind bereits gezoomt (recalculateLayout verwendet scaledConfig)
        const auto& measure = track.measures[measureIndex];
        float measureX = measure.xPosition;
        float measureWidth = measure.calculatedWidth;
        float playheadX = measureX + static_cast<float>(positionInMeasure) * measureWidth;
        
        // Sichtbare Breite
        float viewWidth = static_cast<float>(getWidth()) - 20.0f;
        float centerX = viewWidth / 2.0f;
        
        // Ziel-Scroll-Position: Playhead soll in der Mitte sein
        float targetScroll = playheadX - centerX;
        
        // Am Anfang: Scroll bleibt bei 0, Playhead läuft bis zur Mitte
        // Danach: Scroll folgt dem Playhead, sodass er in der Mitte bleibt
        // Am Ende: Scroll stoppt, Playhead läuft weiter
        float maxScroll = juce::jmax(0.0f, totalWidth - viewWidth);
        targetScroll = juce::jlimit(0.0f, maxScroll, targetScroll);
        
        // Smooth interpolation
        if (forceUpdate)
        {
            // Bei manueller Positionänderung (z.B. zurück zum Anfang) sofort springen
            scrollOffset = targetScroll;
        }
        else
        {
            // Smooth scrolling mit Lerp
            float scrollSpeed = 0.15f;
            scrollOffset += (targetScroll - scrollOffset) * scrollSpeed;
        }
        
        updateScrollbar();
        repaint();
    }
    
    // Reset scroll position (z.B. bei Stop oder zurück zum Anfang)
    void resetScrollPosition()
    {
        scrollOffset = 0.0f;
        currentPlayingMeasure = 0;
        playheadPositionInMeasure = 0.0;
        lastPlayheadX = 0.0f;
        updateScrollbar();
        repaint();
    }
    
    void scrollToMeasure(int measureIndex)
    {
        if (measureIndex < 0 || measureIndex >= track.measures.size())
            return;
        
        // Diese Methode wird nicht mehr direkt verwendet,
        // stattdessen updateSmoothScroll benutzen für flüssiges Scrolling
        // xPosition/calculatedWidth sind bereits gezoomt (recalculateLayout verwendet scaledConfig)
        float measureX = track.measures[measureIndex].xPosition;
        float measureWidth = track.measures[measureIndex].calculatedWidth;
        float viewWidth = static_cast<float>(getWidth()) - 20.0f;
        
        // Scroll so dass der Playhead in der Mitte ist
        float targetScroll = measureX - viewWidth / 2.0f + measureWidth / 2.0f;
        scrollOffset = juce::jlimit(0.0f, juce::jmax(0.0f, totalWidth - viewWidth), targetScroll);
        updateScrollbar();
    }
    
    // Callback when a measure is clicked
    std::function<void(int)> onMeasureClicked;
    
    // Callback when a specific position is clicked (measure index, position within measure 0.0-1.0)
    std::function<void(int, double)> onPositionClicked;
    
    void paint(juce::Graphics& g) override
    {
        // Apply zoom to config
        TabLayoutConfig scaledConfig = config;
        scaledConfig.stringSpacing *= zoom;
        scaledConfig.fretFontSize *= zoom;
        scaledConfig.measurePadding *= zoom;
        scaledConfig.minBeatSpacing *= zoom;
        scaledConfig.baseNoteWidth *= zoom;
        
        // Calculate vertical centering
        float trackHeight = scaledConfig.getTotalHeight(track.stringCount);
        float availableHeight = static_cast<float>(getHeight()) - static_cast<float>(scrollbarHeight);
        float yOffset = (availableHeight - trackHeight) / 2.0f;
        yOffset = juce::jmax(0.0f, yOffset);
        
        // Set hidden notes for ghost preview (hide original notes when showing alternatives)
        if (ghostPreview.active && ghostPreview.ghostPos.string >= 0)
        {
            std::vector<std::tuple<int, int, int>> hidden;
            hidden.push_back(std::make_tuple(
                ghostPreview.originalNote.measureIndex,
                ghostPreview.originalNote.beatIndex,
                ghostPreview.originalNote.noteIndex));
            renderer.setHiddenNotes(hidden);
        }
        else if (groupGhostPreview.active && !groupGhostPreview.originalNotes.isEmpty())
        {
            std::vector<std::tuple<int, int, int>> hidden;
            for (const auto& note : groupGhostPreview.originalNotes)
            {
                hidden.push_back(std::make_tuple(note.measureIndex, note.beatIndex, note.noteIndex));
            }
            renderer.setHiddenNotes(hidden);
        }
        else
        {
            renderer.clearHiddenNotes();
        }
        
        // Draw track FIRST
        juce::Rectangle<float> renderBounds(0, yOffset, static_cast<float>(getWidth()), trackHeight);
        renderer.render(g, track, scaledConfig, renderBounds, scrollOffset, highlightedMeasure);
        
        // Draw current playing measure highlight AFTER track rendering (so it's visible on top)
        if (currentPlayingMeasure >= 0 && currentPlayingMeasure < track.measures.size())
        {
            const auto& measure = track.measures[currentPlayingMeasure];
            // xPosition und calculatedWidth sind bereits gezoomt (recalculateLayout verwendet scaledConfig)
            float measureX = 25.0f + measure.xPosition - scrollOffset;
            float measureWidth = measure.calculatedWidth;
            
            // Semi-transparent green overlay fuer aktuellen Takt
            g.setColour(juce::Colour(0x2000FF00));  // Transparentes Gruen
            g.fillRect(measureX, yOffset, measureWidth, trackHeight);
            
            // Playhead-Linie an der exakten Position innerhalb des Taktes
            float playheadX = measureX + static_cast<float>(playheadPositionInMeasure) * measureWidth;
            g.setColour(juce::Colours::limegreen);
            g.fillRect(playheadX - 1.0f, yOffset, 3.0f, trackHeight);
        }
        
        // Draw note hover highlight when note editing is enabled
        if (noteEditingEnabled)
        {
            // Draw hovered chord name highlight (orange glow + underline)
            if (hoveredChordInfo.measureIndex >= 0)
            {
                auto bounds = hoveredChordInfo.bounds;
                g.setColour(juce::Colours::orange.withAlpha(0.2f));
                g.fillRoundedRectangle(bounds.expanded(3.0f, 2.0f), 4.0f);
                g.setColour(juce::Colours::orange.withAlpha(0.8f));
                g.drawRoundedRectangle(bounds.expanded(2.0f, 1.0f), 4.0f, 1.5f);
                // Underline
                g.drawLine(bounds.getX(), bounds.getBottom() + 1.0f, 
                           bounds.getRight(), bounds.getBottom() + 1.0f, 1.5f);
            }
            
            // Draw hovered note highlight (cyan glow)
            if (hoveredNoteInfo.valid)
            {
                auto bounds = hoveredNoteInfo.noteBounds;
                g.setColour(juce::Colours::cyan.withAlpha(0.4f));
                g.fillRoundedRectangle(bounds.expanded(3.0f), 4.0f);
                g.setColour(juce::Colours::cyan);
                g.drawRoundedRectangle(bounds.expanded(2.0f), 4.0f, 2.0f);
            }
            
            // Draw hovered rest highlight (orange glow - indicates clickable)
            if (hoveredRestInfo.measureIndex >= 0)
            {
                auto bounds = hoveredRestInfo.bounds;
                g.setColour(juce::Colour(0xFFD9904A).withAlpha(0.25f));
                g.fillRoundedRectangle(bounds.expanded(4.0f), 5.0f);
                g.setColour(juce::Colour(0xFFD9904A).withAlpha(0.8f));
                g.drawRoundedRectangle(bounds.expanded(3.0f), 5.0f, 2.0f);
            }
            
            // Draw ghost preview for hovered alternative position
            if (ghostPreview.active && ghostPreview.ghostPos.string >= 0)
            {
                // Find the X position of the original note (same beat position)
                float ghostX = ghostPreview.originalNote.noteBounds.getCentreX();
                
                // Calculate Y position based on the alternative string
                float firstStringY = yOffset + scaledConfig.topMargin;
                float ghostY = firstStringY + ghostPreview.ghostPos.string * scaledConfig.stringSpacing;
                
                // Draw ghost note (semi-transparent)
                juce::String fretText = juce::String(ghostPreview.ghostPos.fret);
                float noteRadius = scaledConfig.stringSpacing * 0.45f;
                // Calculate width based on fret number digits
                float textWidth = scaledConfig.fretFontSize * (ghostPreview.ghostPos.fret >= 10 ? 1.4f : 0.9f) + 4.0f;
                float bgWidth = juce::jmax(noteRadius * 2.0f, textWidth);
                float bgHeight = noteRadius * 2.0f;
                
                // Ghost background (semi-transparent white)
                g.setColour(juce::Colours::white.withAlpha(0.7f));
                g.fillRoundedRectangle(ghostX - bgWidth / 2.0f, ghostY - bgHeight / 2.0f, bgWidth, bgHeight, 3.0f);
                
                // Ghost outline (cyan dashed effect)
                g.setColour(juce::Colours::cyan.withAlpha(0.8f));
                g.drawRoundedRectangle(ghostX - bgWidth / 2.0f - 1.0f, ghostY - bgHeight / 2.0f - 1.0f, 
                                       bgWidth + 2.0f, bgHeight + 2.0f, 4.0f, 2.0f);
                
                // Ghost fret text
                g.setColour(juce::Colours::darkgrey.withAlpha(0.9f));
                g.setFont(juce::Font(juce::FontOptions(scaledConfig.fretFontSize)));
                g.drawText(fretText, 
                           juce::Rectangle<float>(ghostX - bgWidth / 2.0f, ghostY - bgHeight / 2.0f, bgWidth, bgHeight),
                           juce::Justification::centred, false);
            }
            
            // Draw group ghost preview
            if (groupGhostPreview.active && !groupGhostPreview.originalNotes.isEmpty())
            {
                float firstStringY = yOffset + scaledConfig.topMargin;
                float noteRadius = scaledConfig.stringSpacing * 0.45f;
                
                for (int i = 0; i < groupGhostPreview.originalNotes.size() && 
                     i < groupGhostPreview.ghostPositions.positions.size(); ++i)
                {
                    const auto& origNote = groupGhostPreview.originalNotes[i];
                    const auto& ghostPos = groupGhostPreview.ghostPositions.positions[i];
                    
                    float ghostX = origNote.noteBounds.getCentreX();
                    float ghostY = firstStringY + ghostPos.string * scaledConfig.stringSpacing;
                    
                    juce::String fretText = juce::String(ghostPos.fret);
                    // Calculate width based on fret number digits
                    float textWidth = scaledConfig.fretFontSize * (ghostPos.fret >= 10 ? 1.4f : 0.9f) + 4.0f;
                    float bgWidth = juce::jmax(noteRadius * 2.0f, textWidth);
                    float bgHeight = noteRadius * 2.0f;
                    
                    // Ghost background
                    g.setColour(juce::Colours::white.withAlpha(0.7f));
                    g.fillRoundedRectangle(ghostX - bgWidth / 2.0f, ghostY - bgHeight / 2.0f, bgWidth, bgHeight, 3.0f);
                    
                    // Ghost outline (orange for group)
                    g.setColour(juce::Colours::orange.withAlpha(0.8f));
                    g.drawRoundedRectangle(ghostX - bgWidth / 2.0f - 1.0f, ghostY - bgHeight / 2.0f - 1.0f, 
                                           bgWidth + 2.0f, bgHeight + 2.0f, 4.0f, 2.0f);
                    
                    // Ghost fret text
                    g.setColour(juce::Colours::darkgrey.withAlpha(0.9f));
                    g.setFont(juce::Font(juce::FontOptions(scaledConfig.fretFontSize)));
                    g.drawText(fretText, 
                               juce::Rectangle<float>(ghostX - bgWidth / 2.0f, ghostY - bgHeight / 2.0f, bgWidth, bgHeight),
                               juce::Justification::centred, false);
                }
            }
            
            // Draw selected notes highlight (yellow/orange)
            for (const auto& note : selectedNotes)
            {
                auto bounds = note.noteBounds;
                g.setColour(juce::Colours::yellow.withAlpha(0.4f));
                g.fillRoundedRectangle(bounds.expanded(4.0f), 5.0f);
                g.setColour(juce::Colours::orange);
                g.drawRoundedRectangle(bounds.expanded(3.0f), 5.0f, 2.0f);
            }
            
            // Draw selection rectangle while dragging
            if (isDragSelecting && selectionRect.getWidth() > 2 && selectionRect.getHeight() > 2)
            {
                g.setColour(juce::Colours::cyan.withAlpha(0.15f));
                g.fillRect(selectionRect);
                g.setColour(juce::Colours::cyan.withAlpha(0.8f));
                float dashLengths[] = { 4.0f, 4.0f };
                g.drawDashedLine(juce::Line<float>(selectionRect.getTopLeft(), selectionRect.getTopRight()), dashLengths, 2);
                g.drawDashedLine(juce::Line<float>(selectionRect.getTopRight(), selectionRect.getBottomRight()), dashLengths, 2);
                g.drawDashedLine(juce::Line<float>(selectionRect.getBottomRight(), selectionRect.getBottomLeft()), dashLengths, 2);
                g.drawDashedLine(juce::Line<float>(selectionRect.getBottomLeft(), selectionRect.getTopLeft()), dashLengths, 2);
            }
            
            // Draw beat cursor (highlight the currently navigated beat)
            if (lastSelectedNote.measureIndex >= 0 && lastSelectedNote.measureIndex < track.measures.size()
                && !noteEditPopup.isShowing() && !fretInputPopup.isShowing())
            {
                const auto& curMeasure = track.measures[lastSelectedNote.measureIndex];
                if (lastSelectedNote.beatIndex >= 0 && lastSelectedNote.beatIndex < curMeasure.beats.size())
                {
                    // Find the X position of this beat from rendered notes or rests
                    float beatCenterX = -1.0f;
                    
                    if (lastSelectedNote.valid)
                    {
                        // Have a note - use its bounds
                        beatCenterX = lastSelectedNote.noteBounds.getCentreX();
                    }
                    else
                    {
                        // Rest - find from rendered rests
                        for (const auto& ri : renderer.getRenderedRests())
                        {
                            if (ri.measureIndex == lastSelectedNote.measureIndex && ri.beatIndex == lastSelectedNote.beatIndex)
                            {
                                beatCenterX = ri.bounds.getCentreX();
                                break;
                            }
                        }
                    }
                    
                    if (beatCenterX > 0.0f)
                    {
                        float firstStringY = yOffset + scaledConfig.topMargin;
                        float cursorHeight = (track.stringCount - 1) * scaledConfig.stringSpacing + 8.0f;
                        
                        // Subtle column highlight
                        g.setColour(juce::Colour(0x184A90D9));
                        g.fillRect(beatCenterX - 12.0f, firstStringY - 4.0f, 24.0f, cursorHeight);
                        
                        // Top/bottom cursor marks
                        g.setColour(juce::Colour(0xFF4A90D9));
                        g.fillRect(beatCenterX - 6.0f, firstStringY - 5.0f, 12.0f, 2.0f);
                        g.fillRect(beatCenterX - 6.0f, firstStringY + cursorHeight - 5.0f, 12.0f, 2.0f);
                    }
                }
            }
        }
        
        // Draw live MIDI notes (editor mode)
        if (!liveNotes.empty())
        {
            // Position for live notes: center of the visible area
            float centerX = static_cast<float>(getWidth()) / 2.0f;
            float firstStringY = yOffset + scaledConfig.topMargin;
            
            // Draw a vertical line to indicate current input position
            g.setColour(juce::Colour(0x40FF6600));  // Transparent orange
            g.fillRect(centerX - 30.0f, yOffset, 60.0f, trackHeight);
            
            // Draw each live note
            for (const auto& note : liveNotes)
            {
                if (note.string >= 0 && note.string < 6)
                {
                    float stringY = firstStringY + note.string * scaledConfig.stringSpacing;
                    
                    // Draw highlighted fret number
                    juce::String fretText = juce::String(note.fret);
                    
                    // Background box
                    float textWidth = scaledConfig.fretFontSize * (note.fret >= 10 ? 1.4f : 0.9f);
                    g.setColour(juce::Colours::orange);
                    g.fillRoundedRectangle(centerX - textWidth/2 - 3, stringY - scaledConfig.fretFontSize/2 - 2, 
                                           textWidth + 6, scaledConfig.fretFontSize + 4, 3.0f);
                    
                    // Fret number text
                    g.setColour(juce::Colours::white);
                    g.setFont(juce::Font(juce::FontOptions(scaledConfig.fretFontSize)).boldened());
                    g.drawText(fretText, 
                               static_cast<int>(centerX - textWidth/2 - 3), 
                               static_cast<int>(stringY - scaledConfig.fretFontSize/2 - 2),
                               static_cast<int>(textWidth + 6), 
                               static_cast<int>(scaledConfig.fretFontSize + 4),
                               juce::Justification::centred, false);
                    
                    // Draw finger number to the right of the fret box (when enabled)
                    if (note.fingerNumber >= 1 && note.fingerNumber <= 4)
                    {
                        float fingerFontSize = scaledConfig.fretFontSize;
                        float boxW = fingerFontSize + 4.0f;
                        float boxH = fingerFontSize + 4.0f;
                        float fingerX = centerX + textWidth / 2 + 8.0f;  // Right of the orange box with gap
                        float fingerY = stringY - boxH / 2.0f;
                        
                        // Green rounded rectangle background
                        g.setColour(juce::Colour(0xFF00AA44));
                        g.fillRoundedRectangle(fingerX, fingerY, boxW, boxH, 4.0f);
                        
                        // White border
                        g.setColour(juce::Colours::white);
                        g.drawRoundedRectangle(fingerX, fingerY, boxW, boxH, 4.0f, 1.5f);
                        
                        // White finger number text (same size as fret)
                        g.setFont(juce::Font(juce::FontOptions(fingerFontSize)).boldened());
                        g.drawText(juce::String(note.fingerNumber),
                                   static_cast<int>(fingerX),
                                   static_cast<int>(fingerY),
                                   static_cast<int>(boxW),
                                   static_cast<int>(boxH),
                                   juce::Justification::centred, false);
                    }
                }
            }
            
            // Draw muted string indicators (X) for dead notes
            for (int s = 0; s < 6; ++s)
            {
                if (liveMutedStrings[s])
                {
                    float stringY = firstStringY + s * scaledConfig.stringSpacing;
                    float xSize = scaledConfig.fretFontSize * 0.7f;
                    
                    // Draw red X with semi-transparent background
                    g.setColour(juce::Colour(0x60FF0000));
                    g.fillRoundedRectangle(centerX - xSize/2 - 2, stringY - xSize/2 - 2, 
                                           xSize + 4, xSize + 4, 3.0f);
                    g.setColour(juce::Colour(0xFFFF3333));  // Red
                    g.setFont(juce::Font(juce::FontOptions(scaledConfig.fretFontSize * 0.85f)).boldened());
                    g.drawText("X",
                               static_cast<int>(centerX - xSize/2 - 2),
                               static_cast<int>(stringY - xSize/2 - 2),
                               static_cast<int>(xSize + 4),
                               static_cast<int>(xSize + 4),
                               juce::Justification::centred, false);
                }
            }
            
            // Draw chord name above the live notes (like GP5 chord names)
            if (liveChordName.isNotEmpty())
            {
                float chordY = firstStringY - 35.0f;  // Position above first string
                
                // Background for chord name
                g.setColour(juce::Colour(0xFF2D5A1E));  // Dark green background
                float chordWidth = liveChordName.length() * 10.0f + 16.0f;
                g.fillRoundedRectangle(centerX - chordWidth/2, chordY - 8, chordWidth, 22.0f, 4.0f);
                
                // Chord name text
                g.setColour(juce::Colours::lightgreen);
                g.setFont(juce::Font(juce::FontOptions(14.0f)).boldened());
                g.drawText(liveChordName, 
                           static_cast<int>(centerX - chordWidth/2), 
                           static_cast<int>(chordY - 8),
                           static_cast<int>(chordWidth), 
                           22,
                           juce::Justification::centred, false);
            }
        }
        
        // Draw overlay message (Audio-to-MIDI recording / processing)
        if (overlayMessage.isNotEmpty())
        {
            float overlayHeight = availableHeight;
            
            // Semi-transparent dark background over entire TAB area
            g.setColour(juce::Colour(0xCC1A1A2E));  // Dark blue-black, 80% opaque
            g.fillRect(0.0f, 0.0f, static_cast<float>(getWidth()), overlayHeight);
            
            // Centered message box
            float boxWidth = juce::jmin(400.0f, static_cast<float>(getWidth()) - 40.0f);
            float boxHeight = 80.0f;
            float boxX = (static_cast<float>(getWidth()) - boxWidth) / 2.0f;
            float boxY = (overlayHeight - boxHeight) / 2.0f;
            
            // Rounded box background
            g.setColour(juce::Colour(0xFF2D2D44));
            g.fillRoundedRectangle(boxX, boxY, boxWidth, boxHeight, 12.0f);
            g.setColour(juce::Colour(0xFF5588FF));
            g.drawRoundedRectangle(boxX, boxY, boxWidth, boxHeight, 12.0f, 2.0f);
            
            // Message text
            g.setColour(juce::Colours::white);
            g.setFont(juce::Font(juce::FontOptions(18.0f)).boldened());
            g.drawText(overlayMessage, 
                       static_cast<int>(boxX), static_cast<int>(boxY),
                       static_cast<int>(boxWidth), static_cast<int>(boxHeight),
                       juce::Justification::centred, false);
        }
    }
    
    void resized() override
    {
        horizontalScrollbar.setBounds(0, getHeight() - scrollbarHeight, getWidth(), scrollbarHeight);
        recalculateLayout();
    }
    
    void mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override
    {
        // Horizontal scroll with mouse wheel
        if (wheel.deltaX != 0 || (wheel.deltaY != 0 && !event.mods.isCtrlDown()))
        {
            float delta = (wheel.deltaX != 0 ? wheel.deltaX : wheel.deltaY) * 50.0f;
            scrollOffset = juce::jlimit(0.0f, juce::jmax(0.0f, totalWidth - getWidth()), scrollOffset - delta);
            updateScrollbar();
            repaint();
        }
        
        // Zoom with Ctrl + mouse wheel
        if (event.mods.isCtrlDown() && wheel.deltaY != 0)
        {
            setZoom(zoom + wheel.deltaY * 0.1f);
        }
    }
    
    void mouseMove(const juce::MouseEvent& event) override
    {
        if (noteEditingEnabled)
        {
            // Prüfe zuerst ob ein Akkordname gehovert wird
            auto chordHover = findChordAtPosition(event.position);
            if (chordHover.measureIndex >= 0)
            {
                hoveredChordInfo = chordHover;
                hoveredNoteInfo = NoteHitInfo();  // Reset note hover
                hoveredRestInfo = RenderedRestInfo(); // Reset rest hover
                setMouseCursor(juce::MouseCursor::PointingHandCursor);
                repaint();
                return;
            }
            else if (hoveredChordInfo.measureIndex >= 0)
            {
                hoveredChordInfo = RenderedChordInfo();
                setMouseCursor(juce::MouseCursor::NormalCursor);
                repaint();
            }
            
            auto newHovered = findNoteAtPosition(event.position);
            if (newHovered.valid)
            {
                if (newHovered.measureIndex != hoveredNoteInfo.measureIndex ||
                    newHovered.beatIndex != hoveredNoteInfo.beatIndex ||
                    newHovered.noteIndex != hoveredNoteInfo.noteIndex)
                {
                    hoveredNoteInfo = newHovered;
                    hoveredRestInfo = RenderedRestInfo(); // Reset rest hover
                    setMouseCursor(juce::MouseCursor::PointingHandCursor);
                    repaint();
                }
                return;
            }
            
            // Check if a rest is hovered
            auto restHover = findRestAtPosition(event.position);
            if (restHover.measureIndex >= 0)
            {
                hoveredNoteInfo = NoteHitInfo();
                if (restHover.measureIndex != hoveredRestInfo.measureIndex ||
                    restHover.beatIndex != hoveredRestInfo.beatIndex)
                {
                    hoveredRestInfo = restHover;
                }
                
                // Check if hovering over a string line (for insert cursor)
                int hoveredString = findStringAtPosition(event.position);
                if (hoveredString >= 0)
                    setMouseCursor(juce::MouseCursor::CrosshairCursor);
                else
                    setMouseCursor(juce::MouseCursor::PointingHandCursor);
                
                repaint();
                return;
            }
            
            // Nothing hovered - clear all
            if (hoveredNoteInfo.valid || hoveredRestInfo.measureIndex >= 0)
            {
                hoveredNoteInfo = NoteHitInfo();
                hoveredRestInfo = RenderedRestInfo();
                setMouseCursor(juce::MouseCursor::NormalCursor);
                repaint();
            }
        }
    }
    
    void mouseExit(const juce::MouseEvent&) override
    {
        if (hoveredNoteInfo.valid || hoveredChordInfo.measureIndex >= 0 || hoveredRestInfo.measureIndex >= 0)
        {
            hoveredNoteInfo = NoteHitInfo();
            hoveredChordInfo = RenderedChordInfo();
            hoveredRestInfo = RenderedRestInfo();
            setMouseCursor(juce::MouseCursor::NormalCursor);
            repaint();
        }
    }
    
    void mouseDown(const juce::MouseEvent& event) override
    {
        // Schließe Popups falls offen
        if (noteEditPopup.isShowing())
            noteEditPopup.hide();
        if (groupEditPopup.isShowing())
            groupEditPopup.hide();
        if (restEditPopup.isShowing())
            restEditPopup.hide();
        if (fretInputPopup.isShowing())
            fretInputPopup.hide();
        
        // Prüfe zuerst ob Note-Editing aktiviert ist
        if (noteEditingEnabled)
        {
            // Check if a chord name was clicked first
            auto chordHit = findChordAtPosition(event.position);
            if (chordHit.measureIndex >= 0)
            {
                // Chord name clicked - collect all notes in chord span and show voicing alternatives
                selectedNotes.clear();
                showChordVoicingPopup(chordHit);
                return;
            }
            
            auto hitInfo = findNoteAtPosition(event.position);
            if (hitInfo.valid)
            {
                // Single note clicked - show single note popup
                selectedNotes.clear();
                lastSelectedNote = hitInfo;  // Remember for keyboard shortcuts
                showNoteEditPopup(hitInfo);
                return;
            }
            
            // Check if a rest was clicked
            auto restHit = findRestAtPosition(event.position);
            if (restHit.measureIndex >= 0)
            {
                // Check if user clicked on a specific string line
                int clickedString = findStringAtPosition(event.position);
                if (clickedString >= 0)
                {
                    // Show inline fret input on this string
                    showFretInputPopup(restHit, clickedString, event.position);
                }
                else
                {
                    // Show rest edit popup (clicked on rest symbol itself)
                    showRestEditPopup(restHit);
                }
                return;
            }
            else
            {
                // Start rectangle selection
                isDragSelecting = true;
                dragStartPoint = event.position;
                selectionRect = juce::Rectangle<float>(event.position, event.position);
                selectedNotes.clear();
                repaint();
                return;
            }
        }
        
        // Find clicked measure and position within it
        float clickX = event.position.x + scrollOffset - 25.0f;  // Account for clef offset
        
        for (int m = 0; m < track.measures.size(); ++m)
        {
            const auto& measure = track.measures[m];
            // xPosition/calculatedWidth sind bereits gezoomt
            float measureStart = measure.xPosition;
            float measureWidth = measure.calculatedWidth;
            float measureEnd = measureStart + measureWidth;
            
            if (clickX >= measureStart && clickX < measureEnd)
            {
                // Calculate position within measure (0.0 - 1.0)
                double positionInMeasure = (clickX - measureStart) / measureWidth;
                positionInMeasure = juce::jlimit(0.0, 1.0, positionInMeasure);
                
                // Update visual playhead immediately
                currentPlayingMeasure = m;
                playheadPositionInMeasure = positionInMeasure;
                repaint();
                
                // Fire callbacks
                if (onMeasureClicked)
                    onMeasureClicked(m);
                    
                if (onPositionClicked)
                    onPositionClicked(m, positionInMeasure);
                    
                break;
            }
        }
    }
    
    void mouseDrag(const juce::MouseEvent& event) override
    {
        if (noteEditingEnabled && isDragSelecting)
        {
            // Update selection rectangle
            float x1 = juce::jmin(dragStartPoint.x, event.position.x);
            float y1 = juce::jmin(dragStartPoint.y, event.position.y);
            float x2 = juce::jmax(dragStartPoint.x, event.position.x);
            float y2 = juce::jmax(dragStartPoint.y, event.position.y);
            selectionRect = juce::Rectangle<float>(x1, y1, x2 - x1, y2 - y1);
            
            // Find notes within selection rectangle
            selectedNotes.clear();
            for (const auto& noteInfo : renderer.getRenderedNotes())
            {
                if (selectionRect.intersects(noteInfo.bounds))
                {
                    NoteHitInfo hitInfo;
                    hitInfo.valid = true;
                    hitInfo.measureIndex = noteInfo.measureIndex;
                    hitInfo.beatIndex = noteInfo.beatIndex;
                    hitInfo.noteIndex = noteInfo.noteIndex;
                    hitInfo.stringIndex = noteInfo.stringIndex;
                    hitInfo.fret = noteInfo.fret;
                    hitInfo.midiNote = noteInfo.midiNote;
                    hitInfo.noteBounds = noteInfo.bounds;
                    selectedNotes.add(hitInfo);
                }
            }
            
            repaint();
        }
    }
    
    void mouseUp(const juce::MouseEvent& event) override
    {
        if (noteEditingEnabled && isDragSelecting)
        {
            isDragSelecting = false;
            
            // If we have multiple selected notes, show group edit popup
            if (selectedNotes.size() > 1)
            {
                showGroupEditPopup();
            }
            else if (selectedNotes.size() == 1)
            {
                // Single note selected via drag - show single note popup
                showNoteEditPopup(selectedNotes[0]);
                selectedNotes.clear();
            }
            else
            {
                // No notes selected - clear selection
                selectedNotes.clear();
            }
            
            selectionRect = juce::Rectangle<float>();
            repaint();
        }
    }
    
    void scrollBarMoved(juce::ScrollBar* scrollBarThatHasMoved, double newRangeStart) override
    {
        if (scrollBarThatHasMoved == &horizontalScrollbar)
        {
            scrollOffset = static_cast<float>(newRangeStart);
            repaint();
        }
    }
    
    bool keyPressed(const juce::KeyPress& key) override
    {
        if (!noteEditingEnabled) return false;
        
        // If a popup is showing, let it handle the key
        if (noteEditPopup.isShowing() || groupEditPopup.isShowing() || restEditPopup.isShowing() || fretInputPopup.isShowing())
            return false;
        
        // Keyboard shortcuts only work when we have a lastSelectedNote
        if (!lastSelectedNote.valid) return false;
        
        // Delete key - delete selected note
        if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
        {
            deleteNoteAtSelection(lastSelectedNote);
            return true;
        }
        
        // Duration keys 1-6
        NoteDuration durations[] = { NoteDuration::Whole, NoteDuration::Half, NoteDuration::Quarter,
                                      NoteDuration::Eighth, NoteDuration::Sixteenth, NoteDuration::ThirtySecond };
        for (int i = 0; i < 6; ++i)
        {
            if (key.getTextCharacter() == ('1' + i))
            {
                changeBeatDuration(lastSelectedNote, durations[i], false);
                return true;
            }
        }
        
        // Dot toggle
        if (key.getTextCharacter() == '.')
        {
            if (lastSelectedNote.measureIndex >= 0 && lastSelectedNote.measureIndex < track.measures.size())
            {
                auto& measure = track.measures.getReference(lastSelectedNote.measureIndex);
                if (lastSelectedNote.beatIndex >= 0 && lastSelectedNote.beatIndex < measure.beats.size())
                {
                    auto& beat = measure.beats[lastSelectedNote.beatIndex];
                    changeBeatDuration(lastSelectedNote, beat.duration, !beat.isDotted);
                }
            }
            return true;
        }
        
        // +/- change duration
        if (key.getTextCharacter() == '+' || key.getTextCharacter() == '=')
        {
            if (lastSelectedNote.measureIndex >= 0 && lastSelectedNote.measureIndex < track.measures.size())
            {
                auto& measure = track.measures[lastSelectedNote.measureIndex];
                if (lastSelectedNote.beatIndex >= 0 && lastSelectedNote.beatIndex < measure.beats.size())
                {
                    auto& beat = measure.beats[lastSelectedNote.beatIndex];
                    auto newDur = getNextLongerDuration(beat.duration);
                    if (newDur != beat.duration)
                        changeBeatDuration(lastSelectedNote, newDur, false);
                }
            }
            return true;
        }
        if (key.getTextCharacter() == '-')
        {
            if (lastSelectedNote.measureIndex >= 0 && lastSelectedNote.measureIndex < track.measures.size())
            {
                auto& measure = track.measures[lastSelectedNote.measureIndex];
                if (lastSelectedNote.beatIndex >= 0 && lastSelectedNote.beatIndex < measure.beats.size())
                {
                    auto& beat = measure.beats[lastSelectedNote.beatIndex];
                    auto newDur = getNextShorterDuration(beat.duration);
                    if (newDur != beat.duration)
                        changeBeatDuration(lastSelectedNote, newDur, false);
                }
            }
            return true;
        }
        
        // Up/Down - change pitch (semitone), Shift+Up/Down - change pitch (octave)
        if (!key.getModifiers().isCtrlDown() && (key.isKeyCode(juce::KeyPress::upKey) || key.isKeyCode(juce::KeyPress::downKey)))
        {
            bool isShift = key.getModifiers().isShiftDown();
            int delta = key.isKeyCode(juce::KeyPress::upKey) ? (isShift ? 12 : 1) : (isShift ? -12 : -1);
            int newMidi = lastSelectedNote.midiNote + delta;
            if (newMidi >= 0 && newMidi <= 127)
            {
                changeNotePitch(lastSelectedNote, newMidi);
            }
            return true;
        }
        
        // Ctrl+Up/Down - move note to adjacent string
        if (key.getModifiers().isCtrlDown())
        {
            if (key.isKeyCode(juce::KeyPress::upKey))
            {
                moveNoteToAdjacentString(lastSelectedNote, -1);  // Up = lower string index
                return true;
            }
            if (key.isKeyCode(juce::KeyPress::downKey))
            {
                moveNoteToAdjacentString(lastSelectedNote, +1);  // Down = higher string index
                return true;
            }
        }
        
        // Left/Right arrow - navigate between beats
        if (key.isKeyCode(juce::KeyPress::leftKey))
        {
            navigateBeat(-1);
            return true;
        }
        if (key.isKeyCode(juce::KeyPress::rightKey))
        {
            navigateBeat(1);
            return true;
        }
        
        return false;
    }
    
    //==========================================================================
    // Note Editing API
    //==========================================================================
    
    void setNoteEditingEnabled(bool enabled)
    {
        noteEditingEnabled = enabled;
        if (!enabled)
        {
            if (noteEditPopup.isShowing())
                noteEditPopup.hide();
            if (groupEditPopup.isShowing())
                groupEditPopup.hide();
            if (restEditPopup.isShowing())
                restEditPopup.hide();
            if (fretInputPopup.isShowing())
                fretInputPopup.hide();
            hoveredNoteInfo = NoteHitInfo();
            hoveredChordInfo = RenderedChordInfo();
            hoveredRestInfo = RenderedRestInfo();
            ghostPreview.active = false;
            groupGhostPreview.active = false;
            selectedNotes.clear();
            lastSelectedNote = NoteHitInfo();
            isDragSelecting = false;
            selectionRect = juce::Rectangle<float>();
            setMouseCursor(juce::MouseCursor::NormalCursor);
        }
        else
        {
            setWantsKeyboardFocus(true);
            grabKeyboardFocus();
        }
        repaint();
    }
    
    bool isNoteEditingEnabled() const { return noteEditingEnabled; }
    
    /** Callback wenn eine Note-Position geändert wird: measureIdx, beatIdx, oldString, newString, newFret */
    std::function<void(int, int, int, int, int)> onNotePositionChanged;
    
    /** Callback wenn eine Note gelöscht wird: measureIdx, beatIdx, stringIndex */
    std::function<void(int, int, int)> onNoteDeleted;
    
    /** Callback wenn eine Beat-Dauer geändert wird: measureIdx, beatIdx, newDuration(int), isDotted */
    std::function<void(int, int, int, bool)> onBeatDurationChanged;
    
    /** Callback wenn die Tonhöhe geändert wird: measureIdx, beatIdx, stringIndex, newMidiNote, newFret */
    std::function<void(int, int, int, int, int)> onNotePitchChanged;
    
    /** Callback wenn eine Note in eine Pause eingefügt wird: measureIdx, beatIdx, stringIndex, fret, midiNote */
    std::function<void(int, int, int, int, int)> onNoteInserted;
    
    TabTrack& getTrackForEditing() { return track; }
    
private:
    TabTrack track;
    TabRenderer renderer;
    TabLayoutEngine layoutEngine;
    TabLayoutConfig config;
    FretPositionCalculator fretCalculator;
    
    float zoom = 1.0f;
    float scrollOffset = 0.0f;
    float totalWidth = 0.0f;
    int highlightedMeasure = -1;
    int currentPlayingMeasure = -1;
    double playheadPositionInMeasure = 0.0;
    float lastPlayheadX = 0.0f;
    
    // Editor mode (live MIDI input display)
    bool editorMode = false;
    std::vector<LiveNote> liveNotes;
    std::array<bool, 6> liveMutedStrings = { false, false, false, false, false, false };
    juce::String liveChordName;
    juce::String overlayMessage;  // Overlay-Nachricht (z.B. "Audio-to-MIDI recording...")
    
    // Note editing
    bool noteEditingEnabled = false;
    NoteEditPopup noteEditPopup;
    GroupNoteEditPopup groupEditPopup;
    RestEditPopup restEditPopup;
    FretInputPopup fretInputPopup;
    NoteHitInfo hoveredNoteInfo;   // Note unter dem Mauszeiger
    RenderedChordInfo hoveredChordInfo;  // Akkordname unter dem Mauszeiger
    RenderedRestInfo hoveredRestInfo;    // Pause unter dem Mauszeiger
    NoteDuration insertDuration = NoteDuration::Quarter;  // Default duration for note insertion
    
    // Rectangle selection for group editing
    bool isDragSelecting = false;
    juce::Point<float> dragStartPoint;
    juce::Rectangle<float> selectionRect;
    juce::Array<NoteHitInfo> selectedNotes;
    
    // Last selected note for keyboard shortcuts (even when popup is closed)
    NoteHitInfo lastSelectedNote;
    
    // Ghost preview for alternative note positions
    struct GhostNotePreview
    {
        bool active = false;
        NoteHitInfo originalNote;   // The note being edited
        AlternatePosition ghostPos; // The hovered alternative position
    };
    GhostNotePreview ghostPreview;
    
    // Ghost preview for group alternatives
    struct GroupGhostPreview
    {
        bool active = false;
        juce::Array<NoteHitInfo> originalNotes;
        FretPositionCalculator::GroupAlternative ghostPositions;
    };
    GroupGhostPreview groupGhostPreview;
    
    juce::ScrollBar horizontalScrollbar { false };
    const int scrollbarHeight = 14;
    
    NoteHitInfo findNoteAtPosition(juce::Point<float> pos)
    {
        NoteHitInfo hitInfo;
        for (const auto& noteInfo : renderer.getRenderedNotes())
        {
            if (noteInfo.bounds.contains(pos))
            {
                hitInfo.valid = true;
                hitInfo.measureIndex = noteInfo.measureIndex;
                hitInfo.beatIndex = noteInfo.beatIndex;
                hitInfo.noteIndex = noteInfo.noteIndex;
                hitInfo.stringIndex = noteInfo.stringIndex;
                hitInfo.fret = noteInfo.fret;
                hitInfo.midiNote = noteInfo.midiNote;
                hitInfo.noteBounds = noteInfo.bounds;
                
                // Hole Zeiger auf die echte Note
                if (hitInfo.measureIndex >= 0 && hitInfo.measureIndex < track.measures.size())
                {
                    auto& measure = track.measures.getReference(hitInfo.measureIndex);
                    if (hitInfo.beatIndex >= 0 && hitInfo.beatIndex < measure.beats.size())
                    {
                        auto& beat = measure.beats.getReference(hitInfo.beatIndex);
                        if (hitInfo.noteIndex >= 0 && hitInfo.noteIndex < beat.notes.size())
                            hitInfo.notePtr = &beat.notes.getReference(hitInfo.noteIndex);
                    }
                }
                
                // Berechne alternative Positionen
                if (hitInfo.midiNote >= 0)
                {
                    fretCalculator.setTuning(track.tuning);
                    hitInfo.alternatives = fretCalculator.calculateAlternatives(
                        hitInfo.midiNote, hitInfo.stringIndex, hitInfo.fret);
                }
                break;
            }
        }
        return hitInfo;
    }
    
    RenderedRestInfo findRestAtPosition(juce::Point<float> pos)
    {
        for (const auto& restInfo : renderer.getRenderedRests())
        {
            if (restInfo.bounds.contains(pos))
                return restInfo;
        }
        return RenderedRestInfo();
    }
    
    void showNoteEditPopup(const NoteHitInfo& hitInfo)
    {
        if (!hitInfo.valid) return;
        
        // Remember this note for keyboard shortcuts
        lastSelectedNote = hitInfo;
        
        noteEditPopup.onPositionSelected = [this](const NoteHitInfo& info, const AlternatePosition& newPos) {
            applyNotePositionChange(info, newPos);
        };
        
        // Callback for ghost preview when hovering over alternative positions
        noteEditPopup.onHoverPositionChanged = [this](const NoteHitInfo& info, const AlternatePosition& hoverPos) {
            if (hoverPos.string >= 0)
            {
                ghostPreview.active = true;
                ghostPreview.originalNote = info;
                ghostPreview.ghostPos = hoverPos;
            }
            else
            {
                ghostPreview.active = false;
            }
            repaint();
        };
        
        // Callback for note deletion
        noteEditPopup.onNoteDeleteRequested = [this](const NoteHitInfo& info) {
            deleteNoteAtSelection(info);
        };
        
        // Callback for duration change
        noteEditPopup.onDurationChangeRequested = [this](const NoteHitInfo& info, NoteDuration newDur, bool dotted) {
            changeBeatDuration(info, newDur, dotted);
        };
        
        // Callback for pitch change
        noteEditPopup.onNotePitchChanged = [this](const NoteHitInfo& info, int newMidiNote) {
            changeNotePitch(info, newMidiNote);
        };
        
        // Get current beat duration for the popup
        NoteDuration currentDur = NoteDuration::Quarter;
        bool currentDotted = false;
        if (hitInfo.measureIndex >= 0 && hitInfo.measureIndex < track.measures.size())
        {
            const auto& measure = track.measures[hitInfo.measureIndex];
            if (hitInfo.beatIndex >= 0 && hitInfo.beatIndex < measure.beats.size())
            {
                currentDur = measure.beats[hitInfo.beatIndex].duration;
                currentDotted = measure.beats[hitInfo.beatIndex].isDotted;
            }
        }
        
        noteEditPopup.showForNote(hitInfo, track.tuning, this, currentDur, currentDotted);
    }
    
    void showRestEditPopup(const RenderedRestInfo& restInfo)
    {
        if (restInfo.measureIndex < 0) return;
        
        // Callback for rest deletion
        restEditPopup.onRestDeleteRequested = [this](int measureIdx, int beatIdx) {
            deleteRestAndAdjust(measureIdx, beatIdx);
        };
        
        // Callback for rest duration change
        restEditPopup.onRestDurationChangeRequested = [this](int measureIdx, int beatIdx, NoteDuration newDur, bool dotted) {
            changeRestDuration(measureIdx, beatIdx, newDur, dotted);
        };
        
        restEditPopup.showForRest(restInfo, this);
    }
    
    void changeRestDuration(int measureIndex, int beatIndex, NoteDuration newDuration, bool isDotted)
    {
        if (measureIndex < 0 || measureIndex >= track.measures.size()) return;
        auto& measure = track.measures.getReference(measureIndex);
        if (beatIndex < 0 || beatIndex >= measure.beats.size()) return;
        
        auto& beat = measure.beats.getReference(beatIndex);
        if (!beat.isRest) return;
        
        float oldDurQ = beat.getDurationInQuarters();
        
        // Apply new duration
        beat.duration = newDuration;
        beat.isDotted = isDotted;
        beat.isDoubleDotted = false;
        
        float newDurQ = beat.getDurationInQuarters();
        float diff = newDurQ - oldDurQ;
        
        float measureCapacity = (float)measure.timeSignatureNumerator * (4.0f / (float)measure.timeSignatureDenominator);
        
        // Calculate total duration of all beats
        float totalDuration = 0.0f;
        for (const auto& b : measure.beats)
            totalDuration += b.getDurationInQuarters();
        
        if (totalDuration > measureCapacity + 0.001f)
        {
            // Need to shrink subsequent beats/rests
            float excess = totalDuration - measureCapacity;
            for (int b = beatIndex + 1; b < measure.beats.size() && excess > 0.001f; )
            {
                auto& nextBeat = measure.beats.getReference(b);
                float nextDur = nextBeat.getDurationInQuarters();
                
                if (nextDur <= excess + 0.001f)
                {
                    excess -= nextDur;
                    measure.beats.remove(b);
                }
                else
                {
                    auto shorterDur = findClosestDuration(nextDur - excess);
                    nextBeat.duration = shorterDur.first;
                    nextBeat.isDotted = shorterDur.second;
                    nextBeat.isDoubleDotted = false;
                    excess = 0.0f;
                    ++b;
                }
            }
            
            // Verify - if still overflowing, revert
            totalDuration = 0.0f;
            for (const auto& b : measure.beats)
                totalDuration += b.getDurationInQuarters();
            if (totalDuration > measureCapacity + 0.01f)
            {
                // Revert
                beat.duration = findClosestDuration(oldDurQ).first;
                beat.isDotted = findClosestDuration(oldDurQ).second;
                repaint();
                return;
            }
        }
        else if (totalDuration < measureCapacity - 0.001f)
        {
            // Fill gap with rest(s) after current beat
            float gap = measureCapacity - totalDuration;
            while (gap > 0.01f)
            {
                auto restDur = findClosestDuration(gap);
                TabBeat restBeat;
                restBeat.isRest = true;
                restBeat.duration = restDur.first;
                restBeat.isDotted = restDur.second;
                float restLen = restBeat.getDurationInQuarters();
                gap -= restLen;
                int insertPos = beatIndex + 1;
                if (insertPos > measure.beats.size()) insertPos = measure.beats.size();
                measure.beats.insert(insertPos, restBeat);
            }
        }
        
        // Notify processor
        if (onBeatDurationChanged)
            onBeatDurationChanged(measureIndex, beatIndex, static_cast<int>(newDuration), isDotted);
        
        recalculateLayout();
        repaint();
    }
    
    void applyNotePositionChange(const NoteHitInfo& info, const AlternatePosition& newPos)
    {
        if (info.measureIndex < 0 || info.measureIndex >= track.measures.size()) return;
        auto& measure = track.measures.getReference(info.measureIndex);
        if (info.beatIndex < 0 || info.beatIndex >= measure.beats.size()) return;
        auto& beat = measure.beats.getReference(info.beatIndex);
        if (info.noteIndex < 0 || info.noteIndex >= beat.notes.size()) return;
        
        auto& note = beat.notes.getReference(info.noteIndex);
        int oldString = note.string;  // Speichere alte Position VOR der Änderung
        note.string = newPos.string;
        note.fret = newPos.fret;
        note.isManuallyEdited = true;
        if (note.midiNote < 0) note.midiNote = info.midiNote;
        
        // Übergebe oldString statt noteIndex, damit recordedNotes die Note finden kann
        if (onNotePositionChanged)
            onNotePositionChanged(info.measureIndex, info.beatIndex, oldString, newPos.string, newPos.fret);
        
        repaint();
    }
    
    void showGroupEditPopup()
    {
        if (selectedNotes.isEmpty()) return;
        
        // Calculate group bounds for positioning
        juce::Rectangle<float> groupBounds;
        for (const auto& note : selectedNotes)
        {
            if (groupBounds.isEmpty())
                groupBounds = note.noteBounds;
            else
                groupBounds = groupBounds.getUnion(note.noteBounds);
        }
        
        // Build GroupNoteInfo array for the calculator
        juce::Array<FretPositionCalculator::GroupNoteInfo> groupNotes;
        for (const auto& note : selectedNotes)
        {
            FretPositionCalculator::GroupNoteInfo gni;
            gni.midiNote = note.midiNote;
            gni.currentString = note.stringIndex;
            gni.currentFret = note.fret;
            gni.measureIndex = note.measureIndex;
            gni.beatIndex = note.beatIndex;
            gni.noteIndex = note.noteIndex;
            groupNotes.add(gni);
        }
        
        // Calculate group alternatives
        fretCalculator.setTuning(track.tuning);
        auto alternatives = fretCalculator.calculateGroupAlternatives(groupNotes, 5);
        
        if (alternatives.isEmpty())
        {
            // No alternatives found - clear selection
            selectedNotes.clear();
            repaint();
            return;
        }
        
        // Setup callbacks
        groupEditPopup.onGroupSelected = [this](const juce::Array<NoteHitInfo>& notes, 
                                                 const FretPositionCalculator::GroupAlternative& alt) {
            applyGroupPositionChange(notes, alt);
        };
        
        groupEditPopup.onGroupHoverChanged = [this](const juce::Array<NoteHitInfo>& notes,
                                                     const FretPositionCalculator::GroupAlternative& alt,
                                                     bool active) {
            groupGhostPreview.active = active;
            if (active)
            {
                groupGhostPreview.originalNotes = notes;
                groupGhostPreview.ghostPositions = alt;
            }
            repaint();
        };
        
        groupEditPopup.showForGroup(selectedNotes, alternatives, track.tuning, this, groupBounds);
    }
    
    void applyGroupPositionChange(const juce::Array<NoteHitInfo>& notes, 
                                   const FretPositionCalculator::GroupAlternative& alt)
    {
        if (notes.size() != alt.positions.size()) return;
        
        for (int i = 0; i < notes.size(); ++i)
        {
            const auto& info = notes[i];
            const auto& newPos = alt.positions[i];
            
            if (info.measureIndex < 0 || info.measureIndex >= track.measures.size()) continue;
            auto& measure = track.measures.getReference(info.measureIndex);
            if (info.beatIndex < 0 || info.beatIndex >= measure.beats.size()) continue;
            auto& beat = measure.beats.getReference(info.beatIndex);
            if (info.noteIndex < 0 || info.noteIndex >= beat.notes.size()) continue;
            
            auto& note = beat.notes.getReference(info.noteIndex);
            int oldString = note.string;
            note.string = newPos.string;
            note.fret = newPos.fret;
            note.isManuallyEdited = true;
            if (note.midiNote < 0) note.midiNote = info.midiNote;
            
            if (onNotePositionChanged)
                onNotePositionChanged(info.measureIndex, info.beatIndex, oldString, newPos.string, newPos.fret);
        }
        
        selectedNotes.clear();
        groupGhostPreview.active = false;
        repaint();
    }
    
    //==========================================================================
    // Note Delete, Duration Change, String Move
    //==========================================================================
    
    void deleteNoteAtSelection(const NoteHitInfo& info)
    {
        if (info.measureIndex < 0 || info.measureIndex >= track.measures.size()) return;
        auto& measure = track.measures.getReference(info.measureIndex);
        if (info.beatIndex < 0 || info.beatIndex >= measure.beats.size()) return;
        auto& beat = measure.beats.getReference(info.beatIndex);
        if (info.noteIndex < 0 || info.noteIndex >= beat.notes.size()) return;
        
        int stringIndex = beat.notes[info.noteIndex].string;
        
        // Remove the note from the beat
        beat.notes.remove(info.noteIndex);
        
        // If beat has no more notes, make it a rest
        if (beat.notes.isEmpty())
            beat.isRest = true;
        
        // Clear selection
        lastSelectedNote = NoteHitInfo();
        
        // Notify processor
        if (onNoteDeleted)
            onNoteDeleted(info.measureIndex, info.beatIndex, stringIndex);
        
        recalculateLayout();
        repaint();
    }
    
    /** Lösche eine Pause und passe die benachbarten Beats an.
     *  - Wenn davor eine Note/Beat existiert → verlängere sie um die Pausendauer
     *  - Wenn die Pause am Taktanfang steht → verlängere die nächste Note/Beat
     *  - Wenn nur Pausen im Takt → verschmelze zu einer größeren Pause
     */
    void deleteRestAndAdjust(int measureIndex, int beatIndex)
    {
        if (measureIndex < 0 || measureIndex >= track.measures.size()) return;
        auto& measure = track.measures.getReference(measureIndex);
        if (beatIndex < 0 || beatIndex >= measure.beats.size()) return;
        auto& restBeat = measure.beats[beatIndex];
        if (!restBeat.isRest) return;  // Nur Pausen löschen
        
        float restDuration = restBeat.getDurationInQuarters();
        
        // Strategie 1: Beat davor verlängern (wenn vorhanden und kein Rest)
        if (beatIndex > 0)
        {
            auto& prevBeat = measure.beats.getReference(beatIndex - 1);
            float prevDuration = prevBeat.getDurationInQuarters();
            float combinedDuration = prevDuration + restDuration;
            
            // Finde die passende Standarddauer
            auto newDur = findClosestDuration(combinedDuration);
            float newDurQ = 4.0f / static_cast<float>(newDur.first);
            if (newDur.second) newDurQ *= 1.5f;
            
            // Prüfe ob die neue Dauer passt (Differenz < kleinstes Quantum)
            if (std::abs(newDurQ - combinedDuration) < 0.06f)
            {
                // Perfekte oder nahe Passung - verlängere den vorherigen Beat
                prevBeat.duration = newDur.first;
                prevBeat.isDotted = newDur.second;
                prevBeat.isDoubleDotted = false;
                measure.beats.remove(beatIndex);
            }
            else
            {
                // Nicht exakt passend - verlängere so weit wie möglich, 
                // Rest bleibt als kleinere Pause
                auto bestFit = findClosestDuration(combinedDuration);
                float bestFitQ = 4.0f / static_cast<float>(bestFit.first);
                if (bestFit.second) bestFitQ *= 1.5f;
                
                if (bestFitQ <= combinedDuration + 0.001f && bestFitQ > prevDuration + 0.001f)
                {
                    prevBeat.duration = bestFit.first;
                    prevBeat.isDotted = bestFit.second;
                    prevBeat.isDoubleDotted = false;
                    
                    float leftover = combinedDuration - bestFitQ;
                    if (leftover > 0.06f)
                    {
                        // Schrumpfe die Pause auf den Rest
                        auto leftoverDur = findClosestDuration(leftover);
                        auto& rest = measure.beats.getReference(beatIndex);
                        rest.duration = leftoverDur.first;
                        rest.isDotted = leftoverDur.second;
                    }
                    else
                    {
                        measure.beats.remove(beatIndex);
                    }
                }
                else
                {
                    // Kann nicht sinnvoll zusammengefügt werden - einfach entfernen
                    // und vorherigen Beat verlängern
                    prevBeat.duration = bestFit.first;
                    prevBeat.isDotted = bestFit.second;
                    prevBeat.isDoubleDotted = false;
                    measure.beats.remove(beatIndex);
                }
            }
        }
        // Strategie 2: Pause am Anfang - nächsten Beat verlängern
        else if (beatIndex == 0 && measure.beats.size() > 1)
        {
            auto& nextBeat = measure.beats.getReference(1);
            float nextDuration = nextBeat.getDurationInQuarters();
            float combinedDuration = nextDuration + restDuration;
            
            auto newDur = findClosestDuration(combinedDuration);
            float newDurQ = 4.0f / static_cast<float>(newDur.first);
            if (newDur.second) newDurQ *= 1.5f;
            
            if (std::abs(newDurQ - combinedDuration) < 0.06f)
            {
                nextBeat.duration = newDur.first;
                nextBeat.isDotted = newDur.second;
                nextBeat.isDoubleDotted = false;
                measure.beats.remove(0); // Entferne die Pause am Anfang
            }
            else
            {
                // Nicht exakt passend - verlängere den nächsten Beat so weit wie möglich
                auto bestFit = findClosestDuration(combinedDuration);
                float bestFitQ = 4.0f / static_cast<float>(bestFit.first);
                if (bestFit.second) bestFitQ *= 1.5f;
                
                if (bestFitQ <= combinedDuration + 0.001f && bestFitQ > nextDuration + 0.001f)
                {
                    nextBeat.duration = bestFit.first;
                    nextBeat.isDotted = bestFit.second;
                    nextBeat.isDoubleDotted = false;
                    
                    float leftover = combinedDuration - bestFitQ;
                    if (leftover > 0.06f)
                    {
                        auto leftoverDur = findClosestDuration(leftover);
                        auto& rest = measure.beats.getReference(0);
                        rest.duration = leftoverDur.first;
                        rest.isDotted = leftoverDur.second;
                    }
                    else
                    {
                        measure.beats.remove(0);
                    }
                }
                else
                {
                    nextBeat.duration = bestFit.first;
                    nextBeat.isDotted = bestFit.second;
                    nextBeat.isDoubleDotted = false;
                    measure.beats.remove(0);
                }
            }
        }
        // Strategie 3: Einziger Beat im Takt - nicht löschen (Takt braucht mindestens eine Pause)
        else
        {
            DBG("Cannot delete the only rest in measure " << (measureIndex + 1));
            return;
        }
        
        // Clear hover state
        hoveredRestInfo = RenderedRestInfo();
        
        // Speichere editierten Track
        if (onBeatDurationChanged)
            onBeatDurationChanged(measureIndex, -1, 0, false);  // beatIndex -1 = rest deletion
        
        recalculateLayout();
        repaint();
    }
    
    void changeBeatDuration(const NoteHitInfo& info, NoteDuration newDuration, bool isDotted)
    {
        if (info.measureIndex < 0 || info.measureIndex >= track.measures.size()) return;
        auto& measure = track.measures.getReference(info.measureIndex);
        if (info.beatIndex < 0 || info.beatIndex >= measure.beats.size()) return;
        
        auto& beat = measure.beats.getReference(info.beatIndex);
        float oldDurationInQuarters = beat.getDurationInQuarters();
        
        // Apply new duration
        NoteDuration oldDur = beat.duration;
        bool oldDotted = beat.isDotted;
        beat.duration = newDuration;
        beat.isDotted = isDotted;
        beat.isDoubleDotted = false;
        
        float newDurationInQuarters = beat.getDurationInQuarters();
        float diff = newDurationInQuarters - oldDurationInQuarters;
        
        // Calculate total measure duration capacity
        float measureCapacity = (float)measure.timeSignatureNumerator * (4.0f / (float)measure.timeSignatureDenominator);
        
        // Calculate current total duration of all beats
        float totalDuration = 0.0f;
        for (const auto& b : measure.beats)
            totalDuration += b.getDurationInQuarters();
        
        // Check if the new total exceeds measure capacity
        if (totalDuration > measureCapacity + 0.001f)
        {
            // Need to shrink subsequent beats or add rests
            float excess = totalDuration - measureCapacity;
            
            // Try to take time from beats after the current one
            for (int b = info.beatIndex + 1; b < measure.beats.size() && excess > 0.001f; )
            {
                auto& nextBeat = measure.beats.getReference(b);
                float nextDur = nextBeat.getDurationInQuarters();
                
                if (nextDur <= excess + 0.001f)
                {
                    // Remove this beat entirely
                    excess -= nextDur;
                    measure.beats.remove(b);
                    // Don't increment b since we removed
                }
                else
                {
                    // Shrink this beat
                    auto shorterDur = findClosestDuration(nextDur - excess);
                    nextBeat.duration = shorterDur.first;
                    nextBeat.isDotted = shorterDur.second;
                    nextBeat.isDoubleDotted = false;
                    excess = 0.0f;
                    ++b;
                }
            }
            
            // If there's still excess (can't fit), revert
            totalDuration = 0.0f;
            for (const auto& b : measure.beats)
                totalDuration += b.getDurationInQuarters();
            if (totalDuration > measureCapacity + 0.01f)
            {
                beat.duration = oldDur;
                beat.isDotted = oldDotted;
                repaint();
                return;
            }
        }
        else if (totalDuration < measureCapacity - 0.001f)
        {
            // Need to fill gap with a rest
            float gap = measureCapacity - totalDuration;
            
            // Insert rest beat(s) after current beat to fill the gap
            while (gap > 0.01f)
            {
                auto restDur = findClosestDuration(gap);
                TabBeat restBeat;
                restBeat.isRest = true;
                restBeat.duration = restDur.first;
                restBeat.isDotted = restDur.second;
                
                float restLen = restBeat.getDurationInQuarters();
                gap -= restLen;
                
                // Insert after current beat
                int insertPos = info.beatIndex + 1;
                if (insertPos > measure.beats.size()) insertPos = measure.beats.size();
                measure.beats.insert(insertPos, restBeat);
            }
        }
        
        // Notify processor
        if (onBeatDurationChanged)
            onBeatDurationChanged(info.measureIndex, info.beatIndex, static_cast<int>(newDuration), isDotted);
        
        recalculateLayout();
        repaint();
    }
    
    void changeNotePitch(const NoteHitInfo& info, int newMidiNote)
    {
        if (!info.valid || info.measureIndex < 0 || info.measureIndex >= track.measures.size()) return;
        auto& measure = track.measures.getReference(info.measureIndex);
        if (info.beatIndex < 0 || info.beatIndex >= measure.beats.size()) return;
        auto& beat = measure.beats.getReference(info.beatIndex);
        if (info.noteIndex < 0 || info.noteIndex >= beat.notes.size()) return;
        
        if (newMidiNote < 0 || newMidiNote > 127) return;
        
        auto& note = beat.notes.getReference(info.noteIndex);
        int currentString = note.string;
        
        // Try to keep the note on the same string if possible
        int newFret = -1;
        if (currentString >= 0 && currentString < track.tuning.size())
        {
            int fretOnSameString = newMidiNote - track.tuning[currentString];
            if (fretOnSameString >= 0 && fretOnSameString <= 24)
                newFret = fretOnSameString;
        }
        
        int targetString = currentString;
        
        // If not possible on the same string, find the best alternative position
        if (newFret < 0)
        {
            fretCalculator.setTuning(track.tuning);
            auto positions = fretCalculator.calculatePositions(newMidiNote);
            
            if (positions.isEmpty()) return;  // Cannot play this note on this instrument
            
            // Find best position that doesn't conflict with other notes in this beat
            for (const auto& pos : positions)
            {
                bool conflicting = false;
                for (int n = 0; n < beat.notes.size(); ++n)
                {
                    if (n != info.noteIndex && beat.notes[n].string == pos.string)
                    {
                        conflicting = true;
                        break;
                    }
                }
                if (!conflicting)
                {
                    targetString = pos.string;
                    newFret = pos.fret;
                    break;
                }
            }
            
            if (newFret < 0) return;  // No valid position found
        }
        
        int oldString = note.string;
        note.midiNote = newMidiNote;
        note.string = targetString;
        note.fret = newFret;
        note.isManuallyEdited = true;
        
        // Update lastSelectedNote to reflect the new state
        lastSelectedNote.midiNote = newMidiNote;
        lastSelectedNote.stringIndex = targetString;
        lastSelectedNote.fret = newFret;
        
        // Notify processor
        if (onNotePitchChanged)
            onNotePitchChanged(info.measureIndex, info.beatIndex, oldString, newMidiNote, newFret);
        
        recalculateLayout();
        repaint();
    }
    
    void moveNoteToAdjacentString(const NoteHitInfo& info, int direction)
    {
        if (!info.valid || info.midiNote < 0) return;
        
        int targetString = info.stringIndex + direction;
        if (targetString < 0 || targetString >= track.stringCount) return;
        
        // Calculate fret needed on the target string to produce the same MIDI note
        if (targetString >= track.tuning.size()) return;
        int targetFret = info.midiNote - track.tuning[targetString];
        
        // Check if the fret is playable (0-24)
        if (targetFret < 0 || targetFret > 24) return;
        
        // Check if the target string is free at this beat
        if (info.measureIndex >= 0 && info.measureIndex < track.measures.size())
        {
            auto& measure = track.measures.getReference(info.measureIndex);
            if (info.beatIndex >= 0 && info.beatIndex < measure.beats.size())
            {
                auto& beat = measure.beats.getReference(info.beatIndex);
                
                // Check for conflicting note on target string
                for (int n = 0; n < beat.notes.size(); ++n)
                {
                    if (n != info.noteIndex && beat.notes[n].string == targetString)
                        return;  // Target string is occupied
                }
            }
        }
        
        // Apply the change
        AlternatePosition newPos;
        newPos.string = targetString;
        newPos.fret = targetFret;
        applyNotePositionChange(info, newPos);
        
        // Update lastSelectedNote to reflect the new position
        lastSelectedNote.stringIndex = targetString;
        lastSelectedNote.fret = targetFret;
    }
    
    // Helper: find the closest standard duration to a given quarter-note length
    static std::pair<NoteDuration, bool> findClosestDuration(float quarters)
    {
        // Duration table: { quarters, duration, dotted }
        struct DurEntry { float q; NoteDuration d; bool dot; };
        DurEntry table[] = {
            { 4.0f,   NoteDuration::Whole, false },
            { 3.0f,   NoteDuration::Half, true },
            { 2.0f,   NoteDuration::Half, false },
            { 1.5f,   NoteDuration::Quarter, true },
            { 1.0f,   NoteDuration::Quarter, false },
            { 0.75f,  NoteDuration::Eighth, true },
            { 0.5f,   NoteDuration::Eighth, false },
            { 0.375f, NoteDuration::Sixteenth, true },
            { 0.25f,  NoteDuration::Sixteenth, false },
            { 0.125f, NoteDuration::ThirtySecond, false }
        };
        
        float bestDiff = 999.0f;
        int bestIdx = 4;  // Default to quarter
        for (int i = 0; i < 10; ++i)
        {
            if (table[i].q <= quarters + 0.001f)
            {
                float diff = quarters - table[i].q;
                if (diff < bestDiff)
                {
                    bestDiff = diff;
                    bestIdx = i;
                }
            }
        }
        return { table[bestIdx].d, table[bestIdx].dot };
    }
    
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
    
    //==========================================================================
    // Chord Voicing Feature
    //==========================================================================
    
    /** Findet einen Akkordnamen an der Mausposition */
    RenderedChordInfo findChordAtPosition(juce::Point<float> pos)
    {
        for (const auto& chordInfo : renderer.getRenderedChords())
        {
            if (chordInfo.bounds.contains(pos))
                return chordInfo;
        }
        return RenderedChordInfo();
    }
    
    /** Sammelt alle Noten vom Akkord-Beat bis zum nächsten Akkord (oder Taktende).
     *  Gibt NoteHitInfo-Array zurück für das Group-Edit-System.
     */
    juce::Array<NoteHitInfo> collectChordSpanNotes(const RenderedChordInfo& chordInfo)
    {
        juce::Array<NoteHitInfo> notes;
        
        if (chordInfo.measureIndex < 0 || chordInfo.measureIndex >= track.measures.size())
            return notes;
        
        const auto& measure = track.measures[chordInfo.measureIndex];
        
        // Sammle Noten ab dem Akkord-Beat bis zum nächsten Akkord (oder Taktende)
        for (int b = chordInfo.beatIndex; b < measure.beats.size(); ++b)
        {
            // Stoppe beim nächsten Akkord (aber nicht beim Start-Akkord selbst)
            if (b > chordInfo.beatIndex && measure.beats[b].chordName.isNotEmpty())
                break;
            
            const auto& beat = measure.beats[b];
            if (beat.isRest) continue;
            
            for (int n = 0; n < beat.notes.size(); ++n)
            {
                const auto& note = beat.notes[n];
                if (note.fret < 0) continue;  // Leere Slots überspringen
                
                NoteHitInfo hitInfo;
                hitInfo.valid = true;
                hitInfo.measureIndex = chordInfo.measureIndex;
                hitInfo.beatIndex = b;
                hitInfo.noteIndex = n;
                hitInfo.stringIndex = note.string;
                hitInfo.fret = note.fret;
                hitInfo.midiNote = note.midiNote;
                
                // Finde die gerenderten Bounds für diese Note
                for (const auto& rendered : renderer.getRenderedNotes())
                {
                    if (rendered.measureIndex == chordInfo.measureIndex &&
                        rendered.beatIndex == b &&
                        rendered.noteIndex == n)
                    {
                        hitInfo.noteBounds = rendered.bounds;
                        break;
                    }
                }
                
                // MIDI-Note berechnen falls nicht gesetzt
                if (hitInfo.midiNote < 0 && note.string < track.tuning.size())
                {
                    hitInfo.midiNote = track.tuning[note.string] + note.fret;
                }
                
                notes.add(hitInfo);
            }
        }
        
        return notes;
    }
    
    /** Zeigt das Voicing-Popup für einen angeklickten Akkord */
    void showChordVoicingPopup(const RenderedChordInfo& chordInfo)
    {
        // Sammle alle Noten im Akkord-Span
        auto chordNotes = collectChordSpanNotes(chordInfo);
        
        if (chordNotes.isEmpty())
            return;
        
        // Setze selectedNotes für die visuelle Markierung
        selectedNotes = chordNotes;
        
        // Berechne Gruppen-Alternativen
        juce::Array<FretPositionCalculator::GroupNoteInfo> groupNotes;
        for (const auto& note : chordNotes)
        {
            FretPositionCalculator::GroupNoteInfo gni;
            gni.midiNote = note.midiNote;
            gni.currentString = note.stringIndex;
            gni.currentFret = note.fret;
            gni.measureIndex = note.measureIndex;
            gni.beatIndex = note.beatIndex;
            gni.noteIndex = note.noteIndex;
            groupNotes.add(gni);
        }
        
        fretCalculator.setTuning(track.tuning);
        auto alternatives = fretCalculator.calculateGroupAlternatives(groupNotes, 8);
        
        if (alternatives.isEmpty())
        {
            selectedNotes.clear();
            repaint();
            return;
        }
        
        // Berechne Bounds für Popup-Positionierung (Bereich um den Akkordnamen)
        juce::Rectangle<float> groupBounds = chordInfo.bounds;
        for (const auto& note : chordNotes)
        {
            if (!note.noteBounds.isEmpty())
                groupBounds = groupBounds.getUnion(note.noteBounds);
        }
        
        // Setup Callbacks (wiederverwendet das Group-Edit-System)
        groupEditPopup.onGroupSelected = [this](const juce::Array<NoteHitInfo>& notes, 
                                                 const FretPositionCalculator::GroupAlternative& alt) {
            applyGroupPositionChange(notes, alt);
        };
        
        groupEditPopup.onGroupHoverChanged = [this](const juce::Array<NoteHitInfo>& notes,
                                                     const FretPositionCalculator::GroupAlternative& alt,
                                                     bool active) {
            groupGhostPreview.active = active;
            if (active)
            {
                groupGhostPreview.originalNotes = notes;
                groupGhostPreview.ghostPositions = alt;
            }
            repaint();
        };
        
        groupEditPopup.showForGroup(selectedNotes, alternatives, track.tuning, this, groupBounds);
    }
    
    /** Bestimme welche Saite an einer Y-Position liegt (für Insert-on-String) */
    int findStringAtPosition(juce::Point<float> pos)
    {
        TabLayoutConfig scaledConfig = config;
        scaledConfig.stringSpacing *= zoom;
        scaledConfig.topMargin *= zoom;
        
        float trackHeight = scaledConfig.getTotalHeight(track.stringCount);
        float availableHeight = static_cast<float>(getHeight()) - static_cast<float>(scrollbarHeight);
        float yOffset = juce::jmax(0.0f, (availableHeight - trackHeight) / 2.0f);
        float firstStringY = yOffset + scaledConfig.topMargin;
        
        float hitTolerance = scaledConfig.stringSpacing * 0.35f;
        
        for (int s = 0; s < track.stringCount; ++s)
        {
            float stringY = firstStringY + s * scaledConfig.stringSpacing;
            if (std::abs(pos.y - stringY) <= hitTolerance)
                return s;
        }
        return -1;  // Not on any string
    }
    
    /** Zeige das FretInput-Popup auf einer Saite über einer Pause */
    void showFretInputPopup(const RenderedRestInfo& restInfo, int stringIdx, juce::Point<float> clickPos)
    {
        // Wire the callback
        fretInputPopup.onNoteInsertRequested = [this](int measureIdx, int beatIdx, int stringIdx, int fret, int midiNote) {
            insertNoteAtRest(measureIdx, beatIdx, stringIdx, fret, midiNote);
        };
        
        fretInputPopup.showForInsert(restInfo, stringIdx, clickPos, this, track.tuning);
    }
    
    /** Füge eine Note in eine Pause ein. Die Pause wird in Note + Rest gesplittet. */
    void insertNoteAtRest(int measureIndex, int beatIndex, int stringIndex, int fret, int midiNote)
    {
        if (measureIndex < 0 || measureIndex >= track.measures.size()) return;
        auto& measure = track.measures.getReference(measureIndex);
        if (beatIndex < 0 || beatIndex >= measure.beats.size()) return;
        auto& beat = measure.beats.getReference(beatIndex);
        if (!beat.isRest) return;  // Can only insert into rests
        
        float restDurationQ = beat.getDurationInQuarters();
        
        // Determine the note duration: use insertDuration if it fits, otherwise use rest duration
        NoteDuration noteDur = insertDuration;
        bool noteDotted = false;
        
        // Calculate chosen duration in quarters
        float noteDurQ = 4.0f / static_cast<float>(noteDur);
        if (noteDotted) noteDurQ *= 1.5f;
        
        // If chosen duration is longer than the rest, use the rest's duration
        if (noteDurQ > restDurationQ + 0.001f)
        {
            noteDur = beat.duration;
            noteDotted = beat.isDotted;
            noteDurQ = restDurationQ;
        }
        
        // Create the new note
        TabNote newNote;
        newNote.string = stringIndex;
        newNote.fret = fret;
        newNote.midiNote = midiNote;
        newNote.velocity = 100;
        newNote.isManuallyEdited = true;
        
        // Convert the rest beat into a note beat
        beat.isRest = false;
        beat.notes.clear();
        beat.notes.add(newNote);
        beat.duration = noteDur;
        beat.isDotted = noteDotted;
        beat.isDoubleDotted = false;
        
        // If there is remaining time, insert a rest after this beat
        float remaining = restDurationQ - noteDurQ;
        if (remaining > 0.01f)
        {
            // Fill remaining time with rest(s)
            int insertPos = beatIndex + 1;
            while (remaining > 0.01f)
            {
                auto restDur = findClosestDuration(remaining);
                TabBeat restBeat;
                restBeat.isRest = true;
                restBeat.duration = restDur.first;
                restBeat.isDotted = restDur.second;
                float restLen = restBeat.getDurationInQuarters();
                remaining -= restLen;
                if (insertPos > measure.beats.size()) insertPos = measure.beats.size();
                measure.beats.insert(insertPos, restBeat);
                insertPos++;
            }
        }
        
        // Update lastSelectedNote to point to the new note
        lastSelectedNote.valid = true;
        lastSelectedNote.measureIndex = measureIndex;
        lastSelectedNote.beatIndex = beatIndex;
        lastSelectedNote.noteIndex = 0;
        lastSelectedNote.stringIndex = stringIndex;
        lastSelectedNote.fret = fret;
        lastSelectedNote.midiNote = midiNote;
        
        // Notify processor
        if (onNoteInserted)
            onNoteInserted(measureIndex, beatIndex, stringIndex, fret, midiNote);
        
        recalculateLayout();
        repaint();
    }
    
    /** Navigiere zum nächsten/vorherigen Beat */
    void navigateBeat(int direction)
    {
        int m = lastSelectedNote.valid ? lastSelectedNote.measureIndex : 0;
        int b = lastSelectedNote.valid ? lastSelectedNote.beatIndex : -1;
        
        if (m < 0 || m >= track.measures.size()) m = 0;
        
        b += direction;
        
        // Handle measure boundary
        if (b < 0)
        {
            // Go to last beat of previous measure
            m--;
            if (m < 0) return;  // Already at start
            b = track.measures[m].beats.size() - 1;
        }
        else if (m < track.measures.size() && b >= track.measures[m].beats.size())
        {
            // Go to first beat of next measure
            m++;
            if (m >= track.measures.size()) return;  // Already at end
            b = 0;
        }
        
        if (m < 0 || m >= track.measures.size()) return;
        const auto& measure = track.measures[m];
        if (b < 0 || b >= measure.beats.size()) return;
        const auto& beat = measure.beats[b];
        
        // Update lastSelectedNote
        lastSelectedNote.measureIndex = m;
        lastSelectedNote.beatIndex = b;
        
        if (!beat.isRest && !beat.notes.isEmpty())
        {
            // Select the first note in the beat
            const auto& note = beat.notes[0];
            lastSelectedNote.valid = true;
            lastSelectedNote.noteIndex = 0;
            lastSelectedNote.stringIndex = note.string;
            lastSelectedNote.fret = note.fret;
            lastSelectedNote.midiNote = note.midiNote >= 0 ? note.midiNote : 
                (note.string < track.tuning.size() ? track.tuning[note.string] + note.fret : -1);
            
            // Find alternatives
            if (lastSelectedNote.midiNote >= 0)
            {
                fretCalculator.setTuning(track.tuning);
                lastSelectedNote.alternatives = fretCalculator.calculateAlternatives(
                    lastSelectedNote.midiNote, lastSelectedNote.stringIndex, lastSelectedNote.fret);
            }
        }
        else
        {
            // Rest - set position but mark as "no note selected"
            lastSelectedNote.valid = false;
            lastSelectedNote.noteIndex = -1;
            lastSelectedNote.stringIndex = 0;
            lastSelectedNote.fret = -1;
            lastSelectedNote.midiNote = -1;
        }
        
        // Scroll to make the beat visible
        if (m >= 0 && m < track.measures.size())
        {
            float measureX = track.measures[m].xPosition;
            float viewWidth = static_cast<float>(getWidth()) - 20.0f;
            if (measureX < scrollOffset || measureX + track.measures[m].calculatedWidth > scrollOffset + viewWidth)
            {
                scrollOffset = juce::jmax(0.0f, measureX - viewWidth * 0.1f);
                updateScrollbar();
            }
        }
        
        repaint();
    }
    
    void recalculateLayout()
    {
        // Apply zoom to config
        TabLayoutConfig scaledConfig = config;
        scaledConfig.stringSpacing *= zoom;
        scaledConfig.measurePadding *= zoom;
        scaledConfig.minBeatSpacing *= zoom;
        scaledConfig.baseNoteWidth *= zoom;
        
        // Calculate layout
        totalWidth = layoutEngine.calculateLayout(track, scaledConfig, getWidth()) + 50.0f;
        
        // Clamp scroll offset
        scrollOffset = juce::jlimit(0.0f, juce::jmax(0.0f, totalWidth - getWidth()), scrollOffset);
        
        updateScrollbar();
    }
    
    void updateScrollbar()
    {
        float visibleWidth = getWidth();
        horizontalScrollbar.setRangeLimits(0.0, totalWidth);
        horizontalScrollbar.setCurrentRange(scrollOffset, visibleWidth);
    }
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TabViewComponent)
};
