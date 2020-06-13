#include <JuceHeader.h>
#include <math.h>
#include <string.h>

class MainComponent : public AudioAppComponent, private Timer {
public:
    MainComponent() {
        // set the size of the component after
        // you add any child components.
        setSize(initialWidth, initialHeight);
        setAudioChannels(0, 2);

        sampler.addVoice(new SamplerVoice());  // hat
        sampler.addVoice(new SamplerVoice());  // snare
        sampler.addVoice(new SamplerVoice());  // kick

        BigInteger hatMidiRange;
        hatMidiRange.setRange(60, 61, true);
        WavAudioFormat wavFormat;
        std::unique_ptr<AudioFormatReader> hatReader(wavFormat.createReaderFor(new MemoryInputStream(BinaryData::hat_wav, BinaryData::hat_wavSize, false), true));
        sampler.addSound(new SamplerSound("hat",
                                          *hatReader,
                                          hatMidiRange,
                                          60,  // midi note
                                          0.,  // attack time in seconds
                                          0.,  // release time in seconds
                                          3.));  // maximum sample length in seconds
        
        BigInteger snareMidiRange;
        snareMidiRange.setRange(61, 62, true);
        std::unique_ptr<AudioFormatReader> snareReader(wavFormat.createReaderFor(new MemoryInputStream(BinaryData::snare_wav, BinaryData::snare_wavSize, false), true));
        sampler.addSound(new SamplerSound("snare",
                                          *snareReader,
                                          snareMidiRange,
                                          61,  // midi note
                                          0.,  // attack time in seconds
                                          0.,  // release time in seconds
                                          3.));  // maximum sample length in seconds
        
        BigInteger kickMidiRange;
        kickMidiRange.setRange(62, 63, true);
        std::unique_ptr<AudioFormatReader> kickReader(wavFormat.createReaderFor(new MemoryInputStream(BinaryData::kick_wav, BinaryData::kick_wavSize, false), true));
        sampler.addSound(new SamplerSound("kick",
                                          *kickReader,
                                          kickMidiRange,
                                          62,  // midi note
                                          0.,  // attack time in seconds
                                          0.,  // release time in seconds
                                          3.));  // maximum sample length in seconds
    }
    ~MainComponent() {
        shutdownAudio();
        sampler.clearSounds();
        sampler.clearVoices();
    }

    void paint(Graphics& g) override {
        g.fillAll(Colour(32, 32, 32));
        
        g.setColour(Colours::rebeccapurple);
        g.setFont(getCubicFont());
        g.setFont(radiusStep);
        g.drawText(std::to_string(bpm),
                   getWidth() / 2.f - radiusStep * 1.25,
                   getHeight() / 2.f - radiusStep,
                   radiusStep * 2.5f,
                   radiusStep * 2.5f,
                   Justification::centred);
        
        drawCircleCursor(g, hatRadius + radiusStep / 2.f);
        
        drawCircleGuide(g, hatRadius);
        drawCircleGuide(g, snareRadius);
        drawCircleGuide(g, kickRadius);
        
        drawCircleGrid(g, hatRadius, hatSubdivisions);
        drawCircleGrid(g, snareRadius, snareSubdivisions);
        drawCircleGrid(g, kickRadius, kickSubdivisions);
        
        drawHatBits(g);
    }
    void resized() override {
        radiusStep = getWidth() / 13.f;
        hatRadius = getWidth() / 2.5f;
        snareRadius = hatRadius - radiusStep;
        kickRadius = snareRadius - radiusStep;
    }
    
    void mouseDoubleClick(const MouseEvent& event) override {
        float x = event.x - getWidth() / 2.f;
        float y = event.y - getHeight() / 2.f;
        
        float clickRadius = sqrt(x*x + y*y);
        
        // BAIL
        if (clickRadius > radiusStep * 2.5) return;
        
        bpm = 96;
        statusRatio = 1.f;
        stopTimer();
        repaint();
    }
    
