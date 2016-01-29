#ifndef _discrete_h_
#define _discrete_h_

/************************************************************************/
/*                                                                      */
/*  MAME - Discrete sound system emulation library                      */
/*                                                                      */
/*  Written by Keith Wilkins (mame@esplexo.co.uk)                       */
/*                                                                      */
/*  (c) K.Wilkins 2000                                                  */
/*                                                                      */
/*  Coding started in November 2000                                     */
/*                                                                      */
/*  Additions/bugfix February 2003 - Sawtooth, Switch, Adjusters        */
/*                                   LFSR noise source                  */
/*                                                                      */
/************************************************************************/
/*                                                                      */
/* Unused/Unconnected input nodes should be set to NODE_NC (No Connect) */
/*                                                                      */
/* Each node can have upto 6 inputs from either constants or other      */
/* nodes within the system.                                             */
/*                                                                      */
/* It should be remembered that the discrete sound system emulation     */
/* does not do individual device emulation, but instead does a function */
/* emulation. So you will need to convert the schematic design into     */
/* a logic block representation.                                        */
/*                                                                      */
/* One node point may feed a number of inputs, for example you could    */
/* connect the output of a DISCRETE_SINEWAVE to the AMPLITUDE input     */
/* of another DISCRETE_SINEWAVE to amplitude modulate its output and    */
/* also connect it to the frequecy input of another to frequency        */
/* modulate its output, the combinations are endless....                */
/*                                                                      */
/* Consider the circuit below:                                          */
/*                                                                      */
/*   --------               ----------                   -------        */
/*  |        |             |          |                 |       |       */
/*  | SQUARE |       Enable| SINEWAVE |                 |       |       */
/*  | WAVE   |------------>|  2000Hz  |---------------->|       |       */
/*  |        | |           |          |                 | ADDER |--}OUT */
/*  | NODE01 | |           |  NODE02  |                 |       |       */
/*   --------  |            ----------                ->|       |       */
/*             |                                     |  |NODE06 |       */
/*             |   ------     ------     ---------   |   -------        */
/*             |  |      |   |      |   |         |  |       ^          */
/*             |  | INV  |Ena| ADD  |Ena| SINEWVE |  |       |          */
/*              ->| ERT  |-->| ER2  |-->| 4000Hz  |--    -------        */
/*                |      |ble|      |ble|         |     |       |       */
/*                |NODE03|   |NODE04|   | NODE05  |     | INPUT |       */
/*                 ------     ------     ---------      |       |       */
/*                                                      |NODE07 |       */
/*                                                       -------        */
/*                                                                      */
/* This should give you an alternating two tone sound switching         */
/* between the 2000Hz and 4000Hz sine waves at the frequency of the     */
/* square wave, with the memory mapped enable signal mapped onto NODE07 */
/* so discrete_sound_w(NODE_06,1) will enable the sound, and            */
/* discrete_sound_w(NODE_06,0) will disable the sound.                  */
/*                                                                      */
/*  DISCRETE_SOUND_START(test_interface)                                */
/*      DISCRETE_SQUAREWAVE(NODE_01,1,0.5,1,50,0)                       */
/*      DISCRETE_SINEWAVE(NODE_02,NODE_01,2000,10000,0)                 */
/*      DISCRETE_INVERT(NODE_03,NODE_01)                                */
/*      DISCRETE_ADDER2(NODE_04,1,NODE_03,1)                            */
/*      DISCRETE_SINEWAVE(NODE_05,NODE_04,4000,10000,0)                 */
/*      DISCRETE_ADDER2(NODE_06,NODE_07,NODE_02,NODE_05)                */
/*      DISCRETE_INPUT(NODE_07,1)                                       */
/*      DISCRETE_OUTPUT(NODE_06)                                        */
/*  DISCRETE_SOUND_END                                                  */
/*                                                                      */
/* To aid simulation speed it is preferable to use the enable/disable   */
/* inputs to a block rather than setting the output amplitude to zero   */
/*                                                                      */
/* Feedback loops are allowed BUT they will always feeback one time     */
/* step later, the loop over the netlist is only performed once per     */
/* deltaT so feedback occurs in the next deltaT step. This is not       */
/* the perfect solution but saves repeatedly traversing the netlist     */
/* until all nodes have settled.                                        */
/*                                                                      */
/* It will often be necessary to add gain and level shifting blocks to  */
/* correct the signal amplitude and offset to the required level        */
/* as all sine waves have 0 offset and swing +ve/-ve so if you want     */
/* to use this as a frequency modulation you need to offset it so that  */
/* it always stays positive, use the DISCRETE_ADDER & DISCRETE_GAIN     */
/* modules to do this. You will also need to do this to scale and off-  */
/* set your memory mapped device inputs if they are being used to set   */
/* amplitude and frequency values.                                      */
/*                                                                      */
/* The best way to work out your system is generally to use a pen and   */
/* paper to draw a logical block diagram like the one above, it helps   */
/* to understand the system ,map the inputs and outputs and to work     */
/* out your node numbering scheme.                                      */
/*                                                                      */
/* Node numbers NODE_01 to NODE_99 are defined at present.              */
/*                                                                      */
/************************************************************************/
/*                                                                      */
/* LIST OF CURRENTLY IMPLEMENTED DISCRETE BLOCKS                        */
/* ---------------------------------------------                        */
/*                                                                      */
/* DISCRETE_SOUND_START(STRUCTURENAME)                                  */
/* DISCRETE_SOUND_END                                                   */
/*                                                                      */
/* DISCRETE_CONSTANT(NODE,CONST0)                                       */
/* DISCRETE_ADJUSTMENT(NODE,ENAB,MIN,MAX,DEFAULT,NAME)                  */
/* DISCRETE_INPUT(NODE,INIT0,ADDR,MASK)                                 */
/* DISCRETE_SQUAREWAVE(NODE,ENAB,FREQ,AMP,DUTY,PHASE)                   */
/* DISCRETE_SINEWAVE(NODE,ENAB,FREQ,AMP,PHASE)                          */
/* DISCRETE_TRIANGLEWAVE(NODE,ENAB,FREQ,AMP,PHASE)                      */
/* DISCRETE_SAWTOOTHWAVE(NODE,ENAB,FREQ,AMP,GRADIENT,PHASE)             */
/* DISCRETE_NOISE(NODE,ENAB,FREQ,AMP)                                   */
/* DISCRETE_LFSR_NOISE(NODE,ENAB,FREQ,AMP,LFSRDESC)                     */
/*                                                                      */
/* DISCRETE_INVERT(NODE,IN0)                                            */
/* DISCRETE_MULTIPLY(NODE,ENAB,IN0,IN1)                                 */
/* DISCRETE_DIVIDE(NODE,ENAB,IN0,IN1)                                   */
/* DISCRETE_GAIN(NODE,IN0,GAIN)                                         */
/* DISCRETE_ONOFF(NODE,IN0,IN1)                                         */
/* DISCRETE_ADDER2(NODE,IN0,IN1,IN2)                                    */
/* DISCRETE_ADDER3(NODE,IN0,IN1,IN2,IN3)                                */
/* DISCRETE_SWITCH(NODE,ENAB,SWITCH,INP0,INP1)                          */
/* DISCRETE_ONESHOTR(NODE,ENAB,TRIG,AMP,WIDTH) - Retriggerable          */
/* DISCRETE_ONESHOT(NODE,ENAB,TRIG,RESET,AMP,WIDTH) - Non retriggerable */
/*                                                                      */
/* DISCRETE_RCFILTER(NODE,ENAB,IN0,RVAL,CVAL)                           */
/* DISCRETE_RCDISC(NODE,ENAB,IN0,RVAL,CVAL)                             */
/* DISCRETE_RCDISC2(NODE,IN0,RVAL0,IN1,RVAL1,CVAL)                      */
/* DISCRETE_RAMP(NODE,ENAB,RAMP,GRAD,MIN,MAX,CLAMP)                     */
/* DISCRETE_CLAMP(NODE,ENAB,IN0,MIN,MAX,CLAMP)                          */
/* DISCRETE_LADDER(NODE,ENAB,IN0,GAIN,LADDER)                           */
/* DISCRETE_SAMPLHOLD(NODE,ENAB,INP0,CLOCK,CLKTYPE)                     */
/*                                                                      */
/* DISCRETE_LOGIC_INVERT(NODE,ENAB,INP0)                                */
/* DISCRETE_LOGIC_AND(NODE,ENAB,INP0,INP1)                              */
/* DISCRETE_LOGIC_AND3(NODE,ENAB,INP0,INP1,INP2)                        */
/* DISCRETE_LOGIC_AND4(NODE,ENAB,INP0,INP1,INP2,INP3)                   */
/* DISCRETE_LOGIC_NAND(NODE,ENAB,INP0,INP1)                             */
/* DISCRETE_LOGIC_NAND3(NODE,ENAB,INP0,INP1,INP2)                       */
/* DISCRETE_LOGIC_NAND4(NODE,ENAB,INP0,INP1,INP2,INP3)                  */
/* DISCRETE_LOGIC_OR(NODE,ENAB,INP0,INP1)                               */
/* DISCRETE_LOGIC_OR3(NODE,ENAB,INP0,INP1,INP2)                         */
/* DISCRETE_LOGIC_OR4(NODE,ENAB,INP0,INP1,INP2,INP3)                    */
/* DISCRETE_LOGIC_NOR(NODE,ENAB,INP0,INP1)                              */
/* DISCRETE_LOGIC_NOR3(NODE,ENAB,INP0,INP1,INP2)                        */
/* DISCRETE_LOGIC_NOR4(NODE,ENAB,INP0,INP1,INP2,INP3)                   */
/* DISCRETE_LOGIC_XOR(NODE,ENAB,INP0,INP1)                              */
/* DISCRETE_LOGIC_NXOR(NODE,ENAB,INP0,INP1)                             */
/*                                                                      */
/* DISCRETE_NE555(NODE,RESET,TRIGR,THRSH,CTRLV,VCC)                     */
/*                                                                      */
/* DISCRETE_OUTPUT(OPNODE)                                              */
/* DISCRETE_OUTPUT_STEREO(OPNODEL,OPNODER)                              */
/*                                                                      */
/************************************************************************/
/*                                                                      */
/* DISCRETE_CONSTANT - Single output, fixed at compile time             */
/*                                                                      */
/*                         ----------                                   */
/*                        |          |                                  */
/*                        | CONSTANT |--------}   Netlist node          */
/*                        |          |                                  */
/*                         ----------                                   */
/*  Declaration syntax                                                  */
/*                                                                      */
/*     DISCRETE_CONSTANT(name of node, constant value)                  */
/*                                                                      */
/*  Example config line                                                 */
/*                                                                      */
/*     DISCRETE_CONSTANT(NODE_01, 100)                                  */
/*                                                                      */
/*  Define a node that has a constant value of 100                      */
/*                                                                      */
/************************************************************************/
/*                                                                      */
/* DISCRETE_ADJUSTMENT - Adjustable constant nodempile time             */
/*                                                                      */
/*                         ----------                                   */
/*                        |          |                                  */
/*                        | ADJUST.. |--------}   Netlist node          */
/*                        |          |                                  */
/*                         ----------                                   */
/*  Declaration syntax                                                  */
/*                                                                      */
/*     DISCRETE_ADJUSTMENT(name of node,                                */
/*                         enable node or static value                  */
/*                         static minimum value the node can take       */
/*                         static maximum value the node can take       */
/*                         default static value for the node            */
/*                         log/linear scale 0=Linear !0=Logarithmic     */
/*                         ascii name of the node for UI)               */
/*                                                                      */
/*  Example config line                                                 */
/*                                                                      */
/*     DISCRETE_ADJUSTMENT(NODE_01,1.0,0.0,5.0,2.5,0,"VCO Trimmer")     */
/*                                                                      */
/*  Define an adjustment slider that has a default value of 2.5 and     */
/*  can be adjusted between 0.0 and 5.0 via the user interface.         */
/*  Adujstment scaling is Linear.                                       */
/*                                                                      */
/************************************************************************/
/*                                                                      */
/* DISCRETE INPUT - Single output node, initialised at compile time and */
/*                variable via memory based interface                   */
/*                                                                      */
/*                             ----------                               */
/*                      -----\|          |                              */
/*        Memory Mapped ===== | INPUT(A) |----}   Netlist node          */
/*            Write     -----/|          |                              */
/*                             ----------                               */
/*                                                                      */
/*  Declaration syntax                                                  */
/*                                                                      */
/*     DISCRETE_INPUT(name of node, initial value, addr, addrmask)      */
/*                                                                      */
/*  Example config line                                                 */
/*                                                                      */
/*     DISCRETE_INPUT(NODE_02, 100,0x0000,0x000f)                       */
/*                                                                      */
/*  Define a memory mapped input node called NODE_02 that has an        */
/*  initial value of 100. It is memory mapped into location 0x0000,     */
/*  0x0010, 0x0020 etc all the way to 0x0ff0. The exact size of memory  */
/*  space is defined by DSS_INPUT_SPACE in file disc_inp.c              */
/*                                                                      */
/*  The incoming address is first logicalled AND with the the ADDRMASK  */
/*  and then compared to the address, if there is a match on the write  */
/*  then the node is written to.                                        */
/*                                                                      */
/*  The memory space for discrete sound is 4096 locations (0x1000)      */
/*  the addr/addrmask values are used to setup a lookup table.          */
/*                                                                      */
/*  Can be written to with:    discrete_sound_w(0x0000, data);          */
/*                                                                      */
/************************************************************************/
/*                                                                      */
/* DISCRETE_SQUAREWAVE - Squarewave waveform generator node, has four   */
/*                       input nodes FREQUENCY, AMPLITUDE, ENABLE and   */
/*                       DUTY if node is not connected it will default  */
/*                       to the initialised value.                      */
/*                                                                      */
/*                         ------------                                 */
/*                        |            |                                */
/*    ENABLE     -0------}|            |                                */
/*                        |            |                                */
/*    FREQUENCY  -1------}|            |                                */
/*                        |            |                                */
/*    AMPLITUDE  -2------}| SQUAREWAVE |----}   Netlist node            */
/*                        |            |                                */
/*    DUTY CYCLE -3------}|            |                                */
/*                        |            |                                */
/*    BIAS       -4------}|            |                                */
/*                        |            |                                */
/*    PHASE      -5------}|            |                                */
/*                        |            |                                */
/*                         ------------                                 */
/*                                                                      */
/*  Declaration syntax                                                  */
/*                                                                      */
/*     DISCRETE_SQUAREWAVE(name of node,                                */
/*                         enable node or static value                  */
/*                         frequency node or static value               */
/*                         amplitude node or static value               */
/*                         duty cycle node or static value              */
/*                         dc bias value for waveform                   */
/*                         starting phase value in degrees)             */
/*                                                                      */
/*  Example config line                                                 */
/*                                                                      */
/*     DISCRETE_SQUAREWAVE(NODE_03,NODE_01,NODE_02,0,100,50,90)         */
/*                                                                      */
/************************************************************************/
/*                                                                      */
/* DISCRETE_SINEWAVE   - Sinewave waveform generator node, has four     */
/*                       input nodes FREQUENCY, AMPLITUDE, ENABLE and   */
/*                       PHASE, if a node is not connected it will      */
/*                       default to the initialised value in the macro  */
/*                                                                      */
/*                         ------------                                 */
/*                        |            |                                */
/*    ENABLE     -0------}|            |                                */
/*                        |            |                                */
/*    FREQUENCY  -1------}|            |                                */
/*                        | SINEWAVE   |----}   Netlist node            */
/*    AMPLITUDE  -2------}|            |                                */
/*                        |            |                                */
/*    BIAS       -3------}|            |                                */
/*                        |            |                                */
/*    PHASE      -4------}|            |                                */
/*                        |            |                                */
/*                         ------------                                 */
/*                                                                      */
/*  Declaration syntax                                                  */
/*                                                                      */
/*     DISCRETE_SINEWAVE  (name of node,                                */
/*                         enable node or static value                  */
/*                         frequency node or static value               */
/*                         amplitude node or static value               */
/*                         dc bias value for waveform                   */
/*                         starting phase value in degrees)             */
/*                                                                      */
/*  Example config line                                                 */
/*                                                                      */
/*     DISCRETE_SINEWAVE(NODE_03,NODE_01,NODE_02,10000,90)              */
/*                                                                      */
/************************************************************************/
/*                                                                      */
/* DISCRETE_TRIANGLEW  - Triagular waveform generator, generates        */
/*                       equal ramp up/down at chosen frequency         */
/*                                                                      */
/*                         ------------                                 */
/*                        |            |                                */
/*    ENABLE     -0------}|            |                                */
/*                        |            |                                */
/*    FREQUENCY  -1------}|  TRIANGLE  |----}   Netlist node            */
/*                        |    WAVE    |                                */
/*    AMPLITUDE  -2------}|            |                                */
/*                        |            |                                */
/*    BIAS       -3------}|            |                                */
/*                        |            |                                */
/*    PHASE      -4------}|            |                                */
/*                        |            |                                */
/*                         ------------                                 */
/*                                                                      */
/*  Declaration syntax                                                  */
/*                                                                      */
/*     DISCRETE_TRIANGLEWAVE(name of node,                              */
/*                         enable node or static value                  */
/*                         frequency node or static value               */
/*                         amplitude node or static value               */
/*                         dc bias value for waveform                   */
/*                         starting phase value in degrees)             */
/*                                                                      */
/*  Example config line                                                 */
/*                                                                      */
/*     DISCRETE_TRIANGLEWAVE(NODE_03,1,5000,NODE_01)                    */
/*                                                                      */
/************************************************************************/
/*                                                                      */
/* DISCRETE_SAWTOOTHWAVE - Saw tooth shape waveform generator, rapid    */
/*                         rise and then graduated fall                 */
/*                                                                      */
/*                         ------------                                 */
/*                        |            |                                */
/*    ENABLE     -0------}|            |                                */
/*                        |            |                                */
/*    FREQUENCY  -1------}|            |                                */
/*                        |            |                                */
/*    AMPLITUDE  -2------}|  SAWTOOTH  |----} Netlist Node              */
/*                        |    WAVE    |                                */
/*    BIAS       -3------}|            |                                */
/*                        |            |                                */
/*    GRADIENT   -4------}|            |                                */
/*                        |            |                                */
/*    PHASE      -5------}|            |                                */
/*                        |            |                                */
/*                         ------------                                 */
/*                                                                      */
/*  Declaration syntax                                                  */
/*                                                                      */
/*     DISCRETE_SAWTOOTHWAVE(name of node,                              */
/*                         enable node or static value                  */
/*                         frequency node or static value               */
/*                         amplitude node or static value               */
/*                         dc bias value for waveform                   */
/*                         gradient of wave ==0 //// !=0 \\\\           */
/*                         starting phase value in degrees)             */
/*                                                                      */
/*  Example config line                                                 */
/*                                                                      */
/*     DISCRETE_SAWTOOTHWAVE(NODE_03,1,5000,NODE_01,0,90)               */
/*                                                                      */
/************************************************************************/
/*                                                                      */
/* DISCRETE_NOISE      - Noise waveform generator node, generates       */
/*                       random noise at the chosen frequency.          */
/*                                                                      */
/*                         ------------                                 */
/*                        |            |                                */
/*    ENABLE     -0------}|            |                                */
/*                        |            |                                */
/*    FREQUENCY  -1------}|   NOISE    |----}   Netlist node            */
/*                        |            |                                */
/*    AMPLITUDE  -2------}|            |                                */
/*                        |            |                                */
/*    BIAS       -3------}|            |                                */
/*                        |            |                                */
/*                         ------------                                 */
/*                                                                      */
/*  Declaration syntax                                                  */
/*                                                                      */
/*     DISCRETE_NOISE     (name of node,                                */
/*                         enable node or static value                  */
/*                         frequency node or static value               */
/*                         amplitude node or static value)              */
/*                                                                      */
/*  Example config line                                                 */
/*                                                                      */
/*     DISCRETE_NOISE(NODE_03,1,5000,NODE_01)                           */
/*                                                                      */
/************************************************************************/
/*                                                                      */
/* DISCRETE_LFSR_NOISE - Noise waveform generator node, generates       */
/*                       psuedo random digital stream at the requested  */
/*                       clock frequency. Amplitude is 0/AMPLITUDE.     */
/*                                                                      */
/*                         ------------                                 */
/*                        |            |                                */
/*    ENABLE      -0-----}|            |                                */
/*                        |            |                                */
/*    RESET       -1-----}|            |                                */
/*                        |            |                                */
/*    FREQUENCY   -2-----}|   LFSR     |                                */
/*                        |   CYCLIC   |----}   Netlist Node            */
/*    AMPLITUDE   -3-----}|   NOISE    |                                */
/*                        |            |                                */
/*    FEED BIT    -4-----}|            |                                */
/*                        |            |                                */
/*    LFSR DESC   -5-----}|            |                                */
/*                        |            |                                */
/*                         ------------                                 */
/*                                                                      */
/*  Declaration syntax                                                  */
/*                                                                      */
/*     DISCRETE_LFSR_NOISE(name of node,                                */
/*                         enable node or static value                  */
/*                         reset node or static value                   */
/*                         frequency node or static value               */
/*                         amplitude node or static value               */
/*                         forced infeed bit to shift reg               */
/*                         LFSR noise descriptor structure)             */
/*                                                                      */
/*  Example config line                                                 */
/*                                                                      */
/*     DISCRETE_LFSR_NOISE(NODE_03,1,NODE_04,5000,NODE_01,0,{...})      */
/*                                                                      */
/*  The diagram below outlines the structure of the LFSR model.         */
/*                                                                      */
/*         .-------.                                                    */
/*   FEED  |       |                                                    */
/*   ----->|  F2   |<--------------------------------------------.      */
/*         |       |                                             |      */
/*         .-------.               BS - Bit Select               |      */
/*             |                   Fx - Programmable Function    |      */
/*             |        .-------.  PI - Programmable Inversion   |      */
/*             |        |       |                                |      */
/*             |  .---- | SR>>1 |<--------.                      |      */
/*             |  |     |       |         |                      |      */
/*             V  V     .-------.         |  .----               |      */
/*           .------.                     |->| BS |--. .------.  |      */
/*   BITMASK |      |    .-------------.  |  .----.  .-|      |  |      */
/*   ------->|  F3  |--->| Shift Reg   |--|            |  F1  |--.      */
/*           |      | |  .-------------.  |  .----.  .-|      |         */
/*           .------. |         ^         .->| BS |--. .------.         */
/*                    |         |            .----.                     */
/*   CLOCK            |     RESET VAL                                   */
/*   ---->            |                      .----.  .----.             */
/*                    .----------------------| BS |--| PI |----OUTPUT   */
/*                                           .----.  .----.             */
/*                                                                      */
/************************************************************************/
/*                                                                      */
/* DISCRETE_ADDER      - Node addition function, available in three     */
/*                       lovelly flavours, ADDER2,ADDER3,ADDER4         */
/*                       that perform a summation of incoming nodes     */
/*                                                                      */
/*                         ------------                                 */
/*                        |            |                                */
/*    INPUT0     -0------}|            |                                */
/*                        |            |                                */
/*    INPUT1     -1------}|     |      |                                */
/*                        |    -+-     |----}   Netlist node            */
/*    INPUT2     -2------}|     |      |                                */
/*                        |            |                                */
/*    INPUT3     -3------}|            |                                */
/*                        |            |                                */
/*                         ------------                                 */
/*                                                                      */
/*  Declaration syntax                                                  */
/*                                                                      */
/*     DISCRETE_ADDERx    (name of node,                                */
/*        (x=2/3/4)        enable node or static value                  */
/*                         input0 node or static value                  */
/*                         input1 node or static value                  */
/*                         input2 node or static value                  */
/*                         input3 node or static value)                 */
/*                                                                      */
/*  Example config line                                                 */
/*                                                                      */
/*     DISCRETE_ADDER2(NODE_03,1,NODE_12,-2000)                         */
/*                                                                      */
/*  Always enabled, subtracts 2000 from the output of NODE_12           */
/*                                                                      */
/************************************************************************/
/*                                                                      */
/* DISCRETE_GAIN       - Node multiplication function output is equal   */
/* DISCRETE_MULTIPLY     to INPUT0 * INPUT1                             */
/*                                                                      */
/*                         ------------                                 */
/*                        |            |                                */
/*    ENAB       -0------}|            |                                */
/*                        |    \|/     |                                */
/*    INPUT1     -1------}|    -+-     |----}   Netlist node            */
/*                        |    /|\     |                                */
/*    INPUT2     -2------}|            |                                */
/*                        |            |                                */
/*                         ------------                                 */
/*                                                                      */
/*  Declaration syntax                                                  */
/*                                                                      */
/*     DISCRETE_MULTIPLY  (name of node,                                */
/*                         enable node or static value                  */
/*                         input0 node or static value                  */
/*                         input1 node or static value)                 */
/*                                                                      */
/*     DISCRETE_GAIN      (name of node,                                */
/*                         input0 node or static value                  */
/*                         static value for gain)                       */
/*  Example config line                                                 */
/*                                                                      */
/*     DISCRETE_GAIN(NODE_03,NODE_12,112.0)                             */
/*                                                                      */
/*  Always enabled, multiplies the input NODE_12 by 112.0               */
/*                                                                      */
/************************************************************************/
/*                                                                      */
/* DISCRETE_DIVIDE     - Node dividion function                         */
/*                                                                      */
/*                         ------------                                 */
/*                        |            |                                */
/*    ENAB       -0------}|            |                                */
/*                        |     o      |                                */
/*    INPUT1     -1------}|    ---     |----}   Netlist node            */
/*                        |     o      |                                */
/*    INPUT2     -2------}|            |                                */
/*                        |            |                                */
/*                         ------------                                 */
/*                                                                      */
/*  Declaration syntax                                                  */
/*                                                                      */
/*     DISCRETE_DIVIDE    (name of node,                                */
/*                         enable node or static value                  */
/*                         input0 node or static value                  */
/*                         input1 node or static value)                 */
/*                                                                      */
/*  Example config line                                                 */
/*                                                                      */
/*     DISCRETE_DIVIDE(NODE_03,1.0,NODE_12,50.0)                        */
/*                                                                      */
/*  Always enabled, divides the input NODE_12 by 50.0. Note that a      */
/*  divide by zero condition will give a LARGE number output, it        */
/*  will not stall the machine or simulation. It will also attempt      */
/*  to write a divide by zero error to the Mame log if enabled.         */
/*                                                                      */
/************************************************************************/
/*                                                                      */
/* DISCRETE_SWITCH     - Node switch function, output node is switched  */
/*                       by switch input to take one node/contst or     */
/*                       other. Can be nodes or constants.              */
/*                                                                      */
/*    SWITCH     -0--------------.                                      */
/*                               V                                      */
/*                         ------------                                 */
/*                        |      |     |                                */
/*    INPUT0     -1------}|----o       |                                */
/*                        |       .--- |----}   Netlist node            */
/*    INPUT1     -2------}|----o /     |                                */
/*                        |            |                                */
/*                         ------------                                 */
/*                                                                      */
/*  Declaration syntax                                                  */
/*                                                                      */
/*     DISCRETE_SWITCH    (name of node,                                */
/*        (x=2/3/4)        enable node or static value                  */
/*                         switch node or static value                  */
/*                         input0 node or static value                  */
/*                         input1 node or static value)                 */
/*                                                                      */
/*  Example config line                                                 */
/*                                                                      */
/*     DISCRETE_SWITCH(NODE_03,1,NODE_10,NODE_90,5.0)                   */
/*                                                                      */
/*  Always enabled, NODE_10 switches output to be either NODE_90 or     */
/*  constant value 5.0. Switch==0 inp0=output else inp1=output          */
/*                                                                      */
/************************************************************************/
/*                                                                      */
/* DISCRETE_RCFILTER - Simple single pole RC filter network             */
/*                                                                      */
/*                         ------------                                 */
/*                        |            |                                */
/*    ENAB       -0------}| RC FILTER  |                                */
/*                        |            |                                */
/*    INPUT1     -1------}| -ZZZZ----  |                                */
/*                        |   R   |    |----}   Netlist node            */
/*    RVAL       -2------}|      ---   |                                */
/*                        |      ---C  |                                */
/*    CVAL       -3------}|       |    |                                */
/*                        |            |                                */
/*                         ------------                                 */
/*                                                                      */
/*  Declaration syntax                                                  */
/*                                                                      */
/*     DISCRETE_RCFILTER(name of node,                                  */
/*                       enable                                         */
/*                       input node (or value)                          */
/*                       resistor value in OHMS                         */
/*                       capacitor value in FARADS)                     */
/*                                                                      */
/*  Example config line                                                 */
/*                                                                      */
/*     DISCRETE_RCFILTER(NODE_11,1,NODE_10,100,1e-6)                    */
/*                                                                      */
/*  Defines an always enabled RC filter with a 100R & 1uF network       */
/*  the input is fed from NODE_10.                                      */
/*                                                                      */
/*  This can be also thought of as a low pass filter with a 3dB cutoff  */
/*  at:                                                                 */
/*                                  1                                   */
/*            Fcuttoff =      --------------                            */
/*                            2*Pi*RVAL*CVAL                            */
/*                                                                      */
/*  (3dB cutoff is where the output power has dropped by 3dB ie Half)   */
/*                                                                      */
/************************************************************************/
/*                                                                      */
/* DISCRETE_RCDISC - Simple single pole RC discharge network            */
/*                                                                      */
/*                         ------------                                 */
/*                        |            |                                */
/*    ENAB       -0------}| RC         |                                */
/*                        |            |                                */
/*    INPUT1     -1------}| -ZZZZ----  |                                */
/*                        |   R   |    |----}   Netlist node            */
/*    RVAL       -2------}|      ---   |                                */
/*                        |      ---C  |                                */
/*    CVAL       -3------}|       |    |                                */
/*                        |            |                                */
/*                         ------------                                 */
/*                                                                      */
/*  Declaration syntax                                                  */
/*                                                                      */
/*     DISCRETE_RCFILTER(name of node,                                  */
/*                       enable                                         */
/*                       input node (or value)                          */
/*                       resistor value in OHMS                         */
/*                       capacitor value in FARADS)                     */
/*                                                                      */
/*  Example config line                                                 */
/*                                                                      */
/*     DISCRETE_RCDISC(NODE_11,NODE_10,10,100,1e-6)                     */
/*                                                                      */
/*  When enabled by NODE_10, C dischanges from 10v as indicated by RC   */
/*  of 100R & 1uF.                                                      */
/*                                                                      */
/************************************************************************/
/*                                                                      */
/* DISCRETE_RCDIS2C  - Switched input RC discharge network              */
/*                                                                      */
/*                         ------------                                 */
/*                        |            |                                */
/*    SWITCH     -0------}| IP0 | IP1  |                                */
/*                        |            |                                */
/*    INPUT0     -1------}| -ZZZZ-.    |                                */
/*                        |   R0  |    |                                */
/*    RVAL0      -2------}|       |    |                                */
/*                        |       |    |                                */
/*    INPUT1     -3------}| -ZZZZ----  |                                */
/*                        |   R1  |    |----}   Netlist node            */
/*    RVAL1      -4------}|      ---   |                                */
/*                        |      ---C  |                                */
/*    CVAL       -5------}|       |    |                                */
/*                        |            |                                */
/*                         ------------                                 */
/*                                                                      */
/*  Declaration syntax                                                  */
/*                                                                      */
/*      DISCRETE_RCDISC2(name of node,                                  */
/*                       switch                                         */
/*                       input0 node (or value)                         */
/*                       resistor0 value in OHMS                        */
/*                       input1 node (or value)                         */
/*                       resistor1 value in OHMS                        */
/*                       capacitor value in FARADS)                     */
/*                                                                      */
/*  Example config line                                                 */
/*                                                                      */
/*     DISCRETE_RCDISC(NODE_9,NODE_10,10.0,100,0.0,100,1e-6)            */
/*                                                                      */
/*  When switched by NODE_10, C charges/dischanges from 10v/0v          */
/*  as dicated by RC0 & RC1 combos respectively                         */
/*  of 100R & 1uF.                                                      */
/*                                                                      */
/************************************************************************/
/*                                                                      */
/* DISCRETE_RAMP - Ramp up/down circuit with clamps & reset             */
/*                                                                      */
/*                         ------------                                 */
/*                        |            |                                */
/*    ENAB       -0------}| FREE/CLAMP |                                */
/*                        |            |                                */
/*    RAMP       -1------}| Up/Down    |                                */
/*                        |            |                                */
/*    GRAD       -2------}| Grad/sec   |                                */
/*                        |            |----}   Netlist node            */
/*    MIN        -3------}| Start clamp|                                */
/*                        |            |                                */
/*    MAX        -4------}| End clamp  |                                */
/*                        |            |                                */
/*    CLAMP      -5------}| off clamp  |                                */
/*                        |            |                                */
/*                         ------------                                 */
/*                                                                      */
/*  Declaration syntax                                                  */
/*                                                                      */
/*         DISCRETE_RAMP(name of node,                                  */
/*                       enable                                         */
/*                       ramp forward/reverse node (or value)           */
/*                       gradient node (or static value)                */
/*                       minimum node or static value                   */
/*                       maximum node or static value                   */
/*                       clamp node or static value when disabled)      */
/*                                                                      */
/*  Example config line                                                 */
/*                                                                      */
/*     DISCRETE_RAMP(NODE_9,NODE_10,NODE_11,10.0,-10.0,10.0,0)          */
/*                                                                      */
/*  Node10 when not zero will allow ramp to operate, when 0 then output */
/*  is clamped to clamp value specified. Node11 ramp when 0 then +ve    */
/*  gradient, not zero equals -ve gradient. Output is clamped to max-   */
/*  min values. Gradient is specified in change/second.                 */
/*                                                                      */
/************************************************************************/
/*                                                                      */
/* DISCRETE_CLAMP - Force a signal to stay within bounds MIN/MAX        */
/*                                                                      */
/*                         ------------                                 */
/*                        |            |                                */
/*    ENAB       -0------}|            |                                */
/*                        |            |                                */
/*    INP0       -1------}|            |                                */
/*                        |            |                                */
/*    MIN        -2------}|   CLAMP    |----}   Netlist node            */
/*                        |            |                                */
/*    MAX        -3------}|            |                                */
/*                        |            |                                */
/*    CLAMP      -4------}|            |                                */
/*                        |            |                                */
/*                         ------------                                 */
/*                                                                      */
/*  Declaration syntax                                                  */
/*                                                                      */
/*        DISCRETE_CLAMP(name of node,                                  */
/*                       enable                                         */
/*                       input node                                     */
/*                       minimum node or static value                   */
/*                       maximum node or static value                   */
/*                       clamp node or static value when disabled)      */
/*                                                                      */
/*  Example config line                                                 */
/*                                                                      */
/*     DISCRETE_CLAMP(NODE_9,NODE_10,NODE_11,2.0,10.0,5.0)              */
/*                                                                      */
/*  Node10 when not zero will allow clamp to operate forcing the value  */
/*  on the node output, to be within the MIN/MAX boundard. When enable  */
/*  is set to zero the node will output the clamp value                 */
/*                                                                      */
/************************************************************************/
/*                                                                      */
/* DISCRETE_SAMPHOLD - Sample & Hold circuit                            */
/*                                                                      */
/*                         ------------                                 */
/*                        |            |                                */
/*    ENAB       -0------}|            |                                */
/*                        |            |                                */
/*    INP0       -1------}|   SAMPLE   |                                */
/*                        |     &      |----} Netlist node              */
/*    CLOCK      -2------}|    HOLD    |                                */
/*                        |            |                                */
/*    CLKTYPE    -3------}|            |                                */
/*                        |            |                                */
/*                         ------------                                 */
/*                                                                      */
/*  Declaration syntax                                                  */
/*                                                                      */
/*     DISCRETE_SAMPHOLD(name of node,                                  */
/*                       enable                                         */
/*                       input node                                     */
/*                       clock node or static value                     */
/*                       input clock type)                              */
/*                                                                      */
/*  Example config line                                                 */
/*                                                                      */
/*     DISCRETE_SAMPHOLD(NODE_9,1,NODE_11,NODE_12,DISC_SAMPHOLD_REDGE)  */
/*                                                                      */
/*  Node9 will sample the input node 11 on the rising edge (REDGE) of   */
/*  the input clock signal of node 12.                                  */
/*                                                                      */
/*   DISC_SAMPHOLD_REDGE  - Rising edge clock                           */
/*   DISC_SAMPHOLD_FEDGE  - Falling edge clock                          */
/*   DISC_SAMPHOLD_HLATCH - Output is latched whilst clock is high      */
/*   DISC_SAMPHOLD_LLATCH - Output is latched whilst clock is low       */
/*                                                                      */
/************************************************************************/
/*                                                                      */
/* DISCRETE_LOGIC_INV  - Logic invertor                                 */
/* DISCRETE_LOGIC_AND  - Logic AND gate (3 & 4 input also available)    */
/* DISCRETE_LOGIC_NAND - Logic NAND gate (3 & 4 input also available)   */
/* DISCRETE_LOGIC_OR   - Logic OR gate (3 & 4 input also available)     */
/* DISCRETE_LOGIC_NOR  - Logic NOR gate (3 & 4 input also available)    */
/* DISCRETE_LOGIC_XOR  - Logic XOR gate                                 */
/* DISCRETE_LOGIC_NXOR - Logic NXOR gate                                */
/*                                                                      */
/*                         ------------                                 */
/*                        |            |                                */
/*    ENAB       -0------}|            |                                */
/*                        |            |                                */
/*    INPUT0     -0------}|            |                                */
/*                        |   LOGIC    |                                */
/*    [INPUT1]   -1------}|  FUNCTION  |----}   Netlist node            */
/*                        |    !&|^    |                                */
/*    [INPUT2]   -2------}|            |                                */
/*                        |            |                                */
/*    [INPUT3]   -3------}|            |                                */
/*                        |            |                                */
/*    [] - Optional        ------------                                 */
/*                                                                      */
/*  Declaration syntax                                                  */
/*                                                                      */
/*     DISCRETE_LOGIC_XXXn(name of node,                                */
/*      (X=INV/AND/etc)    enable node or static value                  */
/*      (n=Blank/2/3)      input0 node or static value                  */
/*                         [input1 node or static value]                */
/*                         [input2 node or static value]                */
/*                         [input3 node or static value])               */
/*                                                                      */
/*  Example config lines                                                */
/*                                                                      */
/*     DISCRETE_LOGIC_INV(NODE_03,1,NODE_12)                            */
/*     DISCRETE_LOGIC_AND(NODE_03,1,NODE_12,NODE_13)                    */
/*     DISCRETE_LOGIC_NOR4(NODE_03,1,NODE_12,NODE_13,NODE_14,NODE_15)   */
/*                                                                      */
/*  Node output is always either 0.0 or 1.0 any input value !=0.0 is    */
/*  taken as a logic 1.                                                 */
/*                                                                      */
/************************************************************************/
/*                                                                      */
/* DISCRETE_NE555 - NE555 Chip simulation                               */
/*                                                                      */
/*                         ------------                                 */
/*                        |            |                                */
/*    RESET      -0------}|            |                                */
/*                        |            |                                */
/*    TRIGGER    -1------}|            |                                */
/*                        |            |                                */
/*    THRESHOLD  -2------}|   NE555    |----}   Netlist node            */
/*                        |            |                                */
/*    CONTROL V  -3------}|            |                                */
/*                        |            |                                */
/*    VCC        -4------}|            |                                */
/*                        |            |                                */
/*                         ------------                                 */
/*                                                                      */
/*  Declaration syntax                                                  */
/*                                                                      */
/*     DISCRETE_NE555(reset node (or value) - <0.7 causes a reset       */
/*                    trigger node (or value)                           */
/*                    threshold node (or value)                         */
/*                    ctrl volt node (or value) - Use NODE_NC for N/C   */
/*                    vcc node (or value) - Needed for comparators)     */
/*                                                                      */
/*  Example config line                                                 */
/*                                                                      */
/*     DISCRETE_NE555(NODE32,1,NODE_31,NODE_31,NODE_NC,5.0)             */
/*                                                                      */
/************************************************************************/
/*                                                                      */
/* DISCRETE_OUTPUT - Single output node to Mame mixer and output        */
/*                                                                      */
/*                             ----------                               */
/*                            |          |     -/|                      */
/*      Netlist node --------}| OUTPUT   |----|  | Sound Output         */
/*                            |          |     -\|                      */
/*                             ----------                               */
/*                                                                      */
/*  Declaration syntax                                                  */
/*                                                                      */
/*     DISCRETE_OUTPUT(name of output node,volume)                      */
/*                                                                      */
/*  Example config line                                                 */
/*                                                                      */
/*     DISCRETE_OUTPUT(NODE_02,100)                                     */
/*                                                                      */
/*  Output stream will be generated from the NODE_02 output stream.     */
/*                                                                      */
/************************************************************************/
/*                                                                      */
/* DISCRETE_OUTPUT_STEREO - Single output node to Mame mixer and output */
/*                                                                      */
/*                                ----------                            */
/*                               |          |                           */
/*    Left  Netlist node -------}|          |     -/|                   */
/*                               | OUTPUT   |----|  | Sound Output      */
/*    Right Netlist node -------}|          |     -\|     L/R           */
/*                               |          |                           */
/*                                ----------                            */
/*  Declaration syntax                                                  */
/*                                                                      */
/*     DISCRETE_OUTPUT_STEREO(left output node,                         */
/*                            right output node,                        */
/*                            volume level)                             */
/*                                                                      */
/*  Example config line                                                 */
/*                                                                      */
/*   DISCRETE_OUTPUT_STEREO(NODE_02,NODE12,100)                         */
/*                                                                      */
/*  Output stream will be generated from the NODE_02 and NODE_12        */
/*  streams.                                                            */
/*                                                                      */
/************************************************************************/

