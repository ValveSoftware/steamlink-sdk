(*****************************************************************************

        DIGITAL SIGNAL PROCESSING TOOLS
        Version 1.03, 2001/06/15
        (c) 1999 - Laurent de Soras

        FFTReal.h
        Fourier transformation of real number arrays.
        Portable ISO C++

------------------------------------------------------------------------------

	LEGAL

	Source code may be freely used for any purpose, including commercial
	applications. Programs must display in their "About" dialog-box (or
	documentation) a text telling they use these routines by Laurent de Soras.
	Modified source code can be distributed, but modifications must be clearly
	indicated.

	CONTACT

	Laurent de Soras
	92 avenue Albert 1er
	92500 Rueil-Malmaison
	France

	ldesoras@club-internet.fr

------------------------------------------------------------------------------

        Translation to ObjectPascal by :
          Frederic Vanmol
          frederic@axiworld.be

*****************************************************************************)


unit
    FFTReal;

interface

uses
    Windows;

(* Change this typedef to use a different floating point type in your FFTs
	(i.e. float, double or long double). *)
type
    pflt_t = ^flt_t;
    flt_t = single;

    pflt_array = ^flt_array;
    flt_array = array[0..0] of flt_t;

    plongarray = ^longarray;
    longarray = array[0..0] of longint;

const
     sizeof_flt : longint = SizeOf(flt_t);



type
    // Bit reversed look-up table nested class
    TBitReversedLUT = class
    private
      _ptr : plongint;
    public
      constructor Create(const nbr_bits: integer);
      destructor Destroy; override;
      function get_ptr: plongint;
    end;

    // Trigonometric look-up table nested class
    TTrigoLUT = class
    private
      _ptr : pflt_t;
    public
      constructor Create(const nbr_bits: integer);
      destructor Destroy; override;
      function get_ptr(const level: integer): pflt_t;
    end;

    TFFTReal = class
    private
      _bit_rev_lut  : TBitReversedLUT;
      _trigo_lut    : TTrigoLUT;
      _sqrt2_2      : flt_t;
      _length       : longint;
      _nbr_bits     : integer;
      _buffer_ptr   : pflt_t;
    public
      constructor Create(const length: longint);
      destructor Destroy; override;

      procedure do_fft(f: pflt_array; const x: pflt_array);
      procedure do_ifft(const f: pflt_array; x: pflt_array);
      procedure rescale(x: pflt_array);
    end;







implementation

uses
    Math;

{ TBitReversedLUT }

constructor TBitReversedLUT.Create(const nbr_bits: integer);
var
   length   : longint;
   cnt      : longint;
   br_index : longint;
   bit      : longint;
begin
  inherited Create;

  length := 1 shl nbr_bits;
  GetMem(_ptr, length*SizeOf(longint));

  br_index := 0;
  plongarray(_ptr)^[0] := 0;
  for cnt := 1 to length-1 do
  begin
    // ++br_index (bit reversed)
    bit := length shr 1;
    br_index := br_index xor bit;
    while br_index and bit = 0 do
    begin
      bit := bit shr 1;
      br_index := br_index xor bit;
    end;

    plongarray(_ptr)^[cnt] := br_index;
  end;
end;

destructor TBitReversedLUT.Destroy;
begin
  FreeMem(_ptr);
  _ptr := nil;
  inherited;
end;

function TBitReversedLUT.get_ptr: plongint;
begin
  Result := _ptr;
end;

{ TTrigLUT }

constructor TTrigoLUT.Create(const nbr_bits: integer);
var
   total_len : longint;
   PI        : double;
   level     : integer;
   level_len : longint;
   level_ptr : pflt_array;
   mul       : double;
   i         : longint;
begin
  inherited Create;

  _ptr := nil;

  if (nbr_bits > 3) then
  begin
    total_len := (1 shl (nbr_bits - 1)) - 4;
    GetMem(_ptr, total_len * sizeof_flt);

    PI := ArcTan(1) * 4;
    for level := 3 to nbr_bits-1 do
    begin
      level_len := 1 shl (level - 1);
      level_ptr := pointer(get_ptr(level));
      mul := PI / (level_len shl 1);

      for i := 0 to level_len-1 do
        level_ptr^[i] := cos(i * mul);
    end;
  end;