    void mouseDown(const MouseEvent& event) override {
        float x = event.x - getWidth() / 2.f;
        float y = event.y - getHeight() / 2.f;
        
        float clickRadius = sqrt(x*x + y*y);
        float clickAngle = atan2(x, -y);

        bool toggleHat = false;
        bool toggleSnare = false;
        bool toggleKick = false;
        bool timeChange = false;
        if (clickRadius < radiusStep * 2.5) {
            playing = !playing;
            if (playing) {
                timerIntervalHz = (bpm / 60.f) * 12;
                tickEven = true;
                tickCounter = 0;
                startTimerHz(timerIntervalHz);
            } else {
                statusRatio = 1.f;
                stopTimer();
                sampler.allNotesOff(0, true);
            }
        } else if (clickRadius < kickRadius + radiusStep / 2.f) {
            toggleKick = true;
        } else if (clickRadius < snareRadius + radiusStep / 2.f) {
            toggleSnare = true;
        } else if (clickRadius < hatRadius + radiusStep / 2.f) {
            toggleHat = true;
        } else if (clickAngle >= 0 && abs(clickAngle) > MathConstants<float>::halfPi) {
            bpm += 1;
            statusRatio = 1.f;
            stopTimer();
        } else if (clickAngle < 0 && abs(clickAngle) > MathConstants<float>::halfPi) {
            bpm -= 1;
            statusRatio = 1.f;
            stopTimer();
        } else if (clickAngle >= 0) {
            doubled = !doubled;
            timeChange = true;
        } else if (clickAngle < 0) {
            triplet = !triplet;
            timeChange = true;
        }
        
        if (timeChange) {
            if (doubled && triplet) {
                hatSubdivisions = 24;
                snareSubdivisions = 16;
                kickSubdivisions = 16;
            } else if (doubled && !triplet) {
                hatSubdivisions = 16;
                snareSubdivisions = 16;
                kickSubdivisions = 16;
            } else if (!doubled && triplet) {
                hatSubdivisions = 12;
                snareSubdivisions = 8;
                kickSubdivisions = 8;
            } else if (!doubled && !triplet) {
                hatSubdivisions = 8;
                snareSubdivisions = 8;
                kickSubdivisions = 8;
            }
        }

        if (toggleHat) {
            float hatIndexf = (clickAngle / MathConstants<float>::twoPi) * hatSubdivisions + 0.5f;
            int hatIndex = floor(hatIndexf);
            if (hatIndex < 0) {
                hatIndex = hatSubdivisions + hatIndex;
            }
            
            if (doubled && triplet) {
                doubledTripletArr[hatIndex] = !doubledTripletArr[hatIndex];
            } else if (doubled && !triplet) {
                doubledArr[0][hatIndex] = !doubledArr[0][hatIndex];
            } else if (!doubled && triplet) {
                tripletArr[hatIndex] = !tripletArr[hatIndex];
            } else if (!doubled && !triplet) {
                timeArr[0][hatIndex] = !timeArr[0][hatIndex];
            }
        }
        
        if (toggleSnare) {
            float snareIndexf = (clickAngle / MathConstants<float>::twoPi) * snareSubdivisions + 0.5f;
            int snareIndex = floor(snareIndexf);
            if (snareIndex < 0) {
                snareIndex = snareSubdivisions + snareIndex;
            }
            
            if (doubled) {
                doubledArr[1][snareIndex] = !doubledArr[1][snareIndex];
            } else {
                timeArr[1][snareIndex] = !timeArr[1][snareIndex];
            }
        }

        if (toggleKick) {
            float kickIndexf = (clickAngle / MathConstants<float>::twoPi) * kickSubdivisions + 0.5f;
            int kickIndex = floor(kickIndexf);
            if (kickIndex < 0) {
                kickIndex = kickSubdivisions + kickIndex;
            }
            
            if (doubled) {
                doubledArr[2][kickIndex] = !doubledArr[2][kickIndex];
            } else {
                timeArr[2][kickIndex] = !timeArr[2][kickIndex];
            }
        }
        
        repaint();
    }
    
    void getNextAudioBlock(const AudioSourceChannelInfo& bufferToFill) override {
        // dummy midi buffer
        MidiBuffer dummyMidiBuffer;
        sampler.renderNextBlock(*bufferToFill.buffer,
                                dummyMidiBuffer,
                                bufferToFill.startSample,
                                bufferToFill.numSamples);
    }
    
    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override {
        sampler.setCurrentPlaybackSampleRate(sampleRate);
    }
    
    void releaseResources() override {}