#define DISCRETE_MAX_NODES		300
#define DISCRETE_MAX_ADJUSTERS	20

#define DISC_LOGADJ 1.0
#define DISC_LINADJ 0.0

/* Function possibilities for the LFSR feedback nodes */
/* 2 inputs, one output                               */
#define DISC_LFSR_XOR		0
#define DISC_LFSR_OR		1
#define DISC_LFSR_AND		2
#define DISC_LFSR_XNOR		3
#define DISC_LFSR_NOR		4
#define DISC_LFSR_NAND		5
#define DISC_LFSR_IN0		6
#define DISC_LFSR_IN1		7
#define DISC_LFSR_NOT_IN0	8
#define DISC_LFSR_NOT_IN1	9
#define DISC_LFSR_REPLACE	10

/* Sample & Hold supported clock types */
#define DISC_SAMPHOLD_REDGE		0
#define DISC_SAMPHOLD_FEDGE		1
#define DISC_SAMPHOLD_HLATCH	2
#define DISC_SAMPHOLD_LLATCH	3


struct discrete_sound_block
{
	int node;                                   /* Output node number */
	int type;                                   /* see defines below */
	int input_node0;                            /* input/control nodes */
	int input_node1;                            /* input/control nodes */
	int input_node2;                            /* input/control nodes */
	int input_node3;                            /* input/control nodes */
	int input_node4;                            /* input/control nodes */
	int input_node5;                            /* input/control nodes */
	float initial0;                            /* Initial values */
	float initial1;                            /* Initial values */
	float initial2;                            /* Initial values */
	float initial3;                            /* Initial values */
	float initial4;                            /* Initial values */
	float initial5;                            /* Initial values */
	const void *custom;                         /* Custom function specific initialisation data */
	const char *name;                           /* Node Name */
};

