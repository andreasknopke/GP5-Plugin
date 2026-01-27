/*
  Local debug tool: Simulates what happens when Track 3 is loaded and processed
  Compile separately to debug the crash without Cubase
*/

#include <iostream>
#include <cstdlib>
#include "Source/GP5Parser.h"
#include "Source/MidiExpressionEngine.h"

int main()
{
    std::cout << "=== GP5 Local Debug Tool ===" << std::endl;
    
    GP5Parser parser;
    
    // Load the test file
    juce::File gp5File("D:\\GitHub\\NewProject\\test_partial.gp5");
    
    if (!gp5File.existsAsFile())
    {
        std::cerr << "File not found: " << gp5File.getFullPathName() << std::endl;
        return 1;
    }
    
    std::cout << "Loading: " << gp5File.getFullPathName() << std::endl;
    
    if (!parser.loadFile(gp5File))
    {
        std::cerr << "Failed to parse GP5 file!" << std::endl;
        return 1;
    }
    
    const auto& tracks = parser.getTracks();
    std::cout << "Loaded " << tracks.size() << " tracks" << std::endl;
    
    // Focus on Track 3 (index 2)
    if (tracks.size() < 3)
    {
        std::cerr << "Not enough tracks!" << std::endl;
        return 1;
    }
    
    const auto& track = tracks[2];
    std::cout << "\n=== Track 3: " << track.name << " ===" << std::endl;
    std::cout << "Strings: " << track.stringCount << std::endl;
    std::cout << "Tuning size: " << track.tuning.size() << std::endl;
    
    for (int i = 0; i < track.tuning.size(); ++i)
    {
        std::cout << "  String " << i << ": MIDI " << track.tuning[i] << std::endl;
    }
    
    std::cout << "\nMeasures: " << track.measures.size() << std::endl;
    
    // Simulate MIDI processing for measures 18-25
    MidiExpressionEngine engine;
    juce::MidiBuffer midiBuffer;
    
    double currentBeat = 0.0;
    double beatsPerSecond = 148.0 / 60.0;  // 148 BPM
    int midiChannel = 3;
    int transposeOffset = 0;
    int volumeScale = 100;
    
    std::cout << "\n=== Simulating MIDI generation ===" << std::endl;
    
    for (int measureIdx = 0; measureIdx < std::min(30, (int)track.measures.size()); ++measureIdx)
    {
        const auto& measure = track.measures[measureIdx];
        const auto& beats = measure.voice1;
        
        std::cout << "\nMeasure " << (measureIdx + 1) << ": " << beats.size() << " beats" << std::endl;
        
        for (int beatIdx = 0; beatIdx < beats.size(); ++beatIdx)
        {
            const auto& beat = beats[beatIdx];
            double beatDuration = beat.getDurationInBeats();
            
            // Debug output
            std::cout << "  Beat " << beatIdx << " @ pos " << currentBeat 
                      << " (dur=" << beatDuration << ", rest=" << beat.isRest 
                      << ", notes=" << beat.notes.size() << ")" << std::endl;
            
            // Check each note for problems
            for (const auto& [stringIndex, note] : beat.notes)
            {
                std::cout << "    Note: string=" << stringIndex << ", fret=" << note.fret;
                
                // Validate
                if (stringIndex < 0 || stringIndex >= 12)
                {
                    std::cout << " *** INVALID STRING INDEX! ***";
                }
                if (note.fret < 0 || note.fret > 30)
                {
                    std::cout << " *** INVALID FRET! ***";
                }
                
                // Calculate MIDI note like MidiExpressionEngine does
                int tuningSize = track.tuning.size();
                if (tuningSize > 0 && tuningSize <= 12 && stringIndex < tuningSize)
                {
                    int tuningValue = track.tuning.getUnchecked(stringIndex);
                    int midiNote = tuningValue + note.fret + transposeOffset;
                    std::cout << " -> MIDI " << midiNote;
                    
                    if (midiNote < 0 || midiNote >= 128)
                    {
                        std::cout << " *** INVALID MIDI NOTE! ***";
                    }
                }
                else
                {
                    std::cout << " *** TUNING ACCESS ERROR! stringIdx=" << stringIndex 
                              << " tuningSize=" << tuningSize << " ***";
                }
                
                std::cout << std::endl;
            }
            
            // Simulate processBeat call
            const GP5Beat* nextBeat = (beatIdx + 1 < beats.size()) ? &beats[beatIdx + 1] : nullptr;
            
            try
            {
                engine.processBeat(
                    midiBuffer,
                    midiChannel,
                    beat,
                    nextBeat,
                    track,
                    transposeOffset,
                    volumeScale,
                    currentBeat,
                    beatsPerSecond,
                    beatDuration,
                    0
                );
            }
            catch (const std::exception& e)
            {
                std::cerr << "  !!! EXCEPTION: " << e.what() << std::endl;
            }
            
            currentBeat += beatDuration;
        }
        
        // Simulate updateEffects call
        engine.updateEffects(midiBuffer, currentBeat, beatsPerSecond, 512);
    }
    
    std::cout << "\n=== Simulation complete ===" << std::endl;
    std::cout << "Final beat position: " << currentBeat << std::endl;
    
    return 0;
}
