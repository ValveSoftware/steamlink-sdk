#include "driver.h"
#include <stdio.h>
#include <stdarg.h>
#include <math.h>

/************************************************************************/
/*                                                                      */
/*  MAME - Discrete sound system emulation library                      */
/*                                                                      */
/*  Written by Keith Wilkins (mame@esplexo.co.uk)                       */
/*                                                                      */
/*  (c) K.Wilkins 2000                                                  */
/*                                                                      */
/*  Coding started in November 2000                                     */
/*  KW - Added Sawtooth waveforms  Feb2003                              */
/*                                                                      */
/************************************************************************/
/*                                                                      */
/* SEE DISCRETE.H for documentation on usage                            */
/*                                                                      */
/************************************************************************/
/*                                                                      */
/* Each sound primative DSS_xxxx or DST_xxxx has its own implementation */
/* file. All discrete sound primatives MUST implement the following     */
/* API:                                                                 */
/*                                                                      */
/* dsX_NAME_init() returns pointer to context or -1 for failure         */
/* dsX_NAME_step(inputs, context,float timestep)  - Perform time step   */
/*                                                  return output value */
/* dsX_NAME_kill(context) - Free context memory and return              */
/* dsX_NAME_reset(context) - Reset to initial state                     */
/*                                                                      */
/* Core software takes care of traversing the netlist in the correct    */
/* order                                                                */
/*                                                                      */
/* discrete_sh_start()       - Read Node list, initialise & reset       */
/* discrete_sh_stop()        - Shutdown discrete sound system           */
/* discrete_sh_reset()       - Put sound system back to time 0          */
/* discrete_sh_update()      - Update streams to current time           */
/* discrete_stream_update()  - This does the real update to the sim     */
/*                                                                      */
/************************************************************************/
/*                                                                      */
/* Context -  Memory pointer (void)                                     */
/*            Last output value                                         */
/*            Next object in list (Doubly linked?? or Array of struct)  */
/*                                                                      */
/* Initialisation -                                                     */
/*     Parse table into linked object list                              */
/*     Based on blocking/input tree sort list, perhaps the best         */
/*     solution is to traverse the netlist backwards to work out the    */
/*     dependancies, how will this react to feedback loops.             */
/*     Call init in order                                               */
/*                                                                      */
/* Runtime - Use streams interface and callback to update channels      */
/*     Calc number of steps and deltaT                                  */
/*       Traverse list with step for each object                        */
/*       Store output/s in streaming buffer                             */
/*     Repeat until all steps done                                      */
/*                                                                      */
/* Shutdown -                                                           */
/*     Tranverse list                                                   */
/*       Call _kill for each item                                       */
/*     Free core memory array list.                                     */
/*                                                                      */
/************************************************************************/

static int init_ok=0;
static struct node_description **running_order=NULL;
static int node_count=0;
static struct node_description *node_list=NULL;
static struct node_description *output_node=NULL;
static int discrete_stream=0;
static int discrete_stereo=0;

/* Include simulation objects */
#include "disc_wav.cpp"		/* Wave sources   - SINE/SQUARE/NOISE/etc */
#include "disc_mth.cpp"		/* Math Devices   - ADD/GAIN/etc */
#include "disc_inp.cpp"		/* Input Devices  - INPUT/CONST/etc */
#include "disc_flt.cpp"		/* Filter Devices - RCF/HPF/LPF */
#include "disc_dev.cpp"		/* Popular Devices - NE555/etc */
#include "disc_out.cpp"		/* Output devices */