    void timerCallback() override {
        // every beat will have 12 events to accomodate triplets
        // there are 4 beats in the cicrle
        int hatIndex = std::floor(tickCounter / (48 / hatSubdivisions));
        int snareIndex = std::floor(tickCounter / (48 / snareSubdivisions));
        int kickIndex = std::floor(tickCounter / (48 / kickSubdivisions));
        
        bool triggerHat = false;
        if (hatIndex != prevHatIndex) {
            if (doubled && triplet) {
                if (doubledTripletArr[hatIndex]) triggerHat = true;
            } else if (doubled && !triplet) {
                if (doubledArr[0][hatIndex]) triggerHat = true;
            } else if (!doubled && triplet) {
                if (tripletArr[hatIndex]) triggerHat = true;
            } else if (!doubled && !triplet) {
                if (timeArr[0][hatIndex]) triggerHat = true;
            }
        }
        if (triggerHat) sampler.noteOn(0, 60, .35);

        bool triggerSnare = false;
        if (snareIndex != prevSnareIndex) {
            if (doubled) {
                if (doubledArr[1][snareIndex]) triggerSnare = true;
            } else {
                if (timeArr[1][snareIndex]) triggerSnare = true;
            }
        }
        if (triggerSnare) sampler.noteOn(0, 61, .75);
        
        bool triggerKick = false;
        if (kickIndex != prevKickIndex) {
            if (doubled) {
                if (doubledArr[2][kickIndex]) triggerKick = true;
            } else {
                if (timeArr[2][kickIndex]) triggerKick = true;
            }
        }
        if (triggerKick) sampler.noteOn(0, 62, 1);
        
        if(playing) {
            statusRatio = tickCounter / 48.f;
        } else {
            statusRatio = 1.f;
        }
        
        if (tickEven) {
            repaint();
        }
        tickEven = !tickEven;

        tickCounter += 1;
        tickCounter %= 48;
        prevHatIndex = hatIndex;
        prevSnareIndex = snareIndex;
        prevKickIndex = kickIndex;
    }

private:
    int initialWidth{486};
    int initialHeight{864};
    float radiusStep{initialWidth / 13.f};
    float hatRadius{initialWidth / 2.5f};
    float snareRadius{hatRadius - radiusStep};
    float kickRadius{snareRadius - radiusStep};
    int hatSubdivisions{16};
    int snareSubdivisions{16};
    int kickSubdivisions{16};
    float statusRatio{1.f};
    bool playing{false};
    bool triplet{false};
    bool doubled{true};
    int bpm{96};
    float timerIntervalHz{(bpm / 60.f) * 12};
    bool tickEven{true};
    int tickCounter{0};
    int prevHatIndex{-1};
    int prevSnareIndex{-1};
    int prevKickIndex{-1};

    Synthesiser sampler;
    
    bool timeArr[3][8]
    {
        {true, true, true, true, true, true, true, true},
        {false, false, true, false, false, false, true, false},
        {true, false, false, false, false, true, false, false}
    };
    bool doubledArr[3][16]
    {
        {true, false, true, false, true, false, true, false, true, false, true, false, true, false, true, false},
        {false, false, false, false, true, false, false, false, false, false, false, false, true, false, false, false},
        {true, false, false, false, false, false, false, false, false, false, true, false, false, false, false, false}
    };
    bool tripletArr[12] {true, false, false, true, false, false, true, false, false, true, false, false};
    bool doubledTripletArr[24] {
        true, false, false, true, false, false, true, false, false, true, false, false,
        true, false, false, true, false, false, true, false, false, true, false, false
    };

    static const Font& getCubicFont() {
        static Font cubic {Font(Typeface::createSystemTypefaceFor(BinaryData::cubic_ttf,
                                                                  BinaryData::cubic_ttfSize))};
        return cubic;
    }
    
    void drawCircleCursor(Graphics& g, float radius) {
        Path path;
        path.addCentredArc(getWidth() / 2.f,
                           getHeight() / 2.f,
                           radius, radius,
                           0.f,
                           0.f, MathConstants<float>::twoPi * statusRatio,
                           true);  // startAsNewSubPath
        path.addCentredArc(getWidth() / 2.f,
                           getHeight() / 2.f,
                           radius - radiusStep, radius - radiusStep,
                           0.f,
                           0.f, MathConstants<float>::twoPi * statusRatio,
                           true);  // startAsNewSubPath
        path.addCentredArc(getWidth() / 2.f,
                           getHeight() / 2.f,
                           radius - 2 * radiusStep, radius - 2 * radiusStep,
                           0.f,
                           0.f, MathConstants<float>::twoPi * statusRatio,
                           true);  // startAsNewSubPath
        path.addCentredArc(getWidth() / 2.f,
                           getHeight() / 2.f,
                           radius - 3 * radiusStep, radius - 3 * radiusStep,
                           0.f,
                           0.f, MathConstants<float>::twoPi * statusRatio,
                           true);  // startAsNewSubPath
        g.setColour(Colour(135, 135, 135));
        g.strokePath(path, PathStrokeType(radiusStep / 28.f));
    }

    void drawCircleGuide(Graphics& g, float radius) {
        Path path;
        path.addEllipse(getWidth() / 2.f - radius,
                        getHeight() / 2.f - radius,
                        radius * 2, radius * 2);
        g.setColour(Colour(37, 37, 37));
        g.strokePath(path, PathStrokeType(radiusStep / 30.f));
    }

