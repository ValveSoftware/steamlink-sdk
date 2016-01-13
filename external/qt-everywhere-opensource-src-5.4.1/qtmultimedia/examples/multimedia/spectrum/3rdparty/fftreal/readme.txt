==============================================================================

        FFTReal
        Version 2.00, 2005/10/18

        Fourier transformation (FFT, IFFT) library specialised for real data
        Portable ISO C++

        (c) Laurent de Soras <laurent.de.soras@club-internet.fr>
        Object Pascal port (c) Frederic Vanmol <frederic@fruityloops.com>

==============================================================================



1. Legal
--------

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

Check the file license.txt to get full information about the license.



2. Content
----------

FFTReal is a library to compute Discrete Fourier Transforms (DFT) with the
FFT algorithm (Fast Fourier Transform) on arrays of real numbers. It can
also compute the inverse transform.

You should find in this package a lot of files ; some of them are of interest:
- readme.txt: you are reading it
- FFTReal.h: FFT, length fixed at run-time
- FFTRealFixLen.h: FFT, length fixed at compile-time
- FFTReal.pas: Pascal implementation (working but not up-to-date)
- stopwatch directory



3. Using FFTReal
----------------

Important - if you were using older versions of FFTReal (up to 1.03), some
things have changed. FFTReal is now a template. Therefore use FFTReal<float>
or FFTReal<double> in your code depending on the application datatype. The
flt_t typedef has been removed.

You have two ways to use FFTReal. In the first way, the FFT has its length
fixed at run-time, when the object is instanciated. It means that you have
not to know the length when you write the code. This is the usual way of
proceeding.


3.1 FFTReal - Length fixed at run-time
--------------------------------------

Just instanciate one time a FFTReal object. Specify the data type you want
as template parameter (only floating point: float, double, long double or
custom type). The constructor precompute a lot of things, so it may be a bit
long. The parameter is the number of points used for the next FFTs. It must
be a power of 2:

   #include "FFTReal.h"
   ...
   long len = 1024;
   ...
   FFTReal <float> fft_object (len);   // 1024-point FFT object constructed.

Then you can use this object to compute as many FFTs and IFFTs as you want.
They will be computed very quickly because a lot of work has been done in the
object construction.

   float x [1024];
   float f [1024];

   ...
   fft_object.do_fft (f, x);     // x (real) --FFT---> f (complex)
   ...
   fft_object.do_ifft (f, x);    // f (complex) --IFFT--> x (real)
   fft_object.rescale (x);       // Post-scaling should be done after FFT+IFFT
   ...

x [] and f [] are floating point number arrays. x [] is the real number
sequence which we want to compute the FFT. f [] is the result, in the
"frequency" domain. f has the same number of elements as x [], but f []
elements are complex numbers. The routine uses some FFT properties to
optimize memory and to reduce calculations: the transformaton of a real
number sequence is a conjugate complex number sequence: F [k] = F [-k]*.


3.2 FFTRealFixLen - Length fixed at compile-time
------------------------------------------------

This class is significantly faster than the previous one, giving a speed
gain between 50 and 100 %. The template parameter is the base-2 logarithm of
the FFT length. The datatype is float; it can be changed by modifying the
DataType typedef in FFTRealFixLenParam.h. As FFTReal class, it supports
only floating-point types or equivalent.

To instanciate the object, just proceed as below:

   #include "FFTRealFixLen.h"
   ...
   FFTRealFixLen <10> fft_object; // 1024-point (2^10) FFT object constructed.

Use is similar as the one of FFTReal.


3.3 Data organisation
---------------------

Mathematically speaking, the formulas below show what does FFTReal:

do_fft() : f(k) = sum (p = 0, N-1, x(p) * exp (+j*2*pi*k*p/N))
do_ifft(): x(k) = sum (p = 0, N-1, f(p) * exp (-j*2*pi*k*p/N))

Where j is the square root of -1. The formulas differ only by the sign of
the exponential. When the sign is positive, the transform is called positive.
Common formulas for Fourier transform are negative for the direct tranform and
positive for the inverse one.

