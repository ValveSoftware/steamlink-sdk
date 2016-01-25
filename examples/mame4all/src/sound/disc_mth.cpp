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
/* DST_ADDDER            - Multichannel adder                           */
/* DST_GAIN              - Gain Factor                                  */
/* DST_SWITCH            - Switch implementation                        */
/* DST_RAMP              - Ramp up/down                                 */
/* DST_ONESHOT           - One shot pulse generator                     */
/* DST_DIVIDER           - Division function                            */
/* DST_LADDER            - Resistor ladder implementation               */
/* DST_SAMPHOLD          - Sample & Hold Implementation                 */
/* DST_LOGIC_INV         - Logic level invertor                         */
/* DST_LOGIC_AND         - Logic AND gate 4 input                       */
/* DST_LOGIC_NAND        - Logic NAND gate 4 input                      */
/* DST_LOGIC_OR          - Logic OR gate 4 input                        */
/* DST_LOGIC_NOR         - Logic NOR gate 4 input                       */
/* DST_LOGIC_XOR         - Logic XOR gate 2 input                       */
/* DST_LOGIC_NXOR        - Logic NXOR gate 2 input                      */
/*                                                                      */
/************************************************************************/


struct dss_ramp_context
{
        float step;
        int dir;	/* 1 if End is higher then Start */
	int last_en;	/* Keep track of the last enable value */
};

struct dst_oneshot_context
{
	float countdown;
	float stepsize;
	int state;
};

struct dst_ladder_context
{
        int state;
        float t;           // time
        float step;
		float exponent0;
		float exponent1;
};

struct dst_samphold_context
{
		float lastinput;
		int clocktype;
};

/************************************************************************/
/*                                                                      */
/* DST_ADDER - This is a 4 channel input adder with enable function     */
/*                                                                      */
/* input0    - Enable input value                                       */
/* input1    - Channel0 input value                                     */
/* input2    - Channel1 input value                                     */
/* input3    - Channel2 input value                                     */
/* input4    - Channel3 input value                                     */
/* input5    - NOT USED                                                 */
/*                                                                      */
/************************************************************************/
int dst_adder_step(struct node_description *node)
{
	if(node->input0)
	{
		node->output=node->input1 + node->input2 + node->input3 + node->input4;
	}
	else
	{
		node->output=0;
	}
	return 0;
}


/************************************************************************/
/*                                                                      */
/* DST_GAIN - This is a programmable gain module with enable function   */
/*                                                                      */
/* input0    - Enable input value                                       */
/* input1    - Channel0 input value                                     */
/* input2    - Gain value                                               */
/* input3    - NOT USED                                                 */
/* input4    - NOT USED                                                 */
/* input5    - NOT USED                                                 */
/*                                                                      */
/************************************************************************/
int dst_gain_step(struct node_description *node)
{
	if(node->input0)
	{
		node->output=node->input1 * node->input2;
	}
	else
	{
		node->output=0;
	}
	return 0;
}

/************************************************************************/
/*                                                                      */
/* DST_DIVIDE  - Programmable divider with enable                       */
/*                                                                      */
/* input0    - Enable input value                                       */
/* input1    - Channel0 input value                                     */
/* input2    - Divisor                                                  */
/* input3    - NOT USED                                                 */
/* input4    - NOT USED                                                 */
/* input5    - NOT USED                                                 */
/*                                                                      */
/************************************************************************/
int dst_divide_step(struct node_description *node)
{
	if(node->input0)
	{
		if(node->input2==0)
		{
			node->output=2^31;	/* Max out but dont break */

		}
		else
		{
			node->output=node->input1 / node->input2;
		}
	}
	else
	{
		node->output=0;
	}
	return 0;
}

/************************************************************************/
/*                                                                      */
/* DSS_SWITCH - Programmable 2 pole switchmodule with enable function   */
/*                                                                      */
/* input0    - Enable input value                                       */
/* input1    - switch position                                          */
/* input2    - input0                                                   */
/* input3    - input1                                                   */
/* input4    - NOT USED                                                 */
/* input5    - NOT USED                                                 */
/*                                                                      */
/************************************************************************/
int dst_switch_step(struct node_description *node)
{
	if(node->input0)
	{
		/* Input 1 switches between input0/input2 */
		node->output=(node->input1)?node->input3:node->input2;
	}
	else
	{
		node->output=0;
	}
	return 0;
}


