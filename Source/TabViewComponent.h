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
    };
    
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
    
    // Set live MIDI notes to display (for editor mode)
    void setLiveNotes(const std::vector<LiveNote>& notes)
    {
        liveNotes = notes;
        repaint();
    }
    
    // Enable/disable editor mode (show empty tab with live notes)
    void setEditorMode(bool enabled)
    {
        editorMode = enabled;
        repaint();
    }
    
    bool isEditorMode() const { return editorMode; }
    
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
        const auto& measure = track.measures[measureIndex];
        float measureX = measure.xPosition * zoom;
        float measureWidth = measure.calculatedWidth * zoom;
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
        float measureX = track.measures[measureIndex].xPosition * zoom;
        float measureWidth = track.measures[measureIndex].calculatedWidth * zoom;
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
        
        // Draw track FIRST
        juce::Rectangle<float> renderBounds(0, yOffset, static_cast<float>(getWidth()), trackHeight);
        renderer.render(g, track, scaledConfig, renderBounds, scrollOffset, highlightedMeasure);
        
        // Draw current playing measure highlight AFTER track rendering (so it's visible on top)
        if (currentPlayingMeasure >= 0 && currentPlayingMeasure < track.measures.size())
        {
            const auto& measure = track.measures[currentPlayingMeasure];
            float measureX = 25.0f + measure.xPosition * zoom - scrollOffset;
            float measureWidth = measure.calculatedWidth * zoom;
            
            // Semi-transparent green overlay für aktuellen Takt
            g.setColour(juce::Colour(0x2000FF00));  // Transparentes Grün
            g.fillRect(measureX, yOffset, measureWidth, trackHeight);
            
            // Playhead-Linie an der exakten Position innerhalb des Taktes
            float playheadX = measureX + static_cast<float>(playheadPositionInMeasure) * measureWidth;
            g.setColour(juce::Colours::limegreen);
            g.fillRect(playheadX - 1.0f, yOffset, 3.0f, trackHeight);
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
                }
            }
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
    
    void mouseDown(const juce::MouseEvent& event) override
    {
        // Find clicked measure and position within it
        float clickX = event.position.x + scrollOffset - 25.0f;  // Account for clef offset
        
        for (int m = 0; m < track.measures.size(); ++m)
        {
            const auto& measure = track.measures[m];
            float measureStart = measure.xPosition * zoom;
            float measureWidth = measure.calculatedWidth * zoom;
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
    
    void scrollBarMoved(juce::ScrollBar* scrollBarThatHasMoved, double newRangeStart) override
    {
        if (scrollBarThatHasMoved == &horizontalScrollbar)
        {
            scrollOffset = static_cast<float>(newRangeStart);
            repaint();
        }
    }
    
private:
    TabTrack track;
    TabRenderer renderer;
    TabLayoutEngine layoutEngine;
    TabLayoutConfig config;
    
    float zoom = 1.0f;
    float scrollOffset = 0.0f;
    float totalWidth = 0.0f;
    int highlightedMeasure = -1;
    int currentPlayingMeasure = -1;
    double playheadPositionInMeasure = 0.0;
    float lastPlayheadX = 0.0f;  // Für Erkennung von Positionänderungen
    
    // Editor mode (live MIDI input display)
    bool editorMode = false;
    std::vector<LiveNote> liveNotes;
    
    juce::ScrollBar horizontalScrollbar { false };
    const int scrollbarHeight = 14;
    
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