/************************************************************************/
/*                                                                      */
/*        Define the call tables for running the simulation,            */
/*        add your new node types into here to allow them to be         */
/*        called within the simulation environment.                     */
/*                                                                      */
/************************************************************************/
struct discrete_module module_list[]=
{
	{ DSS_INPUT       ,"DSS_INPUT"       ,dss_input_init       ,dss_input_kill       ,dss_input_reset       ,dss_input_step       },
	{ DSS_CONSTANT    ,"DSS_CONSTANT"    ,NULL                 ,NULL                 ,NULL                  ,dss_constant_step    },
	{ DSS_ADJUSTMENT  ,"DSS_ADJUSTMENT"  ,dss_adjustment_init  ,dss_adjustment_kill  ,dss_adjustment_reset  ,dss_adjustment_step  },
	{ DSS_SQUAREWAVE  ,"DSS_SQUAREWAVE"  ,dss_squarewave_init  ,dss_squarewave_kill  ,dss_squarewave_reset  ,dss_squarewave_step  },
	{ DSS_SINEWAVE    ,"DSS_SINEWAVE"    ,dss_sinewave_init    ,dss_sinewave_kill    ,dss_sinewave_reset    ,dss_sinewave_step    },
	{ DSS_NOISE       ,"DSS_NOISE"       ,dss_noise_init       ,dss_noise_kill       ,dss_noise_reset       ,dss_noise_step       },
	{ DSS_LFSR_NOISE  ,"DSS_LFSR_NOISE"  ,dss_lfsr_init        ,dss_lfsr_kill        ,dss_lfsr_reset        ,dss_lfsr_step        },
	{ DSS_TRIANGLEWAVE,"DSS_TRIANGLEWAVE",dss_trianglewave_init,dss_trianglewave_kill,dss_trianglewave_reset,dss_trianglewave_step},
	{ DSS_SAWTOOTHWAVE,"DSS_SAWTOOTHWAVE",dss_sawtoothwave_init,dss_sawtoothwave_kill,dss_sawtoothwave_reset,dss_sawtoothwave_step},

	{ DST_GAIN        ,"DST_GAIN"        ,NULL                 ,NULL                 ,NULL                  ,dst_gain_step        },
	{ DST_DIVIDE      ,"DST_DIVIDE"      ,NULL                 ,NULL                 ,NULL                  ,dst_divide_step      },
	{ DST_ADDER       ,"DST_ADDER"       ,NULL                 ,NULL                 ,NULL                  ,dst_adder_step       },
	{ DST_SWITCH      ,"DST_SWITCH"      ,NULL                 ,NULL                 ,NULL                  ,dst_switch_step      },
	{ DST_RCFILTER    ,"DST_RCFILTER"    ,dst_rcfilter_init    ,NULL                 ,dst_rcfilter_reset    ,dst_rcfilter_step    },
	{ DST_RCDISC      ,"DST_RCDISC"      ,dst_rcdisc_init      ,dst_rcdisc_kill      ,dst_rcdisc_reset      ,dst_rcdisc_step      },
	{ DST_RCDISC2     ,"DST_RCDISC2"     ,dst_rcdisc2_init     ,dst_rcdisc2_kill     ,dst_rcdisc2_reset     ,dst_rcdisc2_step     },
	{ DST_RAMP        ,"DST_RAMP"        ,dst_ramp_init        ,dst_ramp_kill        ,dst_ramp_reset        ,dst_ramp_step        },
	{ DST_CLAMP       ,"DST_CLAMP"       ,NULL                 ,NULL                 ,NULL                  ,dst_clamp_step       },
	{ DST_LADDER      ,"DST_LADDER"      ,dst_ladder_init      ,dst_ladder_kill      ,dst_ladder_reset      ,dst_ladder_step      },
	{ DST_ONESHOT     ,"DST_ONESHOT"     ,dst_oneshot_init     ,dst_oneshot_kill     ,dst_oneshot_reset     ,dst_oneshot_step     },
	{ DST_SAMPHOLD    ,"DST_SAMPHOLD"    ,dst_samphold_init    ,dst_samphold_kill    ,dst_samphold_reset    ,dst_samphold_step    },

	{ DST_LOGIC_INV   ,"DST_LOGIC_INV"   ,NULL                 ,NULL                 ,NULL                  ,dst_logic_inv_step   },
	{ DST_LOGIC_AND   ,"DST_LOGIC_AND"   ,NULL                 ,NULL                 ,NULL                  ,dst_logic_and_step   },
	{ DST_LOGIC_NAND  ,"DST_LOGIC_NAND"  ,NULL                 ,NULL                 ,NULL                  ,dst_logic_nand_step  },
	{ DST_LOGIC_OR    ,"DST_LOGIC_OR"    ,NULL                 ,NULL                 ,NULL                  ,dst_logic_or_step    },
	{ DST_LOGIC_NOR   ,"DST_LOGIC_NOR"   ,NULL                 ,NULL                 ,NULL                  ,dst_logic_nor_step   },
	{ DST_LOGIC_XOR   ,"DST_LOGIC_XOR"   ,NULL                 ,NULL                 ,NULL                  ,dst_logic_xor_step   },
	{ DST_LOGIC_NXOR  ,"DST_LOGIC_NXOR"  ,NULL                 ,NULL                 ,NULL                  ,dst_logic_nxor_step  },

/*	{ DSD_NE555       ,"DSD_NE555"       ,dsd_ne555_init       ,dsd_ne555_kill       ,dsd_ne555_reset       ,dsd_ne555_step       }, */