/************************************************************************/
/*                                                                      */
/* DST_RAMP - Ramp up/down model usage                                  */
/*                                                                      */
/* input0    - Enable ramp                                              */
/* input1    - Ramp Reverse/Forward switch                              */
/* input2    - Gradient, change/sec                                     */
/* input3    - Start value                                              */
/* input4    - End value                                                */
/* input5    - Clamp value when disabled                                */
/*                                                                      */
/************************************************************************/

int dst_ramp_step(struct node_description *node)
{
	struct dss_ramp_context *context;
	context=(struct dss_ramp_context*)node->context;

	if(node->input0)
	{
		if (!context->last_en)
		{
			context->last_en = 1;
			node->output = node->input3;
		}
		if(context->dir ? node->input1 : !node->input1) node->output+=context->step;
		else node->output-=context->step;
		/* Clamp to min/max */
		if(context->dir ? (node->output < node->input3)
				: (node->output > node->input3)) node->output=node->input3;
		if(context->dir ? (node->output > node->input4)
				: (node->output < node->input4)) node->output=node->input4;
	}
	else
	{
		context->last_en = 0;
		// Disabled so clamp to output
		node->output=node->input5;
	}
	return 0;
}

int dst_ramp_reset(struct node_description *node)
{
	struct dss_ramp_context *context;
	context=(struct dss_ramp_context*)node->context;

	node->output=node->input5;
	context->step = node->input2 / Machine->sample_rate;
	context->dir = ((node->input4 - node->input3) == fabs(node->input4 - node->input3));
	context->last_en = 0;
	return 0;
}

int dst_ramp_init(struct node_description *node)
{
	/* Allocate memory for the context array and the node execution order array */
	if((node->context=(struct dss_ramp_context*)malloc(sizeof(struct dss_ramp_context)))==NULL)
	{
		return 1;
	}
	else
	{
		/* Initialise memory */
		memset(node->context,0,sizeof(struct dss_ramp_context));
	}

	/* Initialise the object */
	dst_ramp_reset(node);
	return 0;
}

int dst_ramp_kill(struct node_description *node)
{
	free(node->context);
	node->context=NULL;
	return 0;
}


/************************************************************************/
/*                                                                      */
/* dst_oneshot - Usage of node_description values for one shot pulse    */
/*                                                                      */
/* input0    - Enable input value                                       */
/* input1    - Trigger value                                            */
/* input2    - Reset value                                              */
/* input3    - Amplitude value                                          */
/* input4    - Width of oneshot pulse                                   */
/* input5    - NOT USED                                                 */
/*                                                                      */
/************************************************************************/
int dst_oneshot_step(struct node_description *node)
{
	struct dst_oneshot_context *context;
	context=(struct dst_oneshot_context*)node->context;

	/* Check state */
	switch(context->state)
	{
		case 0:		/* Waiting for trigger */
			if(node->input1)
			{
				context->state=1;
				context->countdown=node->input4;
				node->output=node->input3;
			}
		 	node->output=0;
			break;

		case 1:		/* Triggered */
			node->output=node->input3;
			if(node->input1 && node->input2)
			{
				// Dont start the countdown if we're still triggering
				// and we've got a reset signal as well
			}
			else
			{
				context->countdown-=context->stepsize;
				if(context->countdown<0.0)
				{
					context->countdown=0;
					node->output=0;
					context->state=2;
				}
			}
			break;

		case 2:		/* Waiting for reset */
		default:
			if(node->input2) context->state=0;
		 	node->output=0;
			break;
	}
	return 0;
}


int dst_oneshot_reset(struct node_description *node)
{
	struct dst_oneshot_context *context=(struct dst_oneshot_context*)node->context;
	context->countdown=0;
	context->stepsize=1.0/Machine->sample_rate;
	context->state=0;
 	node->output=0;
 	return 0;
}

int dst_oneshot_init(struct node_description *node)
{
	/* Allocate memory for the context array and the node execution order array */
	if((node->context=(struct dst_oneshot_context*)malloc(sizeof(struct dst_oneshot_context)))==NULL)
	{
		return 1;
	}
	else
	{
		/* Initialise memory */
		memset(node->context,0,sizeof(struct dst_oneshot_context));
	}

	/* Initialise the object */
	dst_oneshot_reset(node);

	return 0;
}

int dst_oneshot_kill(struct node_description *node)
{
	free(node->context);
	node->context=NULL;
	return 0;
}


