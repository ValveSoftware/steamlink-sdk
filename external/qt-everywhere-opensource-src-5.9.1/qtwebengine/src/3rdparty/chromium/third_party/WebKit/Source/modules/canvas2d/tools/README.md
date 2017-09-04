Performance Analysis
===

This directory contains tools that support a data driven approach to improve the 2D canvas graphics rendering performance. They have 2 main purposes:
1. Improve our understandind of the 2D canvas graphics rendering performance. This better understanding can yield new performance optimization ideas.
2. Help design and calibrate heuristics to determine the optimal rendering strategies at run time.

Performance Data Generation
---

GeneratePerformanceData.html measures the graphics rendering performance for a series of frames involving various 2D canvas operations in varying quantities, scales and shadow blur levels, including:
 - Drawing various types of shapes with multiple different fill types.
 - Drawing SVG and PNG images.
 - Calling getImagedata and putImageData.

After recording performance for all the frames, it generates a CSV string that includes the performance (in frames per second and time per frame) and description of every frame measured.