struct node_description
{
	int 	node;								/* The nodes index number in the node list      */
	int		module;								/* The nodes module number from the module list */
	float	output;								/* The nodes last output value                  */
	struct node_description *input_node0;		/* Either pointer to input node OR NULL in which case use the input value */
	struct node_description *input_node1;
	struct node_description *input_node2;
	struct node_description *input_node3;
	struct node_description *input_node4;
	struct node_description *input_node5;
	float	input0;
	float	input1;
	float	input2;
	float	input3;
	float	input4;
	float	input5;
	void	*context;                           /* Contextual information specific to this node type */
	const char	*name;
	const void *custom;                         /* Custom function specific initialisation data */
};

struct discrete_module
{
	int type;
	const char *name;
	int (*init) (struct node_description *node);	/* Create the context resource for a node */
	int (*kill) (struct node_description *node);	/* Destroy the context of a node and release all resources */
	int (*reset)(struct node_description *node);	/* Called to reset a node after creation or system reset */
	int (*step) (struct node_description *node);	/* Called to execute one time delta of output update */
};

struct discrete_sh_adjuster
{
	float value;
	float min;
	float max;
	float initial;
	const char *name;
	int islogscale;
};

struct discrete_lfsr_desc
{
	int bitlength;
	int reset_value;

	int feedback_bitsel0;
	int feedback_bitsel1;
	int feedback_function0;         /* Combines bitsel0 & bitsel1 */

