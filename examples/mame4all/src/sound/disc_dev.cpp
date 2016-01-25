/************************************************************************/
/*                                                                      */
/*  MAME - Discrete sound system emulation library                      */
/*                                                                      */
/*  Written by Keith Wilkins (mame@dysfunction.demon.co.uk)             */
/*                                                                      */
/*  (c) K.Wilkins 2000                                                  */
/*                                                                      */
/************************************************************************/
/*                                                                      */
/* DSS_NE555             - NE555 Simulation code                        */
/*                                                                      */
/************************************************************************/

struct dsd_ne555_context
{
	int flip_flop;
};


/************************************************************************/
/*                                                                      */
/* DSD_NE555 - Usage of node_description values for NE555 function      */
/*                                                                      */
/* input0    - Reset value                                              */
/* input1    - Trigger value                                            */
/* input2    - Threshold value                                          */
/* input3    - Control Voltage value                                    */
/* input4    - VCC Voltage value                                        */
/* input5    - NOT USED                                                 */
/*                                                                      */
/************************************************************************/
int dsd_ne555_step(struct node_description *node)
{
	struct dsd_ne555_context *context=(struct dsd_ne555_context*)node->context;
	int upper,lower;

	/* Check if reset is in action or not connected */
	if((node->input_node0)->module==DSS_NULL || node->input0>0.7)
	{
		/* Non reset scenario */

		/* Check if the control voltage node is connected is it changes the threshold voltage */
		if((node->input_node3)->module==DSS_NULL) node->input3=(node->input4*2.0)/3.0;

		/* Work out the upper/lower comparator states */
		upper=(node->input2>node->input3)?1:0;
		lower=(node->input1>(node->input3/2.0))?1:0;

		/* Now what will the flip-flop do */
		/* UPPER LOWER OUTPUT    OUTPUT'  */
		/*   0     0     0          0     */
		/*   0     0     1          1     */
		/*   0     1     0          1     */
		/*   0     1     1          1     */
		/*   1     0     0          0     */
		/*   1     0     1          0     */
		/*   1     1     0          1     */
		/*   1     1     1          0     */
		/*                                */
		/* Its an S/R flip flop           */
		if(lower && !upper) context->flip_flop=1;
		if(!lower && upper) context->flip_flop=0;
		if(lower && upper)  context->flip_flop=(context->flip_flop)?0:1;
	}
	else
	{
		/* Reset mode */
		context->flip_flop=0;
	}

	/* Output is either Vcc or Zero */
	node->output=(context->flip_flop)?node->input4:0;

	return 0;
}

int dsd_ne555_reset(struct node_description *node)
{
	struct dsd_ne555_context *context;
	context=(struct dsd_ne555_context*)node->context;
	context->flip_flop=0;
	return 0;
}

int dsd_ne555_init(struct node_description *node)
{
	/* Allocate memory for the context array and the node execution order array */
	if((node->context=(struct dsd_ne555_context*)malloc(sizeof(struct dsd_ne555_context)))==NULL)
	{
		return 1;
	}
	else
	{
		/* Initialise memory */
		memset(node->context,0,sizeof(struct dsd_ne555_context));
	}

	/* Initialise the object */
	dsd_ne555_reset(node);
	return 0;
}

int dsd_ne555_kill(struct node_description *node)
{
	free(node->context);
	node->context=NULL;
	return 0;
}


