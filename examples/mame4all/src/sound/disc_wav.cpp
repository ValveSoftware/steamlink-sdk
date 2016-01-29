/************************************************************************/
/*                                                                      */
/*  MAME - Discrete sound system emulation library                      */
/*                                                                      */
/*  Written by Keith Wilkins (mame@esplexo.co.uk)                       */
/*                                                                      */
/*  (c) K.Wilkins 2000                                                  */
/*                                                                      */
/************************************************************************/
/*                                                                      */
/* DSS_SINEWAVE          - Sinewave generator source code               */
/* DSS_SQUAREWAVE        - Squarewave generator source code             */
/* DSS_TRIANGLEWAVE      - Triangle waveform generator                  */
/* DSS_SAWTOOTHWAVE      - Sawtooth waveform generator                  */
/* DSS_NOISE             - Noise Source - Random source                 */
/* DSS_LFSR_NOISE        - Linear Feedback Shift Register Noise         */
/*                                                                      */
/************************************************************************/

#define PI 3.14159

struct dss_sinewave_context
{
	float phase;
};

struct dss_noise_context
{
	float phase;
};

struct dss_lfsr_context
{
	float phase;
	int	lfsr_reg;
};

struct dss_squarewave_context
{
	float phase;
	float trigger;
};

struct dss_trianglewave_context
{
	float phase;
};

struct dss_sawtoothwave_context
{
	float phase;
	int type;
};

/************************************************************************/
/*                                                                      */
/* DSS_SINWAVE - Usage of node_description values for step function     */
/*                                                                      */
/* input0    - Enable input value                                       */
/* input1    - Frequency input value                                    */
/* input2    - Amplitde input value                                     */
/* input3    - DC Bias                                                  */
/* input4    - Starting phase                                           */
/* input5    - NOT USED                                                 */
/*                                                                      */
/************************************************************************/
int dss_sinewave_step(struct node_description *node)
{
	struct dss_sinewave_context *context=(struct dss_sinewave_context*)node->context;
	float newphase;

	/* Work out the phase step based on phase/freq & sample rate */
	/* The enable input only curtails output, phase rotation     */
	/* still occurs                                              */

	/*     phase step = 2Pi/(output period/sample period)        */
	/*                    boils out to                           */
	/*     phase step = (2Pi*output freq)/sample freq)           */
	newphase=context->phase+((2.0*PI*node->input1)/Machine->sample_rate);
	/* Keep the new phasor in thw 2Pi range.*/
	context->phase=fmod(newphase,2.0*PI);

	if(node->input0)
	{
		node->output=(node->input2/2.0) * sin(newphase);
		/* Add DC Bias component */
		node->output=node->output+node->input3;
	}
	else
	{
		/* Just output DC Bias */
		node->output=node->input3;
	}
	return 0;
}

int dss_sinewave_reset(struct node_description *node)
{
	struct dss_sinewave_context *context;
	float start;
	context=(struct dss_sinewave_context*)node->context;
	/* Establish starting phase, convert from degrees to radians */
	start=(node->input4/360.0)*(2.0*PI);
	/* Make sure its always mod 2Pi */
	context->phase=fmod(start,2.0*PI);
	return 0;
}

int dss_sinewave_init(struct node_description *node)
{
	/* Allocate memory for the context array and the node execution order array */
	if((node->context=(struct dss_sinewave_context*)malloc(sizeof(struct dss_sinewave_context)))==NULL)
	{
		return 1;
	}
	else
	{
		/* Initialise memory */
		memset(node->context,0,sizeof(struct dss_sinewave_context));
	}

	/* Initialise the object */
	dss_sinewave_reset(node);

	return 0;
}

int dss_sinewave_kill(struct node_description *node)
{
	free(node->context);
	node->context=NULL;
	return 0;
}