	int feedback_function1;         /* Combines funct0 & infeed bit */

	int feedback_function2;         /* Combines funct1 & shifted register */
	int feedback_function2_mask;    /* Which bits are affected by function 2 */

	int output_invert;              /* Invert the output */

	int output_bit;
};


struct discrete_ladder
{
	int ladderlength;
	float resistors[8];	/* Limit of 8 bit resistor ladder */
	float capacitor;
};


enum { NODE_00=0x40000000
              , NODE_01, NODE_02, NODE_03, NODE_04, NODE_05, NODE_06, NODE_07, NODE_08, NODE_09,
       NODE_10, NODE_11, NODE_12, NODE_13, NODE_14, NODE_15, NODE_16, NODE_17, NODE_18, NODE_19,
       NODE_20, NODE_21, NODE_22, NODE_23, NODE_24, NODE_25, NODE_26, NODE_27, NODE_28, NODE_29,
       NODE_30, NODE_31, NODE_32, NODE_33, NODE_34, NODE_35, NODE_36, NODE_37, NODE_38, NODE_39,
       NODE_40, NODE_41, NODE_42, NODE_43, NODE_44, NODE_45, NODE_46, NODE_47, NODE_48, NODE_49,
       NODE_50, NODE_51, NODE_52, NODE_53, NODE_54, NODE_55, NODE_56, NODE_57, NODE_58, NODE_59,
       NODE_60, NODE_61, NODE_62, NODE_63, NODE_64, NODE_65, NODE_66, NODE_67, NODE_68, NODE_69,
       NODE_70, NODE_71, NODE_72, NODE_73, NODE_74, NODE_75, NODE_76, NODE_77, NODE_78, NODE_79,
       NODE_80, NODE_81, NODE_82, NODE_83, NODE_84, NODE_85, NODE_86, NODE_87, NODE_88, NODE_89,
       NODE_90, NODE_91, NODE_92, NODE_93, NODE_94, NODE_95, NODE_96, NODE_97, NODE_98, NODE_99,
       NODE_100,NODE_101,NODE_102,NODE_103,NODE_104,NODE_105,NODE_106,NODE_107,NODE_108,NODE_109,
       NODE_110,NODE_111,NODE_112,NODE_113,NODE_114,NODE_115,NODE_116,NODE_117,NODE_118,NODE_119,
       NODE_120,NODE_121,NODE_122,NODE_123,NODE_124,NODE_125,NODE_126,NODE_127,NODE_128,NODE_129,
       NODE_130,NODE_131,NODE_132,NODE_133,NODE_134,NODE_135,NODE_136,NODE_137,NODE_138,NODE_139,
       NODE_140,NODE_141,NODE_142,NODE_143,NODE_144,NODE_145,NODE_146,NODE_147,NODE_148,NODE_149,
       NODE_150,NODE_151,NODE_152,NODE_153,NODE_154,NODE_155,NODE_156,NODE_157,NODE_158,NODE_159,
       NODE_160,NODE_161,NODE_162,NODE_163,NODE_164,NODE_165,NODE_166,NODE_167,NODE_168,NODE_169,
       NODE_170,NODE_171,NODE_172,NODE_173,NODE_174,NODE_175,NODE_176,NODE_177,NODE_178,NODE_179,
       NODE_180,NODE_181,NODE_182,NODE_183,NODE_184,NODE_185,NODE_186,NODE_187,NODE_188,NODE_189,
       NODE_190,NODE_191,NODE_192,NODE_193,NODE_194,NODE_195,NODE_196,NODE_197,NODE_198,NODE_199,
       NODE_200,NODE_201,NODE_202,NODE_203,NODE_204,NODE_205,NODE_206,NODE_207,NODE_208,NODE_209,
       NODE_210,NODE_211,NODE_212,NODE_213,NODE_214,NODE_215,NODE_216,NODE_217,NODE_218,NODE_219,
       NODE_220,NODE_221,NODE_222,NODE_223,NODE_224,NODE_225,NODE_226,NODE_227,NODE_228,NODE_229,
       NODE_230,NODE_231,NODE_232,NODE_233,NODE_234,NODE_235,NODE_236,NODE_237,NODE_238,NODE_239,
       NODE_240,NODE_241,NODE_242,NODE_243,NODE_244,NODE_245,NODE_246,NODE_247,NODE_248,NODE_249,
       NODE_250,NODE_251,NODE_252,NODE_253,NODE_254,NODE_255,NODE_256,NODE_257,NODE_258,NODE_259,
       NODE_260,NODE_261,NODE_262,NODE_263,NODE_264,NODE_265,NODE_266,NODE_267,NODE_268,NODE_269,
       NODE_270,NODE_271,NODE_272,NODE_273,NODE_274,NODE_275,NODE_276,NODE_277,NODE_278,NODE_279,
       NODE_280,NODE_281,NODE_282,NODE_283,NODE_284,NODE_285,NODE_286,NODE_287,NODE_288,NODE_289,
       NODE_290,NODE_291,NODE_292,NODE_293,NODE_294,NODE_295,NODE_296,NODE_297,NODE_298,NODE_299 };