	{ DSO_OUTPUT      ,"DSO_OUTPUT"      ,dso_output_init      ,NULL                 ,NULL                  ,dso_output_step      },
	{ DSS_NULL        ,"DSS_NULL"        ,NULL                 ,NULL                 ,NULL                  ,NULL                 }
};


static struct node_description* find_node(int node)
{
	int loop;
	for(loop=0;loop<node_count;loop++)
	{
		if(node_list[loop].node==node) return &node_list[loop];
	}
	return NULL;
}

static void discrete_stream_update_stereo(int ch, INT16 **buffer, int length)
{
	/* Now we must do length iterations of the node list, one output for each step */
	int loop,loop2;
	struct node_description *node;

	for(loop=0;loop<length;loop++)
	{
		for(loop2=0;loop2<node_count;loop2++)
		{
			/* Pick the first node to process */
			node=running_order[loop2];

			/* Work out what nodes/inputs are required, dont process NO CONNECT nodes */
			/* these are ones that are connected to NODE_LIST[0]                      */
			if(node->input_node0 && (node->input_node0)->node!=NODE_NC) node->input0=(node->input_node0)->output;
			if(node->input_node1 && (node->input_node1)->node!=NODE_NC) node->input1=(node->input_node1)->output;
			if(node->input_node2 && (node->input_node2)->node!=NODE_NC) node->input2=(node->input_node2)->output;
			if(node->input_node3 && (node->input_node3)->node!=NODE_NC) node->input3=(node->input_node3)->output;
			if(node->input_node4 && (node->input_node4)->node!=NODE_NC) node->input4=(node->input_node4)->output;
			if(node->input_node5 && (node->input_node5)->node!=NODE_NC) node->input5=(node->input_node5)->output;

			/* Now step the node */
			if(module_list[node->module].step) (*module_list[node->module].step)(node);
		}

		/* Now put the output into the buffers */
		buffer[0][loop]=((struct dso_output_context*)(output_node->context))->left;
		buffer[1][loop]=((struct dso_output_context*)(output_node->context))->right;
	}
}

static void discrete_stream_update_mono(int ch,INT16 *buffer, int length)
{
	/* Now we must do length iterations of the node list, one output for each step */
	int loop,loop2;
	struct node_description *node;

	for(loop=0;loop<length;loop++)
	{
		for(loop2=0;loop2<node_count;loop2++)
		{
			/* Pick the first node to process */
			node=running_order[loop2];

			/* Work out what nodes/inputs are required, dont process NO CONNECT nodes */
			/* these are ones that are connected to NODE_LIST[0]                      */
			if(node->input_node0 && (node->input_node0)->node!=NODE_NC) node->input0=(node->input_node0)->output;
			if(node->input_node1 && (node->input_node1)->node!=NODE_NC) node->input1=(node->input_node1)->output;
			if(node->input_node2 && (node->input_node2)->node!=NODE_NC) node->input2=(node->input_node2)->output;
			if(node->input_node3 && (node->input_node3)->node!=NODE_NC) node->input3=(node->input_node3)->output;
			if(node->input_node4 && (node->input_node4)->node!=NODE_NC) node->input4=(node->input_node4)->output;
			if(node->input_node5 && (node->input_node5)->node!=NODE_NC) node->input5=(node->input_node5)->output;

			/* Now step the node */
			if(module_list[node->module].step) (*module_list[node->module].step)(node);
		}

		/* Now put the output into the buffer */
		buffer[loop]=(((struct dso_output_context*)(output_node->context))->left+((struct dso_output_context*)(output_node->context))->right)/2;
	}
}