    void drawArcUnit(Graphics& g, float radius, int subdivisions, int index) {
        Path path;
        path.addCentredArc(getWidth() / 2.f,
                           getHeight() / 2.f,
                           radius, radius,
                           (MathConstants<float>::twoPi / subdivisions) * (index - .5f),
                           MathConstants<float>::twoPi / (subdivisions * 8),
                           MathConstants<float>::twoPi / subdivisions - MathConstants<float>::twoPi / (subdivisions * 8),
                           true);  // startAsNewSubPath
        g.setColour(Colour(55, 55, 55));
        g.strokePath(path, PathStrokeType(radiusStep / 13.f));
    }

    void drawCircleGrid(Graphics& g, float radius, int subdivisions) {
        for(int i=0; i<subdivisions; i++)
        {
            drawArcUnit(g, radius, subdivisions, i);
        }
    }
    
    void drawArcBit(Graphics& g, float radius, int subdivisions, int index, Colour colour) {
        Path path;
        path.addCentredArc(getWidth() / 2.f,
                           getHeight() / 2.f,
                           radius, radius,
                           (MathConstants<float>::twoPi / subdivisions) * (index - .5f),
                           MathConstants<float>::twoPi / (subdivisions * 8),
                           MathConstants<float>::twoPi / subdivisions - MathConstants<float>::twoPi / (subdivisions * 8),
                           true);  // startAsNewSubPath
        g.setColour(colour);
        g.strokePath(path, PathStrokeType(radiusStep / 4.f, PathStrokeType::curved, PathStrokeType::rounded));
    }
    
    void drawHatBits(Graphics& g) {
        for(int i=0; i<hatSubdivisions; i++)
        {
            if (doubled && !triplet) {
                if (!doubledArr[0][i]) continue;
            } else if (doubled && triplet) {
                if (!doubledTripletArr[i]) continue;
            } else if (!doubled && triplet) {
                if (!tripletArr[i]) continue;
            } else if (!doubled && !triplet) {
                if (!timeArr[0][i]) continue;
            }
            drawArcBit(g, hatRadius, hatSubdivisions, i, Colours::yellow);
        }
        
        for(int i=0; i<snareSubdivisions; i++)
        {
            if (doubled) {
                if (!doubledArr[1][i]) continue;
            } else {
                if (!timeArr[1][i]) continue;
            }
            drawArcBit(g, snareRadius, snareSubdivisions, i, Colours::cyan);
        }
        
        for(int i=0; i<kickSubdivisions; i++)
        {
            if (doubled) {
                if (!doubledArr[2][i]) continue;
            } else {
                if (!timeArr[2][i]) continue;
            }
            drawArcBit(g, kickRadius, kickSubdivisions, i, Colours::magenta);
        }
    }
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};

class davulApplication  : public JUCEApplication
{
public:
    davulApplication() {}
    
    const String getApplicationName() override       { return ProjectInfo::projectName; }
    const String getApplicationVersion() override    { return ProjectInfo::versionString; }
    bool moreThanOneInstanceAllowed() override       { return true; }
    
    void initialise(const String& commandLine) override
    {
        // application's initialisation code..
        mainWindow.reset(new MainWindow(getApplicationName()));
    }
    
    void shutdown() override
    {
        // application's shutdown code here..
        mainWindow = nullptr; //(deletes our window)
    }
    
    void systemRequestedQuit() override
    {
        // This is called when the app is being asked to quit: you can ignore this
        // request and let the app carry on running, or call quit() to allow the app to close.
        quit();
    }
    
    void anotherInstanceStarted(const String& commandLine) override
    {
        // When another instance of the app is launched while this one is running,
        // this method is invoked, and the commandLine parameter tells you what
        // the other instance's command-line arguments were.
    }
    
    class MainWindow    : public DocumentWindow
    {
    public:
        MainWindow(String name) :
        DocumentWindow(name,
                       Desktop::getInstance().getDefaultLookAndFeel().findColour(ResizableWindow::backgroundColourId),
                       DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar(true);
            setContentOwned(new MainComponent(), true);
            
#if JUCE_IOS || JUCE_ANDROID
            setFullScreen(true);
#else
            setResizable(true, true);
            centreWithSize(getWidth(), getHeight());
#endif
            
            setVisible(true);
        }
        
        void closeButtonPressed() override
        {
            JUCEApplication::getInstance()->systemRequestedQuit();
        }
        
    private:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
    };
    
private:
    std::unique_ptr<MainWindow> mainWindow;
};

// This macro generates the main() routine that launches the app.
START_JUCE_APPLICATION(davulApplication)
