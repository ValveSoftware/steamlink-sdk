program testapp;
{$APPTYPE CONSOLE}
uses
  SysUtils,
  fftreal in 'fftreal.pas',
  Math,
  Windows;

var
   nbr_points  : longint;
   x, f        : pflt_array;
   fft         : TFFTReal;
   i           : longint;
   PI          : double;
   areal, img  : double;
   f_abs       : double;
   buffer_size : longint;
   nbr_tests   : longint;
   time0, time1, time2 : int64;
   timereso    : int64;
   offset      : longint;
   t0, t1      : double;
   nbr_s_chn   : longint;
   tempp1, tempp2 : pflt_array;

begin
  (*______________________________________________
   *
   * Exactness test
   *______________________________________________
   *)

  WriteLn('Accuracy test:');
  WriteLn;

  nbr_points := 16;       // Power of 2
  GetMem(x, nbr_points * sizeof_flt);
  GetMem(f, nbr_points * sizeof_flt);
  fft := TFFTReal.Create(nbr_points);    // FFT object initialized here

  // Test signal
  PI := ArcTan(1) * 4;
  for i := 0 to nbr_points-1 do
  begin
    x^[i] := -1 + sin (3*2*PI*i/nbr_points)
                + cos (5*2*PI*i/nbr_points) * 2
                - sin (7*2*PI*i/nbr_points) * 3
                + cos (8*2*PI*i/nbr_points) * 5;
  end;

  // Compute FFT and IFFT
  fft.do_fft(f, x);
  fft.do_ifft(f, x);
  fft.rescale(x);

  // Display the result
  WriteLn('FFT:');
  for i := 0 to nbr_points div 2 do
  begin
    areal := f^[i];
    if (i > 0) and (i < nbr_points div 2) then
      img := f^[i + nbr_points div 2]
    else
      img := 0;

    f_abs := Sqrt(areal * areal + img * img);
    WriteLn(Format('%5d: %12.6f %12.6f (%12.6f)', [i, areal, img, f_abs]));
  end;

  WriteLn;
  WriteLn('IFFT:');
  for i := 0 to nbr_points-1 do
    WriteLn(Format('%5d: %f', [i, x^[i]]));

  WriteLn;

  FreeMem(x);
  FreeMem(f);
  fft.Free;


  (*______________________________________________
   *
   * Speed test
   *______________________________________________
   *)

  WriteLn('Speed test:');
  WriteLn('Please wait...');
  WriteLn;

  nbr_points := 1024;	          // Power of 2
  buffer_size := 256*nbr_points;  // Number of flt_t (float or double)
  nbr_tests := 10000;

  assert(nbr_points <= buffer_size);
  GetMem(x, buffer_size * sizeof_flt);
  GetMem(f, buffer_size * sizeof_flt);
  fft := TFFTReal.Create(nbr_points);					// FFT object initialized here

  // Test signal: noise
  for i := 0 to nbr_points-1 do
    x^[i] := Random($7fff) - ($7fff shr 1);

  // timing
  QueryPerformanceFrequency(timereso);
  QueryPerformanceCounter(time0);

  for i := 0 to nbr_tests-1 do
  begin
    offset := (i * nbr_points) and (buffer_size - 1);
    tempp1 := f;
    inc(tempp1, offset);
    tempp2 := x;
    inc(tempp2, offset);
    fft.do_fft(tempp1, tempp2);
  end;

  QueryPerformanceCounter(time1);

  for i := 0 to nbr_tests-1 do
  begin
    offset := (i * nbr_points) and (buffer_size - 1);
    tempp1 := f;
    inc(tempp1, offset);
    tempp2 := x;
    inc(tempp2, offset);
    fft.do_ifft(tempp1, tempp2);
    fft.rescale(x);
  end;

  QueryPerformanceCounter(time2);

  t0 := ((time1-time0) / timereso) / nbr_tests;
  t1 := ((time2-time1) / timereso) / nbr_tests;

  WriteLn(Format('%d-points FFT           : %.0f us.', [nbr_points, t0 * 1000000]));
  WriteLn(Format('%d-points IFFT + scaling: %.0f us.', [nbr_points, t1 * 1000000]));

  nbr_s_chn := Floor(nbr_points / ((t0 + t1) * 44100 * 2));
  WriteLn(Format('Peak performance: FFT+IFFT on %d mono channels at 44.1 KHz (with overlapping)', [nbr_s_chn]));
  WriteLn;

  FreeMem(x);
  FreeMem(f);
  fft.Free;

  WriteLn('Press [Return] key to terminate...');
  ReadLn;
end.