/************************************************************************/
/*                                                                      */
/* DST_CLAMP - Simple signal clamping circuit                           */
/*                                                                      */
/* input0    - Enable ramp                                              */
/* input1    - Input value                                              */
/* input2    - Minimum value                                            */
/* input3    - Maximum value                                            */
/* input4    - Clamp when disabled                                      */
/*                                                                      */
/************************************************************************/
int dst_clamp_step(struct node_description *node)
{
	struct dss_ramp_context *context;
	context=(struct dss_ramp_context*)node->context;

	if(node->input0)
	{
		if(node->input1 < node->input2) node->output=node->input2;
		else if(node->input1 > node->input3) node->output=node->input3;
		else node->output=node->input1;
	}
	else
	{
		node->output=node->input4;
	}
	return 0;
}


/************************************************************************/
/*                                                                      */
/* DST_LADDER - Resistor ladder emulation complete with capacitor       */
/*                                                                      */
/* input0    - Enable                                                   */
/* input1    - input0 value                                             */
/* input2    - NOT USED                                                 */
/* input3    - NOT USED                                                 */
/* input4    - NOT USED                                                 */
/* input5    - NOT USED                                                 */
/*                                                                      */
/************************************************************************/
int dst_ladder_step(struct node_description *node)
{
	struct dst_ladder_context *context;
	context=(struct dst_ladder_context*)node->context;
	node->output=0;
	return 0;
}

int dst_ladder_reset(struct node_description *node)
{
	struct dst_ladder_context *context;
	context=(struct dst_ladder_context*)node->context;
	node->output=0;
	return 0;
}

int dst_ladder_init(struct node_description *node)
{
	/* Allocate memory for the context array and the node execution order array */
	if((node->context=(struct dst_ladder_context*)malloc(sizeof(struct dst_ladder_context)))==NULL)
	{
		return 1;
	}
	else
	{
		/* Initialise memory */
		memset(node->context,0,sizeof(struct dst_ladder_context));
	}

	/* Initialise the object */
	dst_ladder_reset(node);
	return 0;
}

int dst_ladder_kill(struct node_description *node)
{
	free(node->context);
	node->context=NULL;
	return 0;
}


/************************************************************************/
/*                                                                      */
/* DST_SAMPHOLD - Sample & Hold Implementation                          */
/*                                                                      */
/* input0    - Enable                                                   */
/* input1    - input0 value                                             */
/* input2    - clock node                                               */
/* input3    - clock type                                               */
/* input4    - NOT USED                                                 */
/* input5    - NOT USED                                                 */
/*                                                                      */
/************************************************************************/
int dst_samphold_step(struct node_description *node)
{
	struct dst_samphold_context *context;
	context=(struct dst_samphold_context*)node->context;

	if(node->input0)
	{
		switch(context->clocktype)
		{
			case DISC_SAMPHOLD_REDGE:
				/* Clock the whole time the input is rising */
				if(node->input2 > context->lastinput) node->output=node->input1;
				break;
			case DISC_SAMPHOLD_FEDGE:
				/* Clock the whole time the input is falling */
				if(node->input2 < context->lastinput) node->output=node->input1;
				break;
			case DISC_SAMPHOLD_HLATCH:
				/* Output follows input if clock != 0 */
				if(node->input2) node->output=node->input1;
				break;
			case DISC_SAMPHOLD_LLATCH:
				/* Output follows input if clock == 0 */
				if(node->input2==0) node->output=node->input1;
				break;
			default:
				break;
		}
	}
	else
	{
		node->output=0;
	}
	/* Save the last value */
	context->lastinput=node->input2;
	return 0;
}

int dst_samphold_reset(struct node_description *node)
{
	struct dst_samphold_context *context;
	context=(struct dst_samphold_context*)node->context;

	node->output=0;
	context->lastinput=-1;
	/* Only stored in here to speed up and save casting in the step function */
	context->clocktype=(int)node->input3;

	return 0;
}

int dst_samphold_init(struct node_description *node)
{
	/* Allocate memory for the context array and the node execution order array */
	if((node->context=(struct dst_samphold_context*)malloc(sizeof(struct dst_samphold_context)))==NULL)
	{
		return 1;
	}
	else
	{
		/* Initialise memory */
		memset(node->context,0,sizeof(struct dst_samphold_context));
	}

	/* Initialise the object */
	dst_samphold_reset(node);
	return 0;
}

int dst_samphold_kill(struct node_description *node)
{
	free(node->context);
	node->context=NULL;
	return 0;
}


