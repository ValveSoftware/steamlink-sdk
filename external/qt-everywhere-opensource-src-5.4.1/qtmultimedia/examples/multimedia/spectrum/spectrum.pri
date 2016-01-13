# The following macros allow certain features and debugging output
# to be disabled / enabled at compile time.

# Debug output from spectrum calculation
DEFINES += LOG_SPECTRUMANALYSER

# Debug output from waveform generation
#DEFINES += LOG_WAVEFORM

# Debug output from engine
DEFINES += LOG_ENGINE

# Dump input data to spectrum analyer, plus artefact data files
#DEFINES += DUMP_SPECTRUMANALYSER

# Dump captured audio data
#DEFINES += DUMP_CAPTURED_AUDIO

# Disable calculation of level
#DEFINES += DISABLE_LEVEL

# Disable calculation of frequency spectrum
# If this macro is defined, the FFTReal DLL will not be built
#DEFINES += DISABLE_FFT

static: DEFINES += DISABLE_FFT

# Disables rendering of the waveform
#DEFINES += DISABLE_WAVEFORM

# If defined, superimpose the progress bar on the waveform
DEFINES += SUPERIMPOSE_PROGRESS_ON_WAVEFORM

# Perform spectrum analysis calculation in a separate thread
DEFINES += SPECTRUM_ANALYSER_SEPARATE_THREAD

# Suppress warnings about strncpy potentially being unsafe, emitted by MSVC
win32: DEFINES += _CRT_SECURE_NO_WARNINGS

win32 {
    # spectrum_build_dir is defined with a leading slash so that it can
    # be used in contexts such as
    #     ..$${spectrum_build_dir}
    # without the result having a trailing slash where spectrum_build_dir
    # is undefined.
    build_pass {
        CONFIG(release, release|debug): spectrum_build_dir = /release
        CONFIG(debug, release|debug): spectrum_build_dir = /debug
    }
}