/* Some Pre-defined nodes for convenience */

#define NODE_NC  NODE_00
#define NODE_OP  (NODE_00+(DISCRETE_MAX_NODES))

#define NODE_START	NODE_00
#define NODE_END	NODE_OP

/************************************************************************/
/*                                                                      */
/*        Enumerated values for Node types in the simulation            */
/*                                                                      */
/*  DSS - Discrete Sound Source                                         */
/*  DST - Discrete Sound Transform                                      */
/*  DSD - Discrete Sound Device                                         */
/*  DSO - Discrete Sound Output                                         */
/*                                                                      */
/************************************************************************/
enum {
	/* Sources */
		DSS_NULL,                                /* Nothing, nill, zippo, only to be used as terminating node */
		DSS_CONSTANT,                            /* Constant node */
		DSS_ADJUSTMENT,                          /* Adjustment node */
		DSS_INPUT,                               /* Memory mapped input node */
		DSS_NOISE,                               /* Random Noise generator */
		DSS_LFSR_NOISE,                          /* Cyclic/Resetable LFSR based Noise generator */
		DSS_SQUAREWAVE,                          /* Square Wave generator */
		DSS_SINEWAVE,                            /* Sine Wave generator */
		DSS_TRIANGLEWAVE,                        /* Triangle wave generator */
		DSS_SAWTOOTHWAVE,                        /* Sawtooth wave generator */
	/* Transforms */
		DST_RCFILTER,							 /* Simple RC Filter network */
		DST_RCDISC,                              /* Simple RC discharge */
		DST_RCDISC2,                             /* Switched 2 Input RC discharge */
		DST_RAMP,                                /* Ramp up/down simulation */
		DST_CLAMP,                               /* Signal Clamp */
//		DST_LPF,                                 /* Low Pass Filter */
//		DST_HPF,                                 /* High Pass Filter */
		DST_GAIN,                                /* Gain Block, C = A*B */
		DST_DIVIDE,                              /* Gain Block, C = A/B */
		DST_ADDER,                               /* C = A+B */
		DST_SWITCH,                              /* C = A or B */
		DST_ONESHOT,                             /* One-shot pulse generator */
		DST_SAMPHOLD,                            /* Sample & hold transform */
		DST_LADDER,                              /* Resistor ladder emulation */
//		DST_DELAY,                               /* Phase shift/Delay line */
	/* Logic */
		DST_LOGIC_INV,
		DST_LOGIC_AND,
		DST_LOGIC_NAND,
		DST_LOGIC_OR,
		DST_LOGIC_NOR,
		DST_LOGIC_XOR,
		DST_LOGIC_NXOR,
	/* Devices */
		DSD_NE555,                               /* NE555 Emulation */
	/* Custom */
//		DST_CUSTOM,                              /* whatever you want someday */
    /* Output Node */
		DSO_OUTPUT                              /* The final output node */
};


