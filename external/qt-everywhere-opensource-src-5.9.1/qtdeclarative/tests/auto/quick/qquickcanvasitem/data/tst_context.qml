
import QtQuick 2.0
import QtTest 1.1

Canvas {
    id: canvas
    width: 1
    height: 1
    contextType: "2d"

    property var contextInPaint

    SignalSpy {
        id: paintedSpy
        target: canvas
        signalName: "paint"
    }

    SignalSpy {
        id: contextSpy
        target: canvas
        signalName: "contextChanged"
    }

    onPaint: {
        contextInPaint = context;
    }

    TestCase {
        name: "ContextTypeStored"
        when: windowShown

        function test_contextType() {
            compare(canvas.contextType, "2d");
        }
    }

    TestCase {
        name: "ContextValidWhenTypePredefined"
        when: canvas.available

        function test_context() {
            // Wait for the context to become active
            wait(100);
            compare(contextSpy.count, 1);

            // Context is available
            verify(canvas.context)
        }

        function test_contextIsConsistent() {
            // Wait for the context to become active
            wait(100);
            compare(contextSpy.count, 1);

            // getContext("2d") is the same as the context property
            compare(canvas.getContext("2d"), canvas.context);
        }

        function test_paintHadContext() {
            // Make there was a paint signal
            wait(100);
            verify(paintedSpy.count, 1)

            // Paint was called with a valid context when contextType is
            // specified
            verify(canvas.contextInPaint)

            // paints context was the correct one
            compare(canvas.contextInPaint, canvas.getContext("2d"));
        }
   }

    // See: http://www.w3.org/TR/css3-fonts/#font-prop
    TestCase {
        name: "ContextFontValidation"
        when: canvas.available

        function test_pixelSize() {
            wait(100);
            compare(contextSpy.count, 1);

            var ctx = canvas.getContext("2d");
            compare(ctx.font, "sans-serif,-1,10,5,50,0,0,0,0,0");

            ctx.font = "80.1px cursive";
            // Can't verify the chosen font family since it's different for each platform.
            compare(ctx.font.substr(ctx.font.indexOf(",") + 1), "-1,80,5,50,0,0,0,0,0");
        }

        function test_valid() {
            wait(100);
            compare(contextSpy.count, 1);

            var ctx = canvas.getContext("2d");

            var validFonts = [
                { string: "12px sans-serif", expected: "sans-serif,-1,12,5,50,0,0,0,0,0" },
                { string: "12px serif", expected: "serif,-1,12,5,50,0,0,0,0,0" },
                { string: "12pt sans-serif", expected: "sans-serif,12,-1,5,50,0,0,0,0,0" },
                { string: "12pt serif", expected: "serif,12,-1,5,50,0,0,0,0,0" },
                { string: "normal 12px sans-serif", expected: "sans-serif,-1,12,5,50,0,0,0,0,0" },
                { string: "normal normal 12px sans-serif", expected: "sans-serif,-1,12,5,50,0,0,0,0,0" },
                { string: "normal normal normal 12px sans-serif", expected: "sans-serif,-1,12,5,50,0,0,0,0,0" },
                { string: "italic 12px sans-serif", expected: "sans-serif,-1,12,5,50,1,0,0,0,0" },
                { string: "italic normal 12px sans-serif", expected: "sans-serif,-1,12,5,50,1,0,0,0,0" },
                { string: "italic normal normal 12px sans-serif", expected: "sans-serif,-1,12,5,50,1,0,0,0,0" },
                { string: "oblique 12px sans-serif", expected: "sans-serif,-1,12,5,50,2,0,0,0,0" },
                { string: "oblique normal 12px sans-serif", expected: "sans-serif,-1,12,5,50,2,0,0,0,0" },
                { string: "oblique normal normal 12px sans-serif", expected: "sans-serif,-1,12,5,50,2,0,0,0,0" },
                { string: "bold 12px sans-serif", expected: "sans-serif,-1,12,5,75,0,0,0,0,0" },
                { string: "0 12px sans-serif", expected: "sans-serif,-1,12,5,0,0,0,0,0,0" },
                { string: "small-caps 12px sans-serif", expected: "sans-serif,-1,12,5,50,0,0,0,0,0" },
                { string: "12px \"sans-serif\"", expected: "sans-serif,-1,12,5,50,0,0,0,0,0" },
                { string: "12px 'sans-serif'", expected: "sans-serif,-1,12,5,50,0,0,0,0,0" },
                // sans-serif will always be chosen, but this still tests that multiple families can be read.
                { string: "12px 'sans-serif' 'cursive'", expected: "sans-serif,-1,12,5,50,0,0,0,0,0" },
                { string: "12px sans-serif 'cursive' monospace", expected: "sans-serif,-1,12,5,50,0,0,0,0,0" },
                { string: "12px sans-serif 'cursive' monospace    ", expected: "sans-serif,-1,12,5,50,0,0,0,0,0" },
                { string: "    12px sans-serif 'cursive' monospace    ", expected: "sans-serif,-1,12,5,50,0,0,0,0,0" },
                { string: "    12px    sans-serif    'cursive'    monospace    ", expected: "sans-serif,-1,12,5,50,0,0,0,0,0" }
            ];
            for (var i = 0; i < validFonts.length; ++i) {
                ctx.font = validFonts[i].string;
                compare(ctx.font.substr(ctx.font.indexOf(",") + 1),
                    validFonts[i].expected.substr(validFonts[i].expected.indexOf(",") + 1));
            }
        }

        function test_invalid() {
            wait(100);
            compare(contextSpy.count, 1);

            var ctx = canvas.getContext("2d");
            var originalFont = ctx.font;

            var fontStrings = [
                "",
                "12px",
                "sans-serif",
                "z12px sans-serif",
                "1z2px sans-serif",
                "12zpx sans-serif",
                "12pxz sans-serif",
                "sans-serif 12px",
                "12px !@weeeeeeee!@!@Don'tNameYourFontThis",
                "12px )(&*^^^%#$@*!!@#$JSPOR)",
                "normal normal normal normal 12px sans-serif",
                "normal normal bold bold 12px sans-serif",
                "bold bold 12px sans-serif",
                "12px 'cursive\"",
                "12px 'cursive\" sans-serif",
                "12px 'cursive"
            ];

            var ignoredWarnings = [
                "Context2D: Font string is empty.",
                "Context2D: Missing or misplaced font family in font string (it must come after the font size).",
                "Context2D: Invalid font size unit in font string.",
                "Context2D: A font size of \"z12\" is invalid.",
                "Context2D: A font size of \"1z2\" is invalid.",
                "Context2D: A font size of \"12z\" is invalid.",
                "Context2D: Invalid font size unit in font string.",
                "Context2D: Missing or misplaced font family in font string (it must come after the font size).",
                "Context2D: Unclosed quote in font string.",
                "Context2D: The font families specified are invalid: )(&*^^^%#$@*!!@#$JSPOR)",
                "Context2D: Duplicate token \"normal\" found in font string.",
                "Context2D: Duplicate token \"bold\" found in font string.",
                "Context2D: Duplicate token \"bold\" found in font string.",
                "Context2D: Mismatched quote in font string.",
                "Context2D: Mismatched quote in font string.",
                "Context2D: Unclosed quote in font string."
            ];

            // Sanity check...
            compare(ignoredWarnings.length, fontStrings.length);

            for (var i = 0; i < fontStrings.length; ++i) {
                ignoreWarning(ignoredWarnings[i]);
                ctx.font = fontStrings[i];
                compare(ctx.font, originalFont);
            }
        }
    }

    TestCase {
        name: "Colors"
        when: canvas.available

        function test_colors() {
            wait(100);
            compare(contextSpy.count, 1);

            var ctx = canvas.getContext("2d");
            // QTBUG-47894
            ctx.strokeStyle = 'hsl(255, 100%, 50%)';
            var c1 = ctx.strokeStyle.toString();
            ctx.strokeStyle = 'hsl(320, 100%, 50%)';
            var c2 = ctx.strokeStyle.toString();
            verify(c1 !== c2);
        }
    }
}
