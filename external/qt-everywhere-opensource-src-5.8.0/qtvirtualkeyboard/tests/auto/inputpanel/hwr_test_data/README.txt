This directory contains the tools and the data for generating
JavaScript code from the Unipen files. The Unipen data is useful
for testing handwriting recognition input methods. Since the
JavaScript does not have file io, the data must be provided in
JavaScript format.

The build_unipen_data.py is a Python script that scans the directories
for Unipen files, combines them into a data structure and prints
the output as the JavaScript code. The produced JavaScript code
can be imported in the testscript and used for emulating handwritten
characters.

The Unipen data can be collected using the virtual keyboard.
To enable the data collection mode, the virtual keyboard must
be compiled with a special qmake parameters:

CONFIG+=lipi-toolkit CONFIG+=record-trace-input

The first option enables the handwriting recognition engine and
the second option enables the data collection mode.

In data collection mode, the handwriting input for a character
is saved into a file. To eliminate bad input, the text case of
the handwriting input must match with the current text case of
the handwriting keyboard.

The collected traces are saved into VIRTUAL_KEYBOARD_TRACES folder in
the current users home directory. The file names of the collected
traces are constructed as follows:

<unicode>_<confidence>_<index>.txt

Where the unicode is the Unicode value of the recognized character
in decimal format, the confidence is the confidence value in the
scale from 0 to 100 (with 100 being the highest confidence) and
the index is the number of overlapping files in the directory.

After collecting the desired amount of trace samples, the Unipen
files are copied (manually) into this directory and supplied to the
build_unipen_data.py script. For example:

./build_unipen_data.py alphanumeric > unipen_data.js

The generated JavaScript file, unipen_data.js in this case, contains
all the data combined in the input files. This file can now be used
in the input panel tests as a common character database to emulate
handwriting recognition in test cases. The database can be copied to

tests/auto/inputpanel/data/inputpanel/unipen_data.js

and re-run the tests. It should be noted that the character database
should contain at least the characters that are tested.