end;

destructor TTrigoLUT.Destroy;
begin
  FreeMem(_ptr);
  _ptr := nil;
  inherited;
end;

function TTrigoLUT.get_ptr(const level: integer): pflt_t;
var
   tempp : pflt_t;
begin
  tempp := _ptr;
  inc(tempp, (1 shl (level-1)) - 4);
  Result := tempp;
end;

{ TFFTReal }

constructor TFFTReal.Create(const length: longint);
begin
  inherited Create;

  _length := length;
  _nbr_bits := Floor(Ln(length) / Ln(2) + 0.5);
  _bit_rev_lut := TBitReversedLUT.Create(Floor(Ln(length) / Ln(2) + 0.5));
  _trigo_lut := TTrigoLUT.Create(Floor(Ln(length) / Ln(2) + 0.05));
  _sqrt2_2 := Sqrt(2) * 0.5;

  _buffer_ptr := nil;
  if _nbr_bits > 2 then
    GetMem(_buffer_ptr, _length * sizeof_flt);
end;

destructor TFFTReal.Destroy;
begin
  if _buffer_ptr <> nil then
  begin
    FreeMem(_buffer_ptr);
    _buffer_ptr := nil;
  end;

  _bit_rev_lut.Free;
  _bit_rev_lut := nil;
  _trigo_lut.Free;
  _trigo_lut := nil;

  inherited;       
end;