/************************************************************************/
/*                                                                      */
/* DSS_SQUAREWAVE - Usage of node_description values for step function  */
/*                                                                      */
/* input0    - Enable input value                                       */
/* input1    - Frequency input value                                    */
/* input2    - Amplitude input value                                    */
/* input3    - Duty Cycle                                               */
/* input4    - DC Bias level                                            */
/* input5    - Start Phase                                              */
/*                                                                      */
/************************************************************************/
int dss_squarewave_step(struct node_description *node)
{
	struct dss_squarewave_context *context=(struct dss_squarewave_context*)node->context;
	float newphase;

	/* Establish trigger phase from duty */
	context->trigger=((100-node->input3)/100)*(2.0*PI);

	/* Work out the phase step based on phase/freq & sample rate */
	/* The enable input only curtails output, phase rotation     */
	/* still occurs                                              */

	/*     phase step = 2Pi/(output period/sample period)        */
	/*                    boils out to                           */
	/*     phase step = (2Pi*output freq)/sample freq)           */
	newphase=context->phase+((2.0*PI*node->input1)/Machine->sample_rate);
	/* Keep the new phasor in thw 2Pi range.*/
	context->phase=fmod(newphase,2.0*PI);

	if(node->input0)
	{
		if(context->phase>context->trigger)
			node->output=(node->input2/2.0);
		else
			node->output=-(node->input2/2.0);

		/* Add DC Bias component */
		node->output=node->output+node->input4;
	}
	else
	{
		/* Just output DC Bias */
		node->output=node->input4;
	}
	return 0;
}

int dss_squarewave_reset(struct node_description *node)
{
	struct dss_squarewave_context *context;
	float start;
	context=(struct dss_squarewave_context*)node->context;

	/* Establish starting phase, convert from degrees to radians */
	start=(node->input5/360.0)*(2.0*PI);
	/* Make sure its always mod 2Pi */
	context->phase=fmod(start,2.0*PI);

	return 0;
}

int dss_squarewave_init(struct node_description *node)
{
	/* Allocate memory for the context array and the node execution order array */
	if((node->context=(struct dss_squarewave_context*)malloc(sizeof(struct dss_squarewave_context)))==NULL)
	{
		return 1;
	}
	else
	{
		/* Initialise memory */
		memset(node->context,0,sizeof(struct dss_squarewave_context));
	}

	/* Initialise the object */
	dss_squarewave_reset(node);

	return 0;
}

int dss_squarewave_kill(struct node_description *node)
{
	free(node->context);
	node->context=NULL;
	return 0;
}


/************************************************************************/
/*                                                                      */
/* DSS_TRIANGLEWAVE - Usage of node_description values for step function*/
/*                                                                      */
/* input0    - Enable input value                                       */
/* input1    - Frequency input value                                    */
/* input2    - Amplitde input value                                     */
/* input3    - DC Bias value                                            */
/* input4    - Initial Phase                                            */
/* input5    - NOT USED                                                 */
/*                                                                      */
/************************************************************************/
int dss_trianglewave_step(struct node_description *node)
{
	struct dss_trianglewave_context *context=(struct dss_trianglewave_context*)node->context;
	float newphase;

	/* Work out the phase step based on phase/freq & sample rate */
	/* The enable input only curtails output, phase rotation     */
	/* still occurs                                              */

	/*     phase step = 2Pi/(output period/sample period)        */
	/*                    boils out to                           */
	/*     phase step = (2Pi*output freq)/sample freq)           */
	newphase=context->phase+((2.0*PI*node->input1)/Machine->sample_rate);
	/* Keep the new phasor in thw 2Pi range.*/
	newphase=fmod(newphase,2.0*PI);
	context->phase=newphase;

	if(node->input0)
	{
		node->output=newphase < PI ? (node->input2 * (newphase / (PI/2.0) - 1.0))/2.0 :
									(node->input2 * (3.0 - newphase / (PI/2.0)))/2.0 ;

		/* Add DC Bias component */
		node->output=node->output+node->input3;
	}
	else
	{
		/* Just output DC Bias */
		node->output=node->input3;
	}
	return 0;
}