/************************************************************************/
/*                                                                      */
/*        Encapsulation macros for defining your simulation             */
/*                                                                      */
/************************************************************************/
#define DISCRETE_SOUND_START(STRUCTURENAME) struct discrete_sound_block STRUCTURENAME[] = {
#define DISCRETE_SOUND_END                                             { NODE_00, DSS_NULL       ,NODE_NC,NODE_NC,NODE_NC,NODE_NC,NODE_NC,NODE_NC,0     ,0     ,0     ,0     ,0     ,0     ,NULL  ,"End Marker" }  };

#define DISCRETE_CONSTANT(NODE,CONST)                                  { NODE   , DSS_CONSTANT   ,NODE_NC,NODE_NC,NODE_NC,NODE_NC,NODE_NC,NODE_NC,CONST ,0     ,0     ,0     ,0     ,0     ,NULL  ,"Constant"           },
#define DISCRETE_ADJUSTMENT(NODE,ENAB,MIN,MAX,DEFAULT,LOGLIN,NAME)     { NODE   , DSS_ADJUSTMENT ,ENAB   ,NODE_NC,NODE_NC,NODE_NC,NODE_NC,NODE_NC,ENAB  ,MIN   ,MAX   ,DEFAULT,LOGLIN,0    ,NULL  ,NAME                 },
#define DISCRETE_INPUT(NODE,ADDR,MASK,INIT0)                           { NODE   , DSS_INPUT      ,NODE_NC,NODE_NC,NODE_NC,NODE_NC,NODE_NC,NODE_NC,INIT0 ,ADDR  ,MASK  ,1     ,0     ,INIT0 ,NULL  ,"Input"              },
#define DISCRETE_INPUTX(NODE,ADDR,MASK,GAIN,OFFSET,INIT0)              { NODE   , DSS_INPUT      ,NODE_NC,NODE_NC,NODE_NC,NODE_NC,NODE_NC,NODE_NC,INIT0 ,ADDR  ,MASK  ,GAIN  ,OFFSET,INIT0 ,NULL  ,"InputX"             },