(*==========================================================================*/
/*      Name: do_fft                                                        */
/*      Description: Compute the FFT of the array.                          */
/*      Input parameters:                                                   */
/*        - x: pointer on the source array (time).                          */
/*      Output parameters:                                                  */
/*        - f: pointer on the destination array (frequencies).              */
/*             f [0...length(x)/2] = real values,                           */
/*             f [length(x)/2+1...length(x)-1] = imaginary values of        */
/*               coefficents 1...length(x)/2-1.                             */
/*==========================================================================*)
procedure TFFTReal.do_fft(f: pflt_array; const x: pflt_array);
var
   sf, df     : pflt_array;
   pass       : integer;
   nbr_coef   : longint;
   h_nbr_coef : longint;
   d_nbr_coef : longint;
   coef_index : longint;
   bit_rev_lut_ptr : plongarray;
   rev_index_0 : longint;
   rev_index_1 : longint;
   rev_index_2 : longint;
   rev_index_3 : longint;
   df2         : pflt_array;
   n1, n2, n3  : integer;
   sf_0, sf_2  : flt_t;
   sqrt2_2     : flt_t;
   v           : flt_t;
   cos_ptr     : pflt_array;
   i           : longint;
   sf1r, sf2r  : pflt_array;
   dfr, dfi    : pflt_array;
   sf1i, sf2i  : pflt_array;
   c, s        : flt_t;
   temp_ptr    : pflt_array;
   b_0, b_2    : flt_t;
begin
  n1 := 1;
  n2 := 2;
  n3 := 3;

  (*______________________________________________
   *
   * General case
   *______________________________________________
   *)

  if _nbr_bits > 2 then
  begin
    if _nbr_bits and 1 <> 0 then
    begin
      df := pointer(_buffer_ptr);
      sf := f;
    end
    else
    begin
      df := f;
      sf := pointer(_buffer_ptr);
    end;

    //
    // Do the transformation in several passes
    //

    // First and second pass at once
    bit_rev_lut_ptr := pointer(_bit_rev_lut.get_ptr);
    coef_index := 0;

    repeat
      rev_index_0 := bit_rev_lut_ptr^[coef_index];
      rev_index_1 := bit_rev_lut_ptr^[coef_index + 1];
      rev_index_2 := bit_rev_lut_ptr^[coef_index + 2];
      rev_index_3 := bit_rev_lut_ptr^[coef_index + 3];

      df2 := pointer(longint(df) + (coef_index*sizeof_flt));
      df2^[n1] := x^[rev_index_0] - x^[rev_index_1];
      df2^[n3] := x^[rev_index_2] - x^[rev_index_3];

      sf_0 := x^[rev_index_0] + x^[rev_index_1];
      sf_2 := x^[rev_index_2] + x^[rev_index_3];

      df2^[0] := sf_0 + sf_2;
      df2^[n2] := sf_0 - sf_2;

      inc(coef_index, 4);
    until (coef_index >= _length);


    // Third pass
    coef_index := 0;
    sqrt2_2 := _sqrt2_2;

    repeat
      sf^[coef_index] := df^[coef_index] + df^[coef_index + 4];
      sf^[coef_index + 4] := df^[coef_index] - df^[coef_index + 4];
      sf^[coef_index + 2] := df^[coef_index + 2];
      sf^[coef_index + 6] := df^[coef_index + 6];

      v := (df [coef_index + 5] - df^[coef_index + 7]) * sqrt2_2;
      sf^[coef_index + 1] := df^[coef_index + 1] + v;
      sf^[coef_index + 3] := df^[coef_index + 1] - v;

      v := (df^[coef_index + 5] + df^[coef_index + 7]) * sqrt2_2;
      sf [coef_index + 5] := v + df^[coef_index + 3];
      sf [coef_index + 7] := v - df^[coef_index + 3];

      inc(coef_index, 8);
    until (coef_index >= _length);


    // Next pass
    for pass := 3 to _nbr_bits-1 do
    begin
      coef_index := 0;
      nbr_coef := 1 shl pass;
      h_nbr_coef := nbr_coef shr 1;
      d_nbr_coef := nbr_coef shl 1;

      cos_ptr := pointer(_trigo_lut.get_ptr(pass));
      repeat
        sf1r := pointer(longint(sf) + (coef_index * sizeof_flt));
        sf2r := pointer(longint(sf1r) + (nbr_coef * sizeof_flt));
        dfr := pointer(longint(df) + (coef_index * sizeof_flt));
        dfi := pointer(longint(dfr) + (nbr_coef * sizeof_flt));

        // Extreme coefficients are always real
        dfr^[0] := sf1r^[0] + sf2r^[0];
        dfi^[0] := sf1r^[0] - sf2r^[0];   // dfr [nbr_coef] =
        dfr^[h_nbr_coef] := sf1r^[h_nbr_coef];
        dfi^[h_nbr_coef] := sf2r^[h_nbr_coef];

        // Others are conjugate complex numbers
        sf1i := pointer(longint(sf1r) + (h_nbr_coef * sizeof_flt));
        sf2i := pointer(longint(sf1i) + (nbr_coef * sizeof_flt));

        for i := 1 to h_nbr_coef-1 do
        begin
          c := cos_ptr^[i];               // cos (i*PI/nbr_coef);
          s := cos_ptr^[h_nbr_coef - i];  // sin (i*PI/nbr_coef);

          v := sf2r^[i] * c - sf2i^[i] * s;
          dfr^[i] := sf1r^[i] + v;
          dfi^[-i] := sf1r^[i] - v;	// dfr [nbr_coef - i] =

          v := sf2r^[i] * s + sf2i^[i] * c;
          dfi^[i] := v + sf1i^[i];
          dfi^[nbr_coef - i] := v - sf1i^[i];
        end;

        inc(coef_index, d_nbr_coef);
      until (coef_index >= _length);

      // Prepare to the next pass
      temp_ptr := df;
      df := sf;
      sf := temp_ptr;
    end;
  end

  (*______________________________________________
   *
   * Special cases
   *______________________________________________
   *)

  // 4-point FFT
  else if _nbr_bits = 2 then
  begin
    f^[n1] := x^[0] - x^[n2];
    f^[n3] := x^[n1] - x^[n3];

    b_0 := x^[0] + x^[n2];
    b_2 := x^[n1] + x^[n3];

    f^[0] := b_0 + b_2;
    f^[n2] := b_0 - b_2;
  end

  // 2-point FFT
  else if _nbr_bits = 1 then
  begin
    f^[0] := x^[0] + x^[n1];
    f^[n1] := x^[0] - x^[n1];
  end

  // 1-point FFT
  else
    f^[0] := x^[0];
end;


(*==========================================================================*/
/*      Name: do_ifft                                                       */
/*      Description: Compute the inverse FFT of the array. Notice that      */
/*                   IFFT (FFT (x)) = x * length (x). Data must be          */
/*                   post-scaled.                                           */
/*      Input parameters:                                                   */
/*        - f: pointer on the source array (frequencies).                   */
/*             f [0...length(x)/2] = real values,                           */
/*             f [length(x)/2+1...length(x)-1] = imaginary values of        */
/*               coefficents 1...length(x)/2-1.                             */
/*      Output parameters:                                                  */
/*        - x: pointer on the destination array (time).                     */
/*==========================================================================*)
procedure TFFTReal.do_ifft(const f: pflt_array; x: pflt_array);
var
   n1, n2, n3      : integer;
   n4, n5, n6, n7  : integer;
   sf, df, df_temp : pflt_array;
   pass            : integer;
   nbr_coef        : longint;
   h_nbr_coef      : longint;
   d_nbr_coef      : longint;
   coef_index      : longint;
   cos_ptr         : pflt_array;
   i               : longint;
   sfr, sfi        : pflt_array;
   df1r, df2r      : pflt_array;
   df1i, df2i      : pflt_array;
   c, s, vr, vi    : flt_t;
   temp_ptr        : pflt_array;
   sqrt2_2         : flt_t;
   bit_rev_lut_ptr : plongarray;
   sf2             : pflt_array;
   b_0, b_1, b_2, b_3 : flt_t;
begin
  n1 := 1;
  n2 := 2;
  n3 := 3;
  n4 := 4;
  n5 := 5;
  n6 := 6;
  n7 := 7;

  (*______________________________________________
   *
   * General case
   *______________________________________________
   *)

  if _nbr_bits > 2 then
  begin
    sf := f;

    if _nbr_bits and 1 <> 0 then
    begin
      df := pointer(_buffer_ptr);
      df_temp := x;
    end
    else
    begin
      df := x;
      df_temp := pointer(_buffer_ptr);
    end;

    // Do the transformation in several pass

    // First pass
    for pass := _nbr_bits-1 downto 3 do
    begin
      coef_index := 0;
      nbr_coef := 1 shl pass;
      h_nbr_coef := nbr_coef shr 1;
      d_nbr_coef := nbr_coef shl 1;

      cos_ptr := pointer(_trigo_lut.get_ptr(pass));

      repeat
        sfr := pointer(longint(sf) + (coef_index*sizeof_flt));
        sfi := pointer(longint(sfr) + (nbr_coef*sizeof_flt));
        df1r := pointer(longint(df) + (coef_index*sizeof_flt));
        df2r := pointer(longint(df1r) + (nbr_coef*sizeof_flt));

        // Extreme coefficients are always real
        df1r^[0] := sfr^[0] + sfi^[0];		// + sfr [nbr_coef]
        df2r^[0] := sfr^[0] - sfi^[0];		// - sfr [nbr_coef]
        df1r^[h_nbr_coef] := sfr^[h_nbr_coef] * 2;
        df2r^[h_nbr_coef] := sfi^[h_nbr_coef] * 2;

        // Others are conjugate complex numbers
        df1i := pointer(longint(df1r) + (h_nbr_coef*sizeof_flt));
        df2i := pointer(longint(df1i) + (nbr_coef*sizeof_flt));

        for i := 1 to h_nbr_coef-1 do
        begin
          df1r^[i] := sfr^[i] + sfi^[-i];		// + sfr [nbr_coef - i]
          df1i^[i] := sfi^[i] - sfi^[nbr_coef - i];

          c := cos_ptr^[i];					// cos (i*PI/nbr_coef);
          s := cos_ptr^[h_nbr_coef - i];	// sin (i*PI/nbr_coef);
          vr := sfr^[i] - sfi^[-i];		// - sfr [nbr_coef - i]
          vi := sfi^[i] + sfi^[nbr_coef - i];

          df2r^[i] := vr * c + vi * s;
          df2i^[i] := vi * c - vr * s;
        end;

        inc(coef_index, d_nbr_coef);
      until (coef_index >= _length);


      // Prepare to the next pass 
      if (pass < _nbr_bits - 1) then
      begin
        temp_ptr := df;
        df := sf;
        sf := temp_ptr;
      end
      else
      begin
        sf := df;
        df := df_temp;
      end
    end;

    // Antepenultimate pass
    sqrt2_2 := _sqrt2_2;
    coef_index := 0;

    repeat
      df^[coef_index] := sf^[coef_index] + sf^[coef_index + 4];
      df^[coef_index + 4] := sf^[coef_index] - sf^[coef_index + 4];
      df^[coef_index + 2] := sf^[coef_index + 2] * 2;
      df^[coef_index + 6] := sf^[coef_index + 6] * 2;

      df^[coef_index + 1] := sf^[coef_index + 1] + sf^[coef_index + 3];
      df^[coef_index + 3] := sf^[coef_index + 5] - sf^[coef_index + 7];

      vr := sf^[coef_index + 1] - sf^[coef_index + 3];
      vi := sf^[coef_index + 5] + sf^[coef_index + 7];

      df^[coef_index + 5] := (vr + vi) * sqrt2_2;
      df^[coef_index + 7] := (vi - vr) * sqrt2_2;

      inc(coef_index, 8);
    until (coef_index >= _length);


    // Penultimate and last pass at once 
    coef_index := 0;
    bit_rev_lut_ptr := pointer(_bit_rev_lut.get_ptr);
    sf2 := df;

    repeat
      b_0 := sf2^[0] + sf2^[n2];
      b_2 := sf2^[0] - sf2^[n2];
      b_1 := sf2^[n1] * 2;
      b_3 := sf2^[n3] * 2;

      x^[bit_rev_lut_ptr^[0]] := b_0 + b_1;
      x^[bit_rev_lut_ptr^[n1]] := b_0 - b_1;
      x^[bit_rev_lut_ptr^[n2]] := b_2 + b_3;
      x^[bit_rev_lut_ptr^[n3]] := b_2 - b_3;

      b_0 := sf2^[n4] + sf2^[n6];
      b_2 := sf2^[n4] - sf2^[n6];
      b_1 := sf2^[n5] * 2;
      b_3 := sf2^[n7] * 2;

      x^[bit_rev_lut_ptr^[n4]] := b_0 + b_1;
      x^[bit_rev_lut_ptr^[n5]] := b_0 - b_1;
      x^[bit_rev_lut_ptr^[n6]] := b_2 + b_3;
      x^[bit_rev_lut_ptr^[n7]] := b_2 - b_3;

      inc(sf2, 8);
      inc(coef_index, 8);
      inc(bit_rev_lut_ptr, 8);
    until (coef_index >= _length);
  end

  (*______________________________________________
   *
   * Special cases
   *______________________________________________
   *)

  // 4-point IFFT
  else if _nbr_bits = 2 then
  begin
    b_0 := f^[0] + f [n2];
    b_2 := f^[0] - f [n2];

    x^[0] := b_0 + f [n1] * 2;
    x^[n2] := b_0 - f [n1] * 2;
    x^[n1] := b_2 + f [n3] * 2;
    x^[n3] := b_2 - f [n3] * 2;
  end

  // 2-point IFFT
  else if _nbr_bits = 1 then
  begin
    x^[0] := f^[0] + f^[n1];
    x^[n1] := f^[0] - f^[n1];
  end

  // 1-point IFFT
  else
    x^[0] := f^[0];
end;

(*==========================================================================*/
/*      Name: rescale                                                       */
/*      Description: Scale an array by divide each element by its length.   */
/*                   This function should be called after FFT + IFFT.       */
/*      Input/Output parameters:                                            */
/*        - x: pointer on array to rescale (time or frequency).             */
/*==========================================================================*)
procedure TFFTReal.rescale(x: pflt_array);
var
   mul  : flt_t;
   i    : longint;
begin
  mul := 1.0 / _length;
  i := _length - 1;

  repeat
    x^[i] := x^[i] * mul;
    dec(i);
  until (i < 0);
end;

end.