int dss_trianglewave_reset(struct node_description *node)
{
	struct dss_trianglewave_context *context;
	float start;

	context=(struct dss_trianglewave_context*)node->context;
	/* Establish starting phase, convert from degrees to radians */
	start=(node->input4/360.0)*(2.0*PI);
	/* Make sure its always mod 2Pi */
	context->phase=fmod(start,2.0*PI);
	return 0;
}

int dss_trianglewave_init(struct node_description *node)
{
	/* Allocate memory for the context array and the node execution order array */
	if((node->context=(struct dss_trianglewave_context*)malloc(sizeof(struct dss_trianglewave_context)))==NULL)
	{
		return 1;
	}
	else
	{
		/* Initialise memory */
		memset(node->context,0,sizeof(struct dss_trianglewave_context));
	}

	/* Initialise the object */
	dss_trianglewave_reset(node);
	return 0;
}

int dss_trianglewave_kill(struct node_description *node)
{
	free(node->context);
	node->context=NULL;
	return 0;
}



/************************************************************************/
/*                                                                      */
/* DSS_SAWTOOTHWAVE - Usage of node_description values for step function*/
/*                                                                      */
/* input0    - Enable input value                                       */
/* input1    - Frequency input value                                    */
/* input2    - Amplitde input value                                     */
/* input3    - DC Bias Value                                            */
/* input4    - Gradient                                                 */
/* input5    - Initial Phase                                            */
/*                                                                      */
/************************************************************************/
int dss_sawtoothwave_step(struct node_description *node)
{
	struct dss_sawtoothwave_context *context=(struct dss_sawtoothwave_context*)node->context;
	float newphase;

	/* Work out the phase step based on phase/freq & sample rate */
	/* The enable input only curtails output, phase rotation     */
	/* still occurs                                              */

	/*     phase step = 2Pi/(output period/sample period)        */
	/*                    boils out to                           */
	/*     phase step = (2Pi*output freq)/sample freq)           */
	newphase=context->phase+((2.0*PI*node->input1)/Machine->sample_rate);
	/* Keep the new phasor in thw 2Pi range.*/
	newphase=fmod(newphase,2.0*PI);
	context->phase=newphase;

	if(node->input0)
	{
		node->output=(context->type==0)?newphase*(node->input2/(2.0*PI)):node->input2-(newphase*(node->input2/(2.0*PI)));
		node->output-=node->input2/2.0;
		/* Add DC Bias component */
		node->output=node->output+node->input3;
	}
	else
	{
		/* Just output DC Bias */
		node->output=node->input3;
	}
	return 0;
}

int dss_sawtoothwave_reset(struct node_description *node)
{
	struct dss_sawtoothwave_context *context;
	float start;

	context=(struct dss_sawtoothwave_context*)node->context;
	/* Establish starting phase, convert from degrees to radians */
	start=(node->input5/360.0)*(2.0*PI);
	/* Make sure its always mod 2Pi */
	context->phase=fmod(start,2.0*PI);

	/* Invert gradient depending on sawtooth type /|/|/|/|/| or |\|\|\|\|\ */
	context->type=(node->input4)?1:0;

	return 0;
}

int dss_sawtoothwave_init(struct node_description *node)
{
	/* Allocate memory for the context array and the node execution order array */
	if((node->context=(struct dss_sawtoothwave_context*)malloc(sizeof(struct dss_sawtoothwave_context)))==NULL)
	{
		return 1;
	}
	else
	{
		/* Initialise memory */
		memset(node->context,0,sizeof(struct dss_sawtoothwave_context));
	}

	/* Initialise the object */
	dss_sawtoothwave_reset(node);
	return 0;
}

int dss_sawtoothwave_kill(struct node_description *node)
{
	free(node->context);
	node->context=NULL;
	return 0;
}