#define DISCRETE_SQUAREWAVE(NODE,ENAB,FREQ,AMPL,DUTY,BIAS,PHASE)       { NODE   , DSS_SQUAREWAVE ,ENAB   ,FREQ   ,AMPL   ,DUTY   ,BIAS   ,NODE_NC,ENAB  ,FREQ  ,AMPL  ,DUTY  ,BIAS  ,PHASE ,NULL  ,"Square Wave"        },
#define DISCRETE_SINEWAVE(NODE,ENAB,FREQ,AMPL,BIAS,PHASE)              { NODE   , DSS_SINEWAVE   ,ENAB   ,FREQ   ,AMPL   ,BIAS   ,NODE_NC,NODE_NC,ENAB  ,FREQ  ,AMPL  ,BIAS  ,PHASE ,0     ,NULL  ,"Sine Wave"          },
#define DISCRETE_NOISE(NODE,ENAB,FREQ,AMPL,BIAS)                       { NODE   , DSS_NOISE      ,ENAB   ,FREQ   ,AMPL   ,BIAS   ,NODE_NC,NODE_NC,ENAB  ,FREQ  ,AMPL  ,BIAS  ,0     ,0     ,NULL  ,"Noise Source"       },
#define DISCRETE_TRIANGLEWAVE(NODE,ENAB,FREQ,AMPL,BIAS,PHASE)          { NODE   , DSS_TRIANGLEWAVE,ENAB  ,FREQ   ,AMPL   ,BIAS   ,NODE_NC,NODE_NC,ENAB  ,FREQ  ,AMPL  ,BIAS  ,PHASE ,0     ,NULL  ,"Triangle Wave"      },
#define DISCRETE_SAWTOOTHWAVE(NODE,ENAB,FREQ,AMPL,BIAS,GRAD,PHASE)     { NODE   , DSS_SAWTOOTHWAVE,ENAB  ,FREQ   ,AMPL   ,BIAS   ,NODE_NC,NODE_NC,ENAB  ,FREQ  ,AMPL  ,BIAS  ,GRAD  ,PHASE ,NULL  ,"Saw Tooth Wave"     },
#define DISCRETE_LFSR_NOISE(NODE,ENAB,RESET,FREQ,AMPL,FEED,BIAS,LFSRTB){ NODE   , DSS_LFSR_NOISE ,ENAB   ,RESET  ,FREQ   ,AMPL   ,FEED   ,BIAS   ,ENAB  ,RESET ,FREQ  ,AMPL  ,FEED  ,BIAS  ,LFSRTB,"LFSR Noise Source"  },

#define DISCRETE_RCFILTER(NODE,ENAB,INP0,RVAL,CVAL)                    { NODE   , DST_RCFILTER   ,ENAB   ,INP0   ,NODE_NC,NODE_NC,NODE_NC,NODE_NC,ENAB  ,INP0  ,RVAL  ,CVAL  ,0     ,0     ,NULL  ,"RC Filter"          },
#define DISCRETE_RCDISC(NODE,ENAB,INP0,RVAL,CVAL)                      { NODE   , DST_RCDISC     ,ENAB   ,INP0   ,NODE_NC,NODE_NC,NODE_NC,NODE_NC,ENAB  ,INP0  ,RVAL  ,CVAL  ,0     ,0     ,NULL  ,"RC Discharge"       },
#define DISCRETE_RCDISC2(NODE,SWITCH,INP0,RVAL0,INP1,RVAL1,CVAL)       { NODE   , DST_RCDISC2    ,SWITCH ,INP0   ,NODE_NC,INP1   ,NODE_NC,NODE_NC,SWITCH,INP0  ,RVAL0 ,INP1  ,RVAL1 ,CVAL  ,NULL  ,"RC Discharge 2"     },
#define DISCRETE_RAMP(NODE,ENAB,RAMP,GRAD,START,END,CLAMP)             { NODE   , DST_RAMP       ,ENAB   ,RAMP   ,GRAD   ,START  ,END    ,CLAMP  ,ENAB  ,RAMP  ,GRAD  ,START ,END   ,CLAMP ,NULL  ,"Ramp Up/Down"       },
#define DISCRETE_ONOFF(NODE,ENAB,INP0)                                 { NODE   , DST_GAIN       ,ENAB   ,INP0   ,NODE_NC,NODE_NC,NODE_NC,NODE_NC,ENAB  ,0     ,1     ,0     ,0     ,0     ,NULL  ,"OnOff Switch"       },
#define DISCRETE_INVERT(NODE,INP0)                                     { NODE   , DST_GAIN       ,NODE_NC,INP0   ,NODE_NC,NODE_NC,NODE_NC,NODE_NC,1     ,0     ,-1    ,0     ,0     ,0     ,NULL  ,"Inverter"           },
#define DISCRETE_GAIN(NODE,INP0,GAIN)                                  { NODE   , DST_GAIN       ,NODE_NC,INP0   ,NODE_NC,NODE_NC,NODE_NC,NODE_NC,1     ,INP0  ,GAIN  ,0     ,0     ,0     ,NULL  ,"Gain"               },
#define DISCRETE_MULTIPLY(NODE,ENAB,INP0,INP1)                         { NODE   , DST_GAIN       ,ENAB   ,INP0   ,INP1   ,NODE_NC,NODE_NC,NODE_NC,ENAB  ,INP0  ,INP1  ,0     ,0     ,0     ,NULL  ,"Multiplier"         },
#define DISCRETE_DIVIDE(NODE,ENAB,INP0,INP1)                           { NODE   , DST_DIVIDE     ,ENAB   ,INP0   ,INP1   ,NODE_NC,NODE_NC,NODE_NC,ENAB  ,INP0  ,INP1  ,0     ,0     ,0     ,NULL  ,"Divider"            },
#define DISCRETE_CLAMP(NODE,ENAB,INP0,MIN,MAX,CLAMP)                   { NODE   , DST_CLAMP      ,ENAB   ,INP0   ,MIN    ,MAX    ,CLAMP  ,NODE_NC,ENAB  ,INP0  ,MIN   ,MAX   ,CLAMP ,0     ,NULL  ,"Signal Clamp"       },
#define DISCRETE_SWITCH(NODE,ENAB,SWITCH,INP0,INP1)                    { NODE   , DST_SWITCH     ,ENAB   ,SWITCH ,INP0   ,INP1   ,NODE_NC,NODE_NC,ENAB  ,SWITCH,INP0  ,INP1  ,0     ,0     ,NULL  ,"2 Pole Switch"      },
#define DISCRETE_ADDER2(NODE,ENAB,INP0,INP1)                           { NODE   , DST_ADDER      ,ENAB   ,INP0   ,INP1   ,NODE_NC,NODE_NC,NODE_NC,ENAB  ,INP0  ,INP1  ,0     ,0     ,0     ,NULL  ,"Adder 2 Node"       },
#define DISCRETE_ADDER3(NODE,ENAB,INP0,INP1,INP2)                      { NODE   , DST_ADDER      ,ENAB   ,INP0   ,INP1   ,INP2   ,NODE_NC,NODE_NC,ENAB  ,INP0  ,INP1  ,INP2  ,0     ,0     ,NULL  ,"Adder 3 Node"       },
#define DISCRETE_ADDER4(NODE,ENAB,INP0,INP1,INP2,INP3)                 { NODE   , DST_ADDER      ,ENAB   ,INP0   ,INP1   ,INP2   ,INP3   ,NODE_NC,ENAB  ,INP0  ,INP1  ,INP2  ,INP3  ,0     ,NULL  ,"Adder 4 Node"       },
#define DISCRETE_ONESHOTR(NODE,ENAB,TRIG,AMPL,WIDTH)                   { NODE   , DST_ONESHOT    ,ENAB   ,TRIG   ,NODE_NC,AMPL   ,WIDTH  ,NODE_NC,ENAB  ,TRIG  ,1     ,AMPL  ,WIDTH ,0     ,NULL  ,"One Shot Resetable" },
#define DISCRETE_ONESHOT(NODE,ENAB,TRIG,RESET,AMPL,WIDTH)              { NODE   , DST_ONESHOT    ,ENAB   ,TRIG   ,RESET  ,AMPL   ,WIDTH  ,NODE_NC,ENAB  ,TRIG  ,RESET ,AMPL  ,WIDTH ,0     ,NULL  ,"One Shot"           },
#define DISCRETE_LADDER(NODE,ENAB,INP0,GAIN,LADDER)                    { NODE   , DST_LADDER     ,ENAB   ,INP0   ,GAIN   ,NODE_NC,NODE_NC,NODE_NC,ENAB  ,INP0  ,GAIN  ,0     ,0     ,0     ,LADDER,"Resistor Ladder"    },
#define DISCRETE_SAMPLHOLD(NODE,ENAB,INP0,CLOCK,CLKTYPE)               { NODE   , DST_SAMPHOLD   ,ENAB   ,INP0   ,CLOCK  ,NODE_NC,NODE_NC,NODE_NC,ENAB  ,INP0  ,CLOCK ,CLKTYPE,0    ,0     ,NULL  ,"Sample & Hold"      },