int discrete_sh_start (const struct MachineSound *msound)
{
	struct discrete_sound_block *intf;
	int loop=0,loop2=0,search=0,failed=0;

	/* Initialise */
	intf=(discrete_sound_block*)msound->sound_interface;
	node_count=0;

	/* Sanity check and node count */
	while(1)
	{
		/* Check the node parameter is a valid node */
		if(intf[node_count].node<NODE_START || intf[node_count].node>NODE_END)
		{
			logerror("discrete_sh_start() - Invalid node number on node %02d descriptor\n",node_count);
			return 1;
		}
		if(intf[node_count].type>DSO_OUTPUT)
		{
			logerror("discrete_sh_start() - Invalid function type on node %02d descriptor\n",node_count);
			return 1;
		}

		/* Node count must include the NULL node as well */
		if(intf[node_count].type==DSS_NULL)
		{
			node_count++;
			break;
		}

		node_count++;

		/* Sanity check */
		if(node_count>DISCRETE_MAX_NODES)
		{
			logerror("discrete_sh_start() - Upper limit of 255 nodes exceeded, have you terminated the interface block.");
			return 1;
		}
	}

	/* Allocate memory for the context array and the node execution order array */
	if((running_order=(struct node_description**)malloc(node_count*sizeof(struct node_description*)))==NULL)
	{
		logerror("discrete_sh_start() - Failed to allocate running order array.\n");
		return 1;
	}
	else
	{
		/* Initialise memory */
		memset(running_order,0,node_count*sizeof(struct node_description*));
	}

	if((node_list=(struct node_description*)malloc(node_count*sizeof(struct node_description)))==NULL)
	{
		logerror("discrete_sh_start() - Failed to allocate context list array.\n");
		return 1;
	}
	else
	{
		/* Initialise memory */
		memset(node_list,0,node_count*sizeof(struct node_description));
	}

	/* Work out the execution order */
	/* FAKE IT FOR THE MOMENT, EXECUTE IN ORDER */
	for(loop=0;loop<node_count;loop++)
	{
		running_order[loop]=&node_list[loop];
	}

	/* Configure the input node pointers, the find_node function wont work without the node ID setup beforehand */
	for(loop=0;loop<node_count;loop++) node_list[loop].node=intf[loop].node;
	failed=0;

	/* Duplicate node number test */
	for(loop=0;loop<node_count;loop++)
	{
		for(loop2=0;loop2<node_count;loop2++)
		{
			if(node_list[loop].node==node_list[loop2].node && loop!=loop2)
			{
				logerror("discrete_sh_start - Node NODE_%02d defined more than once\n",node_list[loop].node-NODE_00);
				failed=1;
			}
		}
	}

	/* Initialise and start all of the objects */
	for(loop=0;loop<node_count;loop++)
	{
		/* Configure the input node pointers */
		node_list[loop].node=intf[loop].node;
		node_list[loop].output=0;
		node_list[loop].input0=intf[loop].initial0;
		node_list[loop].input1=intf[loop].initial1;
		node_list[loop].input2=intf[loop].initial2;
		node_list[loop].input3=intf[loop].initial3;
		node_list[loop].input4=intf[loop].initial4;
		node_list[loop].input5=intf[loop].initial5;
		node_list[loop].input_node0=find_node(intf[loop].input_node0);
		node_list[loop].input_node1=find_node(intf[loop].input_node1);
		node_list[loop].input_node2=find_node(intf[loop].input_node2);
		node_list[loop].input_node3=find_node(intf[loop].input_node3);
		node_list[loop].input_node4=find_node(intf[loop].input_node4);
		node_list[loop].input_node5=find_node(intf[loop].input_node5);
		node_list[loop].name=intf[loop].name;
		node_list[loop].custom=intf[loop].custom;

		/* Check that all referenced nodes have actually been found */
		if(node_list[loop].input_node0==NULL && intf[loop].input_node0>=NODE_START && intf[loop].input_node0<=NODE_END)
		{
			logerror("discrete_sh_start - Node NODE_%02d referenced a non existant node NODE_%02d\n",node_list[loop].node-NODE_00,intf[loop].input_node0-NODE_00);
			failed=1;
		}
		if(node_list[loop].input_node1==NULL && intf[loop].input_node1>=NODE_START && intf[loop].input_node1<=NODE_END)
		{
			logerror("discrete_sh_start - Node NODE_%02d referenced a non existant node NODE_%02d\n",node_list[loop].node-NODE_00,intf[loop].input_node1-NODE_00);
			failed=1;
		}
		if(node_list[loop].input_node2==NULL && intf[loop].input_node2>=NODE_START && intf[loop].input_node2<=NODE_END)
		{
			logerror("discrete_sh_start - Node NODE_%02d referenced a non existant node NODE_%02d\n",node_list[loop].node-NODE_00,intf[loop].input_node2-NODE_00);
			failed=1;
		}
		if(node_list[loop].input_node3==NULL && intf[loop].input_node3>=NODE_START && intf[loop].input_node3<=NODE_END)
		{
			logerror("discrete_sh_start - Node NODE_%02d referenced a non existant node NODE_%02d\n",node_list[loop].node-NODE_00,intf[loop].input_node3-NODE_00);
			failed=1;
		}
		if(node_list[loop].input_node4==NULL && intf[loop].input_node4>=NODE_START && intf[loop].input_node4<=NODE_END)
		{
			logerror("discrete_sh_start - Node NODE_%02d referenced a non existant node NODE_%02d\n",node_list[loop].node-NODE_00,intf[loop].input_node4-NODE_00);
			failed=1;
		}
		if(node_list[loop].input_node5==NULL && intf[loop].input_node5>=NODE_START && intf[loop].input_node5<=NODE_END)
		{
			logerror("discrete_sh_start - Node NODE_%02d referenced a non existant node NODE_%02d\n",node_list[loop].node-NODE_00,intf[loop].input_node5-NODE_00);
			failed=1;
		}

		/* Try to find the simulation module in the module list table */
		search=0;
		while(1)
		{
			if(module_list[search].type==intf[loop].type)
			{
				node_list[loop].module=search;
				if(module_list[search].init)
					if(((*module_list[search].init)(&node_list[loop]))==1) failed=1;
				break;
			}
			else if(module_list[search].type==DSS_NULL)
			{
				if(intf[loop].type==DSS_NULL) break;
				else
				{
					logerror("discrete_sh_start() - Invalid DSS/DST/DSO module type specified in interface, item %02d\n",loop+1);
					failed=1;
					break;
				}
			}
			search++;
		}
	}
	/* Setup the output node */
	if((output_node=find_node(NODE_OP))==NULL)
	{
		logerror("discrete_sh_start() - Counldnt find an output node");
		failed=1;
	}

	/* Different setup for Mono/Stereo systems */
	if ((Machine->drv->sound_attributes&SOUND_SUPPORTS_STEREO) == SOUND_SUPPORTS_STEREO)
	{
		int vol[2];
		const char *stereo_names[2] = { "Discrete Left", "Discrete Right" };
		vol[0] = output_node->input2;
		vol[1] = output_node->input2;
		/* Initialise a stereo, stream, we always use stereo even if only a mono system */
		discrete_stream=stream_init_multi(2,stereo_names,vol,Machine->sample_rate,0,discrete_stream_update_stereo);
		discrete_stereo=1;
	}
	else
	{
		int vol;
		vol = output_node->input2;
		/* Initialise a stereo, stream, we always use stereo even if only a mono system */
		discrete_stream=stream_init("Discrete Sound",vol,Machine->sample_rate,0,discrete_stream_update_mono);
	}

	if(discrete_stream==-1)
	{
		logerror("discrete_sh_start - Stream init returned an error\n");
		failed=1;
	}

	/* Report success or fail */
	if(!failed) init_ok=1;
	return failed;
}

void discrete_sh_stop (void)
{
	int loop=0;
	if(!init_ok) return;

	for(loop=0;loop<node_count;loop++)
	{
    	/* Destruct all of the objects */
		if(module_list[node_list[loop].module].kill) (*module_list[node_list[loop].module].kill)(&node_list[loop]);
	}
	if(node_list) free(node_list);
	if(running_order) free(running_order);
	node_count=0;
	node_list=NULL;
	running_order=NULL;
}

void discrete_sh_reset (void)
{
	/* Reset all of the objects */
	int loop=0;

	if(!init_ok) return;

	for(loop=0;loop<node_count;loop++)
	{
		if(module_list[node_list[loop].module].reset) (*module_list[node_list[loop].module].reset)(&node_list[loop]);
	}
}

void discrete_sh_update(void)
{
	if(!init_ok) return;

	/* Bring stream upto the present time */
	stream_update(discrete_stream, 0);
}
