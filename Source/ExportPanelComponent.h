/*
  ==============================================================================

    ExportPanelComponent.h
    
    Panel for editing song metadata and track instruments before export

  ==============================================================================
*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "TabModels.h"
#include <vector>
#include <functional>

//==============================================================================
// GM Instrument names (128 instruments)
//==============================================================================
static const char* gmInstrumentNamesUI[] = {
    "0: Acoustic Grand Piano", "1: Bright Acoustic Piano", "2: Electric Grand Piano", "3: Honky-tonk Piano",
    "4: Electric Piano 1", "5: Electric Piano 2", "6: Harpsichord", "7: Clavi",
    "8: Celesta", "9: Glockenspiel", "10: Music Box", "11: Vibraphone",
    "12: Marimba", "13: Xylophone", "14: Tubular Bells", "15: Dulcimer",
    "16: Drawbar Organ", "17: Percussive Organ", "18: Rock Organ", "19: Church Organ",
    "20: Reed Organ", "21: Accordion", "22: Harmonica", "23: Tango Accordion",
    "24: Acoustic Guitar (nylon)", "25: Acoustic Guitar (steel)", "26: Electric Guitar (jazz)", "27: Electric Guitar (clean)",
    "28: Electric Guitar (muted)", "29: Overdriven Guitar", "30: Distortion Guitar", "31: Guitar Harmonics",
    "32: Acoustic Bass", "33: Electric Bass (finger)", "34: Electric Bass (pick)", "35: Fretless Bass",
    "36: Slap Bass 1", "37: Slap Bass 2", "38: Synth Bass 1", "39: Synth Bass 2",
    "40: Violin", "41: Viola", "42: Cello", "43: Contrabass",
    "44: Tremolo Strings", "45: Pizzicato Strings", "46: Orchestral Harp", "47: Timpani",
    "48: String Ensemble 1", "49: String Ensemble 2", "50: Synth Strings 1", "51: Synth Strings 2",
    "52: Choir Aahs", "53: Voice Oohs", "54: Synth Voice", "55: Orchestra Hit",
    "56: Trumpet", "57: Trombone", "58: Tuba", "59: Muted Trumpet",
    "60: French Horn", "61: Brass Section", "62: Synth Brass 1", "63: Synth Brass 2",
    "64: Soprano Sax", "65: Alto Sax", "66: Tenor Sax", "67: Baritone Sax",
    "68: Oboe", "69: English Horn", "70: Bassoon", "71: Clarinet",
    "72: Piccolo", "73: Flute", "74: Recorder", "75: Pan Flute",
    "76: Blown Bottle", "77: Shakuhachi", "78: Whistle", "79: Ocarina",
    "80: Lead 1 (square)", "81: Lead 2 (sawtooth)", "82: Lead 3 (calliope)", "83: Lead 4 (chiff)",
    "84: Lead 5 (charang)", "85: Lead 6 (voice)", "86: Lead 7 (fifths)", "87: Lead 8 (bass+lead)",
    "88: Pad 1 (new age)", "89: Pad 2 (warm)", "90: Pad 3 (polysynth)", "91: Pad 4 (choir)",
    "92: Pad 5 (bowed)", "93: Pad 6 (metallic)", "94: Pad 7 (halo)", "95: Pad 8 (sweep)",
    "96: FX 1 (rain)", "97: FX 2 (soundtrack)", "98: FX 3 (crystal)", "99: FX 4 (atmosphere)",
    "100: FX 5 (brightness)", "101: FX 6 (goblins)", "102: FX 7 (echoes)", "103: FX 8 (sci-fi)",
    "104: Sitar", "105: Banjo", "106: Shamisen", "107: Koto",
    "108: Kalimba", "109: Bag pipe", "110: Fiddle", "111: Shanai",
    "112: Tinkle Bell", "113: Agogo", "114: Steel Drums", "115: Woodblock",
    "116: Taiko Drum", "117: Melodic Tom", "118: Synth Drum", "119: Reverse Cymbal",
    "120: Guitar Fret Noise", "121: Breath Noise", "122: Seashore", "123: Bird Tweet",
    "124: Telephone Ring", "125: Helicopter", "126: Applause", "127: Gunshot"
};

//==============================================================================
// GM Drum Kit names (Channel 10 uses these instead of instruments)
//==============================================================================
static const char* gmDrumKitNamesUI[] = {
    "0: Standard Kit",
    "8: Room Kit",
    "16: Power Kit",
    "24: Electronic Kit",
    "25: TR-808 Kit",
    "32: Jazz Kit",
    "40: Brush Kit",
    "48: Orchestra Kit",
    "56: SFX Kit"
};

// Drum kit program numbers (index in gmDrumKitNamesUI -> MIDI program)
static const int gmDrumKitPrograms[] = { 0, 8, 16, 24, 25, 32, 40, 48, 56 };
static const int gmDrumKitCount = 9;

//==============================================================================
// Track row component for the export panel
//==============================================================================
class TrackRowComponent : public juce::Component
{
public:
    TrackRowComponent(int trackIndex, const juce::String& initialName, int initialInstrument, int midiChannel)
        : index(trackIndex), isDrumTrack(midiChannel == 9)  // Channel 10 = index 9
    {
        // Track number label
        trackLabel.setText(juce::String("Track ") + juce::String(trackIndex + 1) + ":", juce::dontSendNotification);
        trackLabel.setJustificationType(juce::Justification::centredRight);
        addAndMakeVisible(trackLabel);
        
        // Track name editor
        nameEditor.setText(initialName);
        nameEditor.setJustification(juce::Justification::centredLeft);
        addAndMakeVisible(nameEditor);
        
        // Instrument/Drum selector
        if (isDrumTrack)
        {
            // Drum channel - show drum kits
            for (int i = 0; i < gmDrumKitCount; ++i)
                instrumentSelector.addItem(gmDrumKitNamesUI[i], i + 1);
            
            // Find matching drum kit or default to Standard
            int selectedIndex = 0;
            for (int i = 0; i < gmDrumKitCount; ++i)
            {
                if (gmDrumKitPrograms[i] == initialInstrument)
                {
                    selectedIndex = i;
                    break;
                }
            }
            instrumentSelector.setSelectedId(selectedIndex + 1, juce::dontSendNotification);
        }
        else
        {
            // Normal instrument channel
            for (int i = 0; i < 128; ++i)
                instrumentSelector.addItem(gmInstrumentNamesUI[i], i + 1);
            instrumentSelector.setSelectedId(initialInstrument + 1, juce::dontSendNotification);
        }
        addAndMakeVisible(instrumentSelector);
    }
    
    void resized() override
    {
        auto area = getLocalBounds().reduced(2);
        trackLabel.setBounds(area.removeFromLeft(70));
        area.removeFromLeft(5);
        nameEditor.setBounds(area.removeFromLeft(200));
        area.removeFromLeft(10);
        instrumentSelector.setBounds(area);
    }
    
    juce::String getTrackName() const { return nameEditor.getText(); }
    
    int getInstrument() const 
    { 
        if (isDrumTrack)
        {
            int idx = instrumentSelector.getSelectedId() - 1;
            if (idx >= 0 && idx < gmDrumKitCount)
                return gmDrumKitPrograms[idx];
            return 0;  // Standard Kit
        }
        return instrumentSelector.getSelectedId() - 1; 
    }
    
private:
    int index;
    bool isDrumTrack;
    juce::Label trackLabel;
    juce::TextEditor nameEditor;
    juce::ComboBox instrumentSelector;
};

//==============================================================================
// Export Panel Component
//==============================================================================
class ExportPanelComponent : public juce::Component
{
public:
    ExportPanelComponent(const juce::String& initialTitle, 
                         const std::vector<TabTrack>& tracks,
                         std::function<void(const juce::String&, const std::vector<std::pair<juce::String, int>>&)> onExport,
                         std::function<void()> onCancel)
        : exportCallback(onExport), cancelCallback(onCancel)
    {
        // Title
        titleLabel.setText("Song Title:", juce::dontSendNotification);
        titleLabel.setJustificationType(juce::Justification::centredRight);
        addAndMakeVisible(titleLabel);
        
        titleEditor.setText(initialTitle);
        addAndMakeVisible(titleEditor);
        
        // Tracks header
        tracksLabel.setText("Tracks:", juce::dontSendNotification);
        tracksLabel.setFont(juce::Font(16.0f).boldened());
        addAndMakeVisible(tracksLabel);
        
        // Create track rows - pass midiChannel for drum detection
        for (size_t i = 0; i < tracks.size(); ++i)
        {
            auto row = std::make_unique<TrackRowComponent>(
                (int)i, 
                tracks[i].name, 
                tracks[i].midiInstrument,
                tracks[i].midiChannel  // Pass channel for drum detection
            );
            addAndMakeVisible(*row);
            trackRows.push_back(std::move(row));
        }
        
        // Buttons
        exportButton.setButtonText("Export GP5");
        exportButton.onClick = [this]() { doExport(); };
        addAndMakeVisible(exportButton);
        
        cancelButton.setButtonText("Cancel");
        cancelButton.onClick = [this]() { 
            if (cancelCallback) cancelCallback(); 
        };
        addAndMakeVisible(cancelButton);
        
        // Calculate height: header(50) + title(35) + tracksLabel(30) + tracks + buttons(50) + padding
        int tracksHeight = juce::jmin((int)tracks.size() * 35, 300);  // Max 300px for tracks
        setSize(600, 50 + 35 + 30 + tracksHeight + 60);
    }
    
    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(40, 40, 45));
        
        // Border
        g.setColour(juce::Colours::grey);
        g.drawRect(getLocalBounds(), 2);
        
        // Header
        g.setColour(juce::Colour(60, 60, 65));
        g.fillRect(0, 0, getWidth(), 40);
        
        g.setColour(juce::Colours::white);
        g.setFont(18.0f);
        g.drawText("Export Recording", 10, 10, getWidth() - 20, 20, juce::Justification::centred);
    }
    
    void resized() override
    {
        auto area = getLocalBounds().reduced(10);
        area.removeFromTop(40);  // Header
        
        // Title row
        auto titleRow = area.removeFromTop(30);
        titleLabel.setBounds(titleRow.removeFromLeft(80));
        titleRow.removeFromLeft(5);
        titleEditor.setBounds(titleRow);
        
        area.removeFromTop(10);
        
        // Tracks label
        tracksLabel.setBounds(area.removeFromTop(25));
        
        area.removeFromTop(5);
        
        // Reserve space for buttons at bottom (fixed 50px)
        auto buttonArea = area.removeFromBottom(50);
        
        // Track rows (remaining space)
        for (auto& row : trackRows)
        {
            row->setBounds(area.removeFromTop(30));
            area.removeFromTop(5);
        }
        
        // Buttons - center them horizontally with fixed size
        int totalButtonWidth = 120 + 10 + 100;  // export + gap + cancel
        int buttonX = (buttonArea.getWidth() - totalButtonWidth) / 2;
        
        exportButton.setBounds(buttonArea.getX() + buttonX, buttonArea.getY() + 8, 120, 34);
        cancelButton.setBounds(buttonArea.getX() + buttonX + 130, buttonArea.getY() + 8, 100, 34);
    }
    
private:
    void doExport()
    {
        if (exportCallback)
        {
            std::vector<std::pair<juce::String, int>> trackData;
            for (auto& row : trackRows)
            {
                trackData.push_back({ row->getTrackName(), row->getInstrument() });
            }
            exportCallback(titleEditor.getText(), trackData);
        }
    }
    
    juce::Label titleLabel;
    juce::TextEditor titleEditor;
    juce::Label tracksLabel;
    
    std::vector<std::unique_ptr<TrackRowComponent>> trackRows;
    
    juce::TextButton exportButton;
    juce::TextButton cancelButton;
    
    std::function<void(const juce::String&, const std::vector<std::pair<juce::String, int>>&)> exportCallback;
    std::function<void()> cancelCallback;
};