#define	DISCRETE_LOGIC_INVERT(NODE,ENAB,INP0)                          { NODE   , DST_LOGIC_INV  ,ENAB   ,INP0   ,NODE_NC,NODE_NC,NODE_NC,NODE_NC,ENAB  ,INP0  ,0     ,0     ,0     ,0     ,NULL  ,"Logic Invertor"     },
#define	DISCRETE_LOGIC_AND(NODE,ENAB,INP0,INP1)                        { NODE   , DST_LOGIC_AND  ,ENAB   ,INP0   ,INP1   ,NODE_NC,NODE_NC,NODE_NC,ENAB  ,INP0  ,INP1  ,1     ,1     ,0     ,NULL  ,"Logic AND (2inp)"   },
#define	DISCRETE_LOGIC_AND3(NODE,ENAB,INP0,INP1,INP2)                  { NODE   , DST_LOGIC_AND  ,ENAB   ,INP0   ,INP1   ,INP2   ,NODE_NC,NODE_NC,ENAB  ,INP0  ,INP1  ,INP2  ,1     ,0     ,NULL  ,"Logic AND (3inp)"   },
#define	DISCRETE_LOGIC_AND4(NODE,ENAB,INP0,INP1,INP2,INP3)             { NODE   , DST_LOGIC_AND  ,ENAB   ,INP0   ,INP1   ,INP2   ,INP3   ,NODE_NC,ENAB  ,INP0  ,INP1  ,INP2  ,INP3  ,0     ,NULL  ,"Logic AND (4inp)"   },
#define	DISCRETE_LOGIC_NAND(NODE,ENAB,INP0,INP1)                       { NODE   , DST_LOGIC_NAND ,ENAB   ,INP0   ,INP1   ,NODE_NC,NODE_NC,NODE_NC,ENAB  ,INP0  ,INP1  ,1     ,1     ,0     ,NULL  ,"Logic NAND (2inp)"  },
#define	DISCRETE_LOGIC_NAND3(NODE,ENAB,INP0,INP1,INP2)                 { NODE   , DST_LOGIC_NAND ,ENAB   ,INP0   ,INP1   ,INP2   ,NODE_NC,NODE_NC,ENAB  ,INP0  ,INP1  ,INP2  ,1     ,0     ,NULL  ,"Logic NAND (3inp)"  },
#define	DISCRETE_LOGIC_NAND4(NODE,ENAB,INP0,INP1,INP2,INP3)            { NODE   , DST_LOGIC_NAND ,ENAB   ,INP0   ,INP1   ,INP2   ,INP3   ,NODE_NC,ENAB  ,INP0  ,INP1  ,INP2  ,INP3  ,0     ,NULL  ,"Logic NAND (4inp)"  },
#define	DISCRETE_LOGIC_OR(NODE,ENAB,INP0,INP1)                         { NODE   , DST_LOGIC_OR   ,ENAB   ,INP0   ,INP1   ,NODE_NC,NODE_NC,NODE_NC,ENAB  ,INP0  ,INP1  ,0     ,0     ,0     ,NULL  ,"Logic OR (2inp)"    },
#define	DISCRETE_LOGIC_OR3(NODE,ENAB,INP0,INP1,INP2)                   { NODE   , DST_LOGIC_OR   ,ENAB   ,INP0   ,INP1   ,INP2   ,NODE_NC,NODE_NC,ENAB  ,INP0  ,INP1  ,INP2  ,0     ,0     ,NULL  ,"Logic OR (3inp)"    },
#define	DISCRETE_LOGIC_OR4(NODE,ENAB,INP0,INP1,INP2,INP3)              { NODE   , DST_LOGIC_OR   ,ENAB   ,INP0   ,INP1   ,INP2   ,INP3   ,NODE_NC,ENAB  ,INP0  ,INP1  ,INP2  ,INP3  ,0     ,NULL  ,"Logic OR (4inp)"    },
#define	DISCRETE_LOGIC_NOR(NODE,ENAB,INP0,INP1)                        { NODE   , DST_LOGIC_NOR  ,ENAB   ,INP0   ,INP1   ,NODE_NC,NODE_NC,NODE_NC,ENAB  ,INP0  ,INP1  ,0     ,0     ,0     ,NULL  ,"Logic NOR (2inp)"   },
#define	DISCRETE_LOGIC_NOR3(NODE,ENAB,INP0,INP1,INP2)                  { NODE   , DST_LOGIC_NOR  ,ENAB   ,INP0   ,INP1   ,INP2   ,NODE_NC,NODE_NC,ENAB  ,INP0  ,INP1  ,INP2  ,0     ,0     ,NULL  ,"Logic NOR (3inp)"   },
#define	DISCRETE_LOGIC_NOR4(NODE,ENAB,INP0,INP1,INP2,INP3)             { NODE   , DST_LOGIC_NOR  ,ENAB   ,INP0   ,INP1   ,INP2   ,INP3   ,NODE_NC,ENAB  ,INP0  ,INP1  ,INP2  ,INP3  ,0     ,NULL  ,"Logic NOR (4inp)"   },
#define	DISCRETE_LOGIC_XOR(NODE,ENAB,INP0,INP1)                        { NODE   , DST_LOGIC_XOR  ,ENAB   ,INP0   ,INP1   ,NODE_NC,NODE_NC,NODE_NC,ENAB  ,INP0  ,INP1  ,0     ,0     ,0     ,NULL  ,"Logic XOR (2inp)"   },
#define	DISCRETE_LOGIC_NXOR(NODE,ENAB,INP0,INP1)                       { NODE   , DST_LOGIC_NXOR ,ENAB   ,INP0   ,INP1   ,NODE_NC,NODE_NC,NODE_NC,ENAB  ,INP0  ,INP1  ,0     ,0     ,0     ,NULL  ,"Logic NXOR (2inp)"  },

#define DISCRETE_NE555(NODE,RESET,TRIGR,THRSH,CTRLV,VCC)               { NODE   , DSD_NE555      ,RESET  ,TRIGR  ,THRSH  ,CTRLV  ,VCC    ,NODE_NC,RESET ,TRIGR ,THRSH ,CTRLV ,VCC   ,0     ,NULL  ,"NE555 (Broken"      },

#define DISCRETE_OUTPUT(OPNODE,VOL)                                    { NODE_OP, DSO_OUTPUT     ,OPNODE ,OPNODE ,NODE_NC,NODE_NC,NODE_NC,NODE_NC,0     ,0     ,VOL   ,0     ,0     ,0     ,NULL  ,"Output Node"        },
#define DISCRETE_OUTPUT_STEREO(OPNODEL,OPNODER,VOL)                    { NODE_OP, DSO_OUTPUT     ,OPNODEL,OPNODER,NODE_NC,NODE_NC,NODE_NC,NODE_NC,0     ,0     ,VOL   ,0     ,0     ,0     ,NULL  ,"Stereo Output Node" },


/************************************************************************/
/*                                                                      */
/*        Software interface to the external world i.e Into Mame        */
/*                                                                      */
/************************************************************************/

int  discrete_sh_start (const struct MachineSound *msound);
void discrete_sh_stop (void);
void discrete_sh_reset (void);
void discrete_sh_update (void);

int  discrete_sh_adjuster_count(struct discrete_sound_block *dsintf);
int  discrete_sh_adjuster_get(int arg,struct discrete_sh_adjuster *adjuster);
int discrete_sh_adjuster_set(int arg,struct discrete_sh_adjuster *adjuster);

WRITE_HANDLER(discrete_sound_w);
READ_HANDLER(discrete_sound_r);


#endif