/************************************************************************/
/*                                                                      */
/* DSS_NOISE - Usage of node_description values for white nose generator*/
/*                                                                      */
/* input0    - Enable input value                                       */
/* input1    - Noise sample frequency                                   */
/* input2    - Amplitude input value                                    */
/* input3    - DC Bias value                                            */
/* input4    - NOT USED                                                 */
/* input5    - NOT USED                                                 */
/*                                                                      */
/************************************************************************/
int dss_noise_step(struct node_description *node)
{
	struct dss_noise_context *context;
	float newphase;
	context=(struct dss_noise_context*)node->context;

	newphase=context->phase+((2.0*PI*node->input1)/Machine->sample_rate);

	/* Keep the new phasor in thw 2Pi range.*/
	context->phase=fmod(newphase,2.0*PI);

	if(node->input0)
	{
		/* Only sample noise on rollover to next cycle */
		if(newphase>(2.0*PI))
		{
			int newval=rand() & 0x7fff;
			node->output=node->input2*(1-(newval/16384.0));

			/* Add DC Bias component */
			node->output=node->output+node->input3;
		}
	}
	else
	{
		/* Just output DC Bias */
		node->output=node->input3;
	}
	return 0;
}


int dss_noise_reset(struct node_description *node)
{
	struct dss_noise_context *context=(struct dss_noise_context*)node->context;
	context->phase=0;
	return 0;
}

int dss_noise_init(struct node_description *node)
{
	/* Allocate memory for the context array and the node execution order array */
	if((node->context=(struct dss_noise_context*)malloc(sizeof(struct dss_noise_context)))==NULL)
	{
		return 1;
	}
	else
	{
		/* Initialise memory */
		memset(node->context,0,sizeof(struct dss_noise_context));
	}

	/* Initialise the object */
	dss_noise_reset(node);

	return 0;
}

int dss_noise_kill(struct node_description *node)
{
	free(node->context);
	node->context=NULL;
	return 0;
}


/************************************************************************/
/*                                                                      */
/* DSS_LFSR_NOISE - Usage of node_description values for LFSR noise gen */
/*                                                                      */
/* input0    - Enable input value                                       */
/* input1    - Register reset                                           */
/* input2    - Noise sample frequency                                   */
/* input3    - Amplitude input value                                    */
/* input4    - Input feed bit                                           */
/* input5    - Bias                                                     */
/*                                                                      */
/************************************************************************/
int	dss_lfsr_function(int myfunc,int in0,int in1,int bitmask)
{
	int retval;

	in0&=bitmask;
	in1&=bitmask;

	switch(myfunc)
	{
		case DISC_LFSR_XOR:
			retval=in0^in1;
			break;
		case DISC_LFSR_OR:
			retval=in0|in1;
			break;
		case DISC_LFSR_AND:
			retval=in0&in1;
			break;
		case DISC_LFSR_XNOR:
			retval=in0^in1;
			retval=retval^bitmask;	/* Invert output */
			break;
		case DISC_LFSR_NOR:
			retval=in0|in1;
			retval=retval^bitmask;	/* Invert output */
			break;
		case DISC_LFSR_NAND:
			retval=in0&in1;
			retval=retval^bitmask;	/* Invert output */
			break;
		case DISC_LFSR_IN0:
			retval=in0;
			break;
		case DISC_LFSR_IN1:
			retval=in1;
			break;
		case DISC_LFSR_NOT_IN0:
			retval=in0^bitmask;
			break;
		case DISC_LFSR_NOT_IN1:
			retval=in1^bitmask;
			break;
		case DISC_LFSR_REPLACE:
			retval=in0&~in1;
			retval=in0|in1;
			break;
		default:
			retval=0;
			break;
	}
	return retval;
}

/* reset prototype so that it can be used in init function */
int dss_lfsr_reset(struct node_description *node);