However in these formulas, f is an array of complex numbers and doesn't
correspound exactly to the f[] array taken as function parameter. The
following table shows how the f[] sequence is mapped onto the usable FFT
coefficients (called bins):

   FFTReal output | Positive FFT equiv.   | Negative FFT equiv.
   ---------------+-----------------------+-----------------------
   f [0]          | Real (bin 0)          | Real (bin 0)
   f [...]        | Real (bin ...)        | Real (bin ...)
   f [length/2]   | Real (bin length/2)   | Real (bin length/2)
   f [length/2+1] | Imag (bin 1)          | -Imag (bin 1)
   f [...]        | Imag (bin ...)        | -Imag (bin ...)
   f [length-1]   | Imag (bin length/2-1) | -Imag (bin length/2-1)

And FFT bins are distributed in f [] as above:

               |                | Positive FFT    | Negative FFT
   Bin         | Real part      | imaginary part  | imaginary part
   ------------+----------------+-----------------+---------------
   0           | f [0]          | 0               | 0
   1           | f [1]          | f [length/2+1]  | -f [length/2+1]
   ...         | f [...],       | f [...]         | -f [...]
   length/2-1  | f [length/2-1] | f [length-1]    | -f [length-1]
   length/2    | f [length/2]   | 0               | 0
   length/2+1  | f [length/2-1] | -f [length-1]   | f [length-1]
   ...         | f [...]        | -f [...]        | f [...]
   length-1    | f [1]          | -f [length/2+1] | f [length/2+1]

f [] coefficients have the same layout for FFT and IFFT functions. You may
notice that scaling must be done if you want to retrieve x after FFT and IFFT.
Actually, IFFT (FFT (x)) = x * length(x). This is a not a problem because
most of the applications don't care about absolute values. Thus, the operation
requires less calculation. If you want to use the FFT and IFFT to transform a
signal, you have to apply post- (or pre-) processing yourself. Multiplying
or dividing floating point numbers by a power of 2 doesn't generate extra
computation noise.



4. Compilation and testing
--------------------------

Drop the following files into your project or makefile:

Array.*
def.h
DynArray.*
FFTReal*.cpp
FFTReal*.h*
OscSinCos.*

Other files are for testing purpose only, do not include them if you just need
to use the library ; they are not needed to use FFTReal in your own programs.

FFTReal may be compiled in two versions: release and debug. Debug version
has checks that could slow down the code. Define NDEBUG to set the Release
mode. For example, the command line to compile the test bench on GCC would
look like:

Debug mode:
g++ -Wall -o fftreal_debug.exe *.cpp stopwatch/*.cpp

Release mode:
g++ -Wall -o fftreal_release.exe -DNDEBUG -O3 *.cpp stopwatch/*.cpp

It may be tricky to compile the test bench because the speed tests use the
stopwatch sub-library, which is not that cross-platform. If you encounter
any problem that you cannot easily fix while compiling it, edit the file
test_settings.h and un-define the speed test macro. Remove the stopwatch
directory from your source file list, too.

If it's not done by default, you should activate the exception handling
of your compiler to get the class memory-leak-safe. Thus, when a memory
allocation fails (in the constructor), an exception is thrown and the entire
object is safely destructed. It reduces the permanent error checking overhead
in the client code. Also, the test bench requires Run-Time Type Information
(RTTI) to be enabled in order to display the names of the tested classes -
sometimes mangled, depending on the compiler.

The test bench may take a long time to compile, especially in Release mode,
because a lot of recursive templates are instanciated.



5. History
----------

v2.00 (2005.10.18)
- Turned FFTReal class into template (data type as parameter)
- Added FFTRealFixLen
- Trigonometric tables are size-limited in order to preserve cache memory;
over a given size, sin/cos functions are computed on the fly.
- Better test bench for accuracy and speed

v1.03 (2001.06.15)
- Thanks to Frederic Vanmol for the Pascal port (works with Delphi).
- Documentation improvement

v1.02 (2001.03.25)
- sqrt() is now precomputed when the object FFTReal is constructed, resulting
in speed impovement for small size FFT.

v1.01 (2000)
- Small modifications, I don't remember what.

v1.00 (1999.08.14)
- First version released

