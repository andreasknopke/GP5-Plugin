/*
  ==============================================================================

    TrackSettingsComponent.h
    
    UI Component for configuring MIDI channels per track
    with Solo, Mute, Volume, and Pan controls

  ==============================================================================
*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

//==============================================================================
// Single Track Row - displays one track's settings
//==============================================================================
class TrackSettingsRow : public juce::Component
{
public:
    TrackSettingsRow(int trackIndex, const juce::String& trackName, bool isDrum,
                     int currentChannel, bool isMuted, bool isSolo, int volume, int pan)
        : trackIdx(trackIndex), isDrumTrack(isDrum)
    {
        // Track Name Label
        addAndMakeVisible(nameLabel);
        juce::String displayName = juce::String(trackIndex + 1) + ": " + trackName;
        if (isDrum) displayName += " [D]";
        nameLabel.setText(displayName, juce::dontSendNotification);
        nameLabel.setFont(juce::FontOptions(12.0f));
        nameLabel.setColour(juce::Label::textColourId, juce::Colours::white);
        
        // MIDI Channel ComboBox
        addAndMakeVisible(channelSelector);
        for (int ch = 1; ch <= 16; ++ch)
        {
            juce::String label = juce::String(ch);
            if (ch == 10) label += " (D)";
            channelSelector.addItem(label, ch);
        }
        channelSelector.setSelectedId(currentChannel, juce::dontSendNotification);
        channelSelector.setTooltip("MIDI Channel");
        
        // Solo Button
        addAndMakeVisible(soloButton);
        soloButton.setButtonText("S");
        soloButton.setToggleState(isSolo, juce::dontSendNotification);
        updateSoloButtonColor();
        soloButton.setTooltip("Solo");
        soloButton.onClick = [this]() {
            bool newState = !soloButton.getToggleState();
            soloButton.setToggleState(newState, juce::dontSendNotification);
            updateSoloButtonColor();
            if (onSoloChanged) onSoloChanged(trackIdx, newState);
        };
        
        // Mute Button
        addAndMakeVisible(muteButton);
        muteButton.setButtonText("M");
        muteButton.setToggleState(isMuted, juce::dontSendNotification);
        updateMuteButtonColor();
        muteButton.setTooltip("Mute");
        muteButton.onClick = [this]() {
            bool newState = !muteButton.getToggleState();
            muteButton.setToggleState(newState, juce::dontSendNotification);
            updateMuteButtonColor();
            if (onMuteChanged) onMuteChanged(trackIdx, newState);
        };
        
        // Volume Slider
        addAndMakeVisible(volumeSlider);
        volumeSlider.setRange(0, 127, 1);
        volumeSlider.setValue(volume, juce::dontSendNotification);
        volumeSlider.setSliderStyle(juce::Slider::LinearHorizontal);
        volumeSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 35, 20);
        volumeSlider.setTooltip("Volume (CC7)");
        volumeSlider.onValueChange = [this]() {
            if (onVolumeChanged) onVolumeChanged(trackIdx, static_cast<int>(volumeSlider.getValue()));
        };
        
        // Pan Slider
        addAndMakeVisible(panSlider);
        panSlider.setRange(0, 127, 1);
        panSlider.setValue(pan, juce::dontSendNotification);
        panSlider.setSliderStyle(juce::Slider::LinearHorizontal);
        panSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 35, 20);
        panSlider.setTooltip("Pan (CC10) - 64=Center");
        panSlider.onValueChange = [this]() {
            if (onPanChanged) onPanChanged(trackIdx, static_cast<int>(panSlider.getValue()));
        };
        
        // Volume Label
        addAndMakeVisible(volLabel);
        volLabel.setText("Vol", juce::dontSendNotification);
        volLabel.setFont(juce::FontOptions(10.0f));
        volLabel.setColour(juce::Label::textColourId, juce::Colours::grey);
        
        // Pan Label
        addAndMakeVisible(panLabel);
        panLabel.setText("Pan", juce::dontSendNotification);
        panLabel.setFont(juce::FontOptions(10.0f));
        panLabel.setColour(juce::Label::textColourId, juce::Colours::grey);
    }
    
    void updateMuteButtonColor()
    {
        if (impliedMute && !muteButton.getToggleState())
        {
            // Implied mute (due to another track being solo) - orange color
            muteButton.setColour(juce::TextButton::buttonColourId, juce::Colours::orange.darker());
        }
        else
        {
            muteButton.setColour(juce::TextButton::buttonColourId,
                muteButton.getToggleState() ? juce::Colours::red.darker() : juce::Colours::grey.darker());
        }
    }
    
    void updateSoloButtonColor()
    {
        soloButton.setColour(juce::TextButton::buttonColourId,
            soloButton.getToggleState() ? juce::Colours::yellow.darker() : juce::Colours::grey.darker());
    }
    
    // Called when solo state changes on any track - updates visual "implied mute" state
    void setImpliedMute(bool implied)
    {
        impliedMute = implied;
        updateMuteButtonColor();
    }
    
    bool isSolo() const { return soloButton.getToggleState(); }
    bool isMuted() const { return muteButton.getToggleState(); }
    
    void resized() override
    {
        auto bounds = getLocalBounds().reduced(2);
        int rowHeight = bounds.getHeight();
        
        // Top row: Name, Channel, Solo, Mute
        auto topRow = bounds.removeFromTop(rowHeight / 2);
        
        nameLabel.setBounds(topRow.removeFromLeft(140));
        topRow.removeFromLeft(5);
        
        channelSelector.setBounds(topRow.removeFromLeft(70));
        topRow.removeFromLeft(5);
        
        soloButton.setBounds(topRow.removeFromLeft(25));
        topRow.removeFromLeft(3);
        muteButton.setBounds(topRow.removeFromLeft(25));
        
        // Bottom row: Volume and Pan sliders
        auto bottomRow = bounds;
        bottomRow.removeFromLeft(5);
        
        volLabel.setBounds(bottomRow.removeFromLeft(25));
        volumeSlider.setBounds(bottomRow.removeFromLeft(130));
        bottomRow.removeFromLeft(10);
        
        panLabel.setBounds(bottomRow.removeFromLeft(25));
        panSlider.setBounds(bottomRow.removeFromLeft(130));
    }
    
    // Callbacks
    std::function<void(int trackIndex, int channel)> onChannelChanged;
    std::function<void(int trackIndex, bool muted)> onMuteChanged;
    std::function<void(int trackIndex, bool solo)> onSoloChanged;
    std::function<void(int trackIndex, int volume)> onVolumeChanged;
    std::function<void(int trackIndex, int pan)> onPanChanged;
    
    juce::ComboBox& getChannelSelector() { return channelSelector; }
    
private:
    int trackIdx;
    bool isDrumTrack;
    bool impliedMute = false;  // True when another track is solo (visual indication)
    
    juce::Label nameLabel;
    juce::ComboBox channelSelector;
    juce::TextButton soloButton;
    juce::TextButton muteButton;
    juce::Slider volumeSlider;
    juce::Slider panSlider;
    juce::Label volLabel;
    juce::Label panLabel;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TrackSettingsRow)
};

//==============================================================================
// Main Settings Panel
//==============================================================================
class TrackSettingsComponent : public juce::Component
{
public:
    TrackSettingsComponent(NewProjectAudioProcessor& processor)
        : audioProcessor(processor)
    {
        // Title
        addAndMakeVisible(titleLabel);
        titleLabel.setText("Track MIDI Settings", juce::dontSendNotification);
        titleLabel.setFont(juce::FontOptions(16.0f, juce::Font::bold));
        titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
        titleLabel.setJustificationType(juce::Justification::centred);
        
        // Column Headers
        addAndMakeVisible(headerLabel);
        headerLabel.setText("Track                        Ch     S  M       Volume              Pan", juce::dontSendNotification);
        headerLabel.setFont(juce::FontOptions(10.0f));
        headerLabel.setColour(juce::Label::textColourId, juce::Colours::grey);
        
        // Close Button
        addAndMakeVisible(closeButton);
        closeButton.setButtonText("X");
        closeButton.onClick = [this]() {
            if (onClose) onClose();
        };
        
        // Viewport for scrollable content
        addAndMakeVisible(viewport);
        viewport.setViewedComponent(&trackListContainer, false);
        viewport.setScrollBarsShown(true, false);
        
        refreshTrackList();
    }
    
    void refreshTrackList()
    {
        trackRows.clear();
        trackListContainer.removeAllChildren();
        
        const auto& tracks = audioProcessor.getGP5Parser().getTracks();
        
        int yPos = 0;
        const int rowHeight = 50;  // Taller rows for two-row layout
        
        for (int i = 0; i < tracks.size(); ++i)
        {
            const auto& track = tracks[i];
            
            auto* row = trackRows.add(new TrackSettingsRow(
                i,
                track.name,
                track.isPercussion,
                audioProcessor.getTrackMidiChannel(i),
                audioProcessor.isTrackMuted(i),
                audioProcessor.isTrackSolo(i),
                audioProcessor.getTrackVolume(i),
                audioProcessor.getTrackPan(i)
            ));
            
            row->setBounds(0, yPos, 450, rowHeight);
            trackListContainer.addAndMakeVisible(row);
            
            // Connect callbacks
            row->getChannelSelector().onChange = [this, i, row]() {
                int newChannel = row->getChannelSelector().getSelectedId();
                audioProcessor.setTrackMidiChannel(i, newChannel);
            };
            
            row->onMuteChanged = [this](int trackIndex, bool muted) {
                audioProcessor.setTrackMuted(trackIndex, muted);
            };
            
            row->onSoloChanged = [this](int trackIndex, bool solo) {
                audioProcessor.setTrackSolo(trackIndex, solo);
                updateAllMuteVisuals();  // Update implied mute on all tracks
            };
            
            row->onVolumeChanged = [this](int trackIndex, int volume) {
                audioProcessor.setTrackVolume(trackIndex, volume);
            };
            
            row->onPanChanged = [this](int trackIndex, int pan) {
                audioProcessor.setTrackPan(trackIndex, pan);
            };
            
            yPos += rowHeight;
        }
        
        trackListContainer.setSize(450, yPos);
        
        // Initialize implied mute visuals based on current solo state
        updateAllMuteVisuals();
    }
    
    void paint(juce::Graphics& g) override
    {
        // Semi-transparent dark background
        g.fillAll(juce::Colour(0xF0252528));
        
        // Border
        g.setColour(juce::Colours::grey);
        g.drawRect(getLocalBounds(), 1);
    }
    
    void resized() override
    {
        auto bounds = getLocalBounds().reduced(10);
        
        // Title row
        auto titleRow = bounds.removeFromTop(25);
        closeButton.setBounds(titleRow.removeFromRight(25));
        titleLabel.setBounds(titleRow);
        
        bounds.removeFromTop(5);
        
        // Header
        headerLabel.setBounds(bounds.removeFromTop(18));
        bounds.removeFromTop(3);
        
        // Track list viewport
        viewport.setBounds(bounds);
        
        // Update container width
        int containerWidth = bounds.getWidth() - 20;
        trackListContainer.setSize(containerWidth, trackListContainer.getHeight());
        for (auto* row : trackRows)
        {
            row->setSize(containerWidth, row->getHeight());
        }
    }
    
    std::function<void()> onClose;
    
    // Updates implied mute visual state on all rows based on solo status
    void updateAllMuteVisuals()
    {
        bool anySolo = audioProcessor.hasAnySolo();
        
        for (auto* row : trackRows)
        {
            if (anySolo)
            {
                // If any track is solo, non-solo tracks show implied mute
                row->setImpliedMute(!row->isSolo());
            }
            else
            {
                // No solo active, clear implied mute on all
                row->setImpliedMute(false);
            }
        }
    }
    
private:
    NewProjectAudioProcessor& audioProcessor;
    
    juce::Label titleLabel;
    juce::Label headerLabel;
    juce::TextButton closeButton;
    
    juce::Viewport viewport;
    juce::Component trackListContainer;
    juce::OwnedArray<TrackSettingsRow> trackRows;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TrackSettingsComponent)
};