int dss_lfsr_step(struct node_description *node)
{
	struct dss_lfsr_context *context;
	float newphase;
	context=(struct dss_lfsr_context*)node->context;

	newphase=context->phase+((2.0*PI*node->input2)/Machine->sample_rate);

	/* Keep the new phasor in thw 2Pi range.*/
	context->phase=fmod(newphase,2.0*PI);

	/* Reset everything if necessary */
	if(node->input1)
	{
		dss_lfsr_reset(node);
	}

	/* Only sample noise on rollover to next cycle */
	if(newphase>(2.0*PI))
	{
		int fb0,fb1,fbresult;
		struct discrete_lfsr_desc *lfsr_desc;

		/* Fetch the LFSR descriptor structure in a local for quick ref */
		lfsr_desc=(struct discrete_lfsr_desc*)(node->custom);

		/* Fetch the last feedback result */
		fbresult=((context->lfsr_reg)>>(lfsr_desc->bitlength))&0x01;

		/* Stage 2 feedback combine fbresultNew with infeed bit */
		fbresult=dss_lfsr_function(lfsr_desc->feedback_function1,fbresult,((node->input4)?0x01:0x00),0x01);

		/* Stage 3 first we setup where the bit is going to be shifted into */
		fbresult=fbresult*lfsr_desc->feedback_function2_mask;
		/* Then we left shift the register, */
		context->lfsr_reg=(context->lfsr_reg)<<1;
		/* Now move the fbresult into the shift register and mask it to the bitlength */
		context->lfsr_reg=dss_lfsr_function(lfsr_desc->feedback_function2,fbresult, (context->lfsr_reg), ((1<<(lfsr_desc->bitlength))-1));

		/* Now get and store the new feedback result */
		/* Fetch the feedback bits */
		fb0=((context->lfsr_reg)>>(lfsr_desc->feedback_bitsel0))&0x01;
		fb1=((context->lfsr_reg)>>(lfsr_desc->feedback_bitsel1))&0x01;
		/* Now do the combo on them */
		fbresult=dss_lfsr_function(lfsr_desc->feedback_function0,fb0,fb1,0x01);
		context->lfsr_reg=dss_lfsr_function(DISC_LFSR_REPLACE,(context->lfsr_reg), fbresult<<(lfsr_desc->bitlength), ((2<<(lfsr_desc->bitlength))-1));

		/* Now select the output bit */
		node->output=((context->lfsr_reg)>>(lfsr_desc->output_bit))&0x01;

		/* Final inversion if required */
		if(lfsr_desc->output_invert) node->output=(node->output)?0.0:1.0;

		/* Gain stage */
		node->output=(node->output)?(node->input3)/2:-(node->input3)/2;
		/* Bias input as required */
		node->output=node->output+node->input5;
	}

	/* If disabled then clamp the output to DC Bias */
	if(!node->input0)
	{
		node->output=node->input5;
	}

	return 0;
}

int dss_lfsr_reset(struct node_description *node)
{
	struct dss_lfsr_context *context;
	struct discrete_lfsr_desc *lfsr_desc;

	context=(struct dss_lfsr_context*)node->context;
	lfsr_desc=(struct discrete_lfsr_desc*)(node->custom);

	context->lfsr_reg=((struct discrete_lfsr_desc*)(node->custom))->reset_value;

	context->lfsr_reg=dss_lfsr_function(DISC_LFSR_REPLACE,0, (dss_lfsr_function(lfsr_desc->feedback_function0,0,0,0x01))<<(lfsr_desc->bitlength),((2<<(lfsr_desc->bitlength))-1));

	/* Now select and setup the output bit */
	node->output=((context->lfsr_reg)>>(lfsr_desc->output_bit))&0x01;

	/* Final inversion if required */
	if(lfsr_desc->output_invert) node->output=(node->output)?0.0:1.0;

	/* Gain stage */
	node->output=(node->output)?(node->input3)/2:-(node->input3)/2;
	/* Bias input as required */
	node->output=node->output+node->input5;

	return 0;
}

int dss_lfsr_init(struct node_description *node)
{
	/* Allocate memory for the context array and the node execution order array */
	if((node->context=(struct dss_lfsr_context*)malloc(sizeof(struct dss_lfsr_context)))==NULL)
	{
		return 1;
	}
	else
	{
		/* Initialise memory */
		memset(node->context,0,sizeof(struct dss_lfsr_context));
	}

	/* Initialise the object */
	dss_lfsr_reset(node);
	((struct dss_lfsr_context*)node->context)->phase=0.0;

	return 0;
}

int dss_lfsr_kill(struct node_description *node)
{
	free(node->context);
	node->context=NULL;
	return 0;
}