/************************************************************************/
/*                                                                      */
/* DST_LOGIC_INV - Logic invertor gate implementation                   */
/*                                                                      */
/* input0    - Enable                                                   */
/* input1    - input0 value                                             */
/* input2    - NOT USED                                                 */
/* input3    - NOT USED                                                 */
/* input4    - NOT USED                                                 */
/* input5    - NOT USED                                                 */
/*                                                                      */
/************************************************************************/
int dst_logic_inv_step(struct node_description *node)
{
	if(node->input0)
	{
		node->output=(node->input1)?0.0:1.0;
	}
	else
	{
		node->output=0.0;
	}
	return 0;
}

/************************************************************************/
/*                                                                      */
/* DST_LOGIC_AND - Logic AND gate implementation                        */
/*                                                                      */
/* input0    - Enable                                                   */
/* input1    - input0 value                                             */
/* input2    - input1 value                                             */
/* input3    - input2 value                                             */
/* input4    - input3 value                                             */
/* input5    - NOT USED                                                 */
/*                                                                      */
/************************************************************************/
int dst_logic_and_step(struct node_description *node)
{
	if(node->input0)
	{
		node->output=(node->input1 && node->input2 && node->input3 && node->input4)?1.0:0.0;
	}
	else
	{
		node->output=0.0;
	}
	return 0;
}

/************************************************************************/
/*                                                                      */
/* DST_LOGIC_NAND - Logic NAND gate implementation                      */
/*                                                                      */
/* input0    - Enable                                                   */
/* input1    - input0 value                                             */
/* input2    - input1 value                                             */
/* input3    - input2 value                                             */
/* input4    - input3 value                                             */
/* input5    - NOT USED                                                 */
/*                                                                      */
/************************************************************************/
int dst_logic_nand_step(struct node_description *node)
{
	if(node->input0)
	{
		node->output=(node->input1 && node->input2 && node->input3 && node->input4)?0.0:1.0;
	}
	else
	{
		node->output=0.0;
	}
	return 0;
}

/************************************************************************/
/*                                                                      */
/* DST_LOGIC_OR  - Logic OR  gate implementation                        */
/*                                                                      */
/* input0    - Enable                                                   */
/* input1    - input0 value                                             */
/* input2    - input1 value                                             */
/* input3    - input2 value                                             */
/* input4    - input3 value                                             */
/* input5    - NOT USED                                                 */
/*                                                                      */
/************************************************************************/
int dst_logic_or_step(struct node_description *node)
{
	if(node->input0)
	{
		node->output=(node->input1 || node->input2 || node->input3 || node->input4)?1.0:0.0;
	}
	else
	{
		node->output=0.0;
	}
	return 0;
}

/************************************************************************/
/*                                                                      */
/* DST_LOGIC_NOR - Logic NOR gate implementation                        */
/*                                                                      */
/* input0    - Enable                                                   */
/* input1    - input0 value                                             */
/* input2    - input1 value                                             */
/* input3    - input2 value                                             */
/* input4    - input3 value                                             */
/* input5    - NOT USED                                                 */
/*                                                                      */
/************************************************************************/
int dst_logic_nor_step(struct node_description *node)
{
	if(node->input0)
	{
		node->output=(node->input1 || node->input2 || node->input3 || node->input4)?0.0:1.0;
	}
	else
	{
		node->output=0.0;
	}
	return 0;
}

/************************************************************************/
/*                                                                      */
/* DST_LOGIC_XOR - Logic XOR gate implementation                        */
/*                                                                      */
/* input0    - Enable                                                   */
/* input1    - input0 value                                             */
/* input2    - input1 value                                             */
/* input3    - NOT USED                                                 */
/* input4    - NOT USED                                                 */
/* input5    - NOT USED                                                 */
/*                                                                      */
/************************************************************************/
int dst_logic_xor_step(struct node_description *node)
{
	if(node->input0)
	{
		node->output=((node->input1 && !node->input2) || (!node->input1 && node->input2))?1.0:0.0;
	}
	else
	{
		node->output=0.0;
	}
	return 0;
}

/************************************************************************/
/*                                                                      */
/* DST_LOGIC_NXOR - Logic NXOR gate implementation                      */
/*                                                                      */
/* input0    - Enable                                                   */
/* input1    - input0 value                                             */
/* input2    - input1 value                                             */
/* input3    - NOT USED                                                 */
/* input4    - NOT USED                                                 */
/* input5    - NOT USED                                                 */
/*                                                                      */
/************************************************************************/
int dst_logic_nxor_step(struct node_description *node)
{
	if(node->input0)
	{
		node->output=((node->input1 && !node->input2) || (!node->input1 && node->input2))?0.0:1.0;
	}
	else
	{
		node->output=0.0;
	}
	return 0;
}
