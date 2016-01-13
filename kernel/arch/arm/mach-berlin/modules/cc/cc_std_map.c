/******************************************************************************* 
* Copyright (C) Marvell International Ltd. and its affiliates 
* 
 * Marvell GPL License Option
 *
 * If you received this File from Marvell, you may opt to use, redistribute and/or
 * modify this File in accordance with the terms and conditions of the General
 * Public License Version 2, June 1991 (the "GPL License"), a copy of which is
 * available along with the File in the license.txt file or by writing to the Free
 * Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 or
 * on the worldwide web at http://www.gnu.org/licenses/gpl.txt.
 *
 * THE FILE IS DISTRIBUTED AS-IS, WITHOUT WARRANTY OF ANY KIND, AND THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE ARE EXPRESSLY
 * DISCLAIMED.  The GPL License provides additional details about this warranty
 * disclaimer.
 ********************************************************************************/

#include <linux/slab.h>   /* kmalloc() */
#include "cc_std_map.h"


#define GaloisMalloc(x) 	kmalloc(x, GFP_ATOMIC)
#define GaloisFree 		kfree

static VOID std_map_clear_helper(std_map* self, std_map_node* x);
static VOID KeyDtor(std_map* self, PVOID pKey);
static VOID DataDtor(std_map* self, PVOID pData);
static HRESULT Search(std_map* self, PVOID pKey, std_map_node** ppNode);
static void LeftRotate(std_map* self, std_map_node* x);
static void RightRotate(std_map* self, std_map_node* y);
static void InsertHelper(std_map* self, std_map_node* z);
static std_map_node * Insert(std_map* self, void* pKey, void* pData);
static void RBDeleteFixUp(std_map* self, std_map_node* x);
static std_map_node* TreeSuccessor(std_map* self,std_map_node* x);
static std_map_node* TreePredecessor(std_map* self, std_map_node* x);
static HRESULT RBDelete(std_map* self, std_map_node* z);

///// Public Methods
std_map* std_map_ctor(std_map* self,
		      std_map_compare_func compare,
		      std_map_dtor_func key_dtor,
		      std_map_dtor_func data_dtor,
		      std_map_node_type eType)
{
	std_map_node* temp;
	if (!self) {
		self = (std_map*) GaloisMalloc( sizeof(std_map) );
	}

	if (self) {
		self->compare = compare;
		self->key_dtor = key_dtor;
		self->data_dtor = data_dtor;
		self->m_eType = eType;
		self->uiSize = 0;

		temp = self->m_pNil = \
			(std_map_node*) GaloisMalloc(sizeof(std_map_node));
		MV_ASSERT(temp);
		temp->pParent = temp->pLeft = temp->pRight = temp;
		temp->bRed = FALSE;
		temp->pKey = 0;
		temp->pData = 0;

		temp = self->m_pRoot = \
			(std_map_node*) GaloisMalloc(sizeof(std_map_node));
		MV_ASSERT(temp);
		temp->pParent = temp->pLeft = temp->pRight = self->m_pNil;
		temp->bRed = FALSE;
		temp->pKey = 0;
		temp->pData = 0;
	}

	return self;
}

VOID std_map_dtor(std_map* self)
{
	if (self)
	{
		std_map_clear(self);
		GaloisFree(self->m_pRoot);
		GaloisFree(self->m_pNil);
		GaloisFree(self);
	}
}

HRESULT std_map_clear(std_map* self)
{
	std_map_node *temp;

	if (!self) return E_POINTER;

	std_map_clear_helper(self, self->m_pRoot->pLeft);

	temp = self->m_pRoot;
	temp->pParent = temp->pLeft = temp->pRight = self->m_pNil;
	temp->bRed = FALSE;
	temp->pKey = 0;
	self->uiSize = 0;

	return S_OK;
}

HRESULT std_map_find(std_map* self, PVOID pKey, PVOID* ppData)
{
	HRESULT hr;
	std_map_node* x;

	//*ppData = NULL; //when ppData == NULL, Segmentation fault

	if(self == NULL) return E_POINTER;
	if(ppData == NULL) return E_POINTER;

	hr = Search(self, pKey, &x);
	if(hr == S_OK)
	{
		*ppData = x->pData;
		return S_OK;
	}
	else
	{
		*ppData = 0;
		return E_FAIL;
	}
}

HRESULT std_map_insert(std_map* self, PVOID pKey, PVOID pData)
{
	Insert(self, pKey, pData);
	self->uiSize++;
	return S_OK;
}

HRESULT std_map_erase(std_map* self, PVOID pKey)
{
	std_map_node* x;
	Search(self, pKey, &x);
	if(x)
	{
		self->uiSize--;
		return RBDelete(self, x);
	}
	else
	{
		return E_FAIL;
	}
}

HRESULT std_map_size(std_map* self, UINT* puiSize)
{
	if (!self || !puiSize) return E_POINTER;
	(*puiSize) = self->uiSize;
	return S_OK;
}

HRESULT std_map_iter(std_map* self, std_map_node** ppNode)
{
	if(*ppNode == 0)
	{
		*ppNode = self->m_pRoot;
	}
	*ppNode = TreePredecessor(self, *ppNode);
	if(*ppNode == self->m_pNil)
	{
		*ppNode = 0;
	}
	return S_OK;
}

//////////// Private Methods
static
VOID std_map_clear_helper(std_map* self, std_map_node* x)
{
	std_map_node* nil= self->m_pNil;
	if (x != nil) {
		std_map_clear_helper(self,x->pLeft);
		std_map_clear_helper(self,x->pRight);
		if(self->key_dtor) self->key_dtor(x->pKey);
		if(self->data_dtor) self->data_dtor(x->pData);
		GaloisFree(x);
	}
}

static VOID
KeyDtor(std_map* self, PVOID pKey)
{
	if(self->key_dtor) self->key_dtor(pKey);
}

static VOID
DataDtor(std_map* self, PVOID pData)
{
	if(self->data_dtor) self->data_dtor(pData);
}

static
HRESULT Search(std_map* self, PVOID pKey, std_map_node** ppNode)
{
	std_map_node* x=self->m_pRoot->pLeft;
	std_map_node* nil=self->m_pNil;
	int compVal;

	*ppNode = NULL;

	if (x == nil) return E_FAIL;
	compVal=self->compare(x->pKey, pKey);
	while(0 != compVal)
	{
		if (1 == compVal)
		{ /* x->key > q */
			x=x->pLeft;
		}
		else
		{
			x=x->pRight;
		}
		if ( x == nil)
		{
			*ppNode = 0;
			return E_FAIL;
		}
		compVal=self->compare(x->pKey, pKey);
	}
	*ppNode = x;
	return S_OK;
}

static
void LeftRotate(std_map* self, std_map_node* x)
{
	std_map_node* y;
	std_map_node* nil=self->m_pNil;

	/*  I originally wrote this function to use the sentinel for */
	/*  nil to avoid checking for nil.  However this introduces a */
	/*  very subtle bug because sometimes this function modifies */
	/*  the parent pointer of nil.  This can be a problem if a */
	/*  function which calls LeftRotate also uses the nil sentinel */
	/*  and expects the nil sentinel's parent pointer to be unchanged */
	/*  after calling this function.  For example, when RBDeleteFixUP */
	/*  calls LeftRotate it expects the parent pointer of nil to be */
	/*  unchanged. */

	y=x->pRight;
	x->pRight=y->pLeft;

	if (y->pLeft != nil) y->pLeft->pParent=x; /* used to use sentinel here */
	/* and do an unconditional assignment instead of testing for nil */

	y->pParent=x->pParent;

	/* instead of checking if x->pParent is the root as in the book, we */
	/* count on the root sentinel to implicitly take care of this case */
	if( x == x->pParent->pLeft) {
		x->pParent->pLeft=y;
	} else {
		x->pParent->pRight=y;
	}
	y->pLeft=x;
	x->pParent=y;
}

static
void RightRotate(std_map* self, std_map_node* y)
{
	std_map_node* x;
	std_map_node* nil=self->m_pNil;

	/*  I originally wrote this function to use the sentinel for */
	/*  nil to avoid checking for nil.  However this introduces a */
	/*  very subtle bug because sometimes this function modifies */
	/*  the parent pointer of nil.  This can be a problem if a */
	/*  function which calls LeftRotate also uses the nil sentinel */
	/*  and expects the nil sentinel's parent pointer to be unchanged */
	/*  after calling this function.  For example, when RBDeleteFixUP */
	/*  calls LeftRotate it expects the parent pointer of nil to be */
	/*  unchanged. */

	x=y->pLeft;
	y->pLeft=x->pRight;

	if (nil != x->pRight)  x->pRight->pParent=y; /*used to use sentinel here */
	/* and do an unconditional assignment instead of testing for nil */

	/* instead of checking if x->pParent is the root as in the book, we */
	/* count on the root sentinel to implicitly take care of this case */
	x->pParent=y->pParent;
	if( y == y->pParent->pLeft) {
		y->pParent->pLeft=x;
	} else {
		y->pParent->pRight=x;
	}
	x->pRight=y;
	y->pParent=x;
}

static
void InsertHelper(std_map* self, std_map_node* z)
{
	/*  This function should only be called by InsertRBTree (see above) */
	std_map_node* x;
	std_map_node* y;
	std_map_node* nil=self->m_pNil;

	z->pLeft=z->pRight=nil;
	y=self->m_pRoot;
	x=self->m_pRoot->pLeft;
	while( x != nil) {
		y=x;
		if (1 == self->compare(x->pKey,z->pKey))
		{ /* x.pKey > z.pKey */
			x=x->pLeft;
		}
		else
		{ /* x,pKey <= z.pKey */
			x=x->pRight;
		}
	}
	z->pParent=y;
	if ( (y == self->m_pRoot) ||
	     (1 == self->compare(y->pKey,z->pKey)))
	{
		/* y.pKey > z.pKey */
		y->pLeft=z;
	}
	else
	{
		y->pRight=z;
	}
}

static
std_map_node * Insert(std_map* self, void* pKey, void* pData)
{
	std_map_node * y;
	std_map_node * x;
	std_map_node * newNode;

	x=(std_map_node*) GaloisMalloc(sizeof(std_map_node));
	if (!x) return NULL;
	x->pKey=pKey;
	x->pData=pData;

	InsertHelper(self,x);
	newNode=x;
	x->bRed=1;
	while(x->pParent->bRed)
	{ /* use sentinel instead of checking for root */
		if (x->pParent == x->pParent->pParent->pLeft)
		{
			y=x->pParent->pParent->pRight;
			if (y->bRed)
			{
				x->pParent->bRed=0;
				y->bRed=0;
				x->pParent->pParent->bRed=1;
				x=x->pParent->pParent;
			}
			else
			{
				if (x == x->pParent->pRight)
				{
					x=x->pParent;
					LeftRotate(self,x);
				}
				x->pParent->bRed=0;
				x->pParent->pParent->bRed=1;
				RightRotate(self,x->pParent->pParent);
			}
		}
		else
		{
			/* case for x->pParent == x->pParent->pParent->pRight */
			y=x->pParent->pParent->pLeft;
			if (y->bRed)
			{
				x->pParent->bRed=0;
				y->bRed=0;
				x->pParent->pParent->bRed=1;
				x=x->pParent->pParent;
			}
			else
			{
				if (x == x->pParent->pLeft)
				{
					x=x->pParent;
					RightRotate(self,x);
				}
				x->pParent->bRed=0;
				x->pParent->pParent->bRed=1;
				LeftRotate(self,x->pParent->pParent);
			}
		}
	}
	self->m_pRoot->pLeft->bRed=0;
	return(newNode);

}

static
void RBDeleteFixUp(std_map* self, std_map_node* x) {
	std_map_node* root=self->m_pRoot->pLeft;
	std_map_node* w;

	while( (!x->bRed) && (root != x))
	{
		if (x == x->pParent->pLeft)
		{
			w=x->pParent->pRight;
			if (w->bRed)
			{
				w->bRed=0;
				x->pParent->bRed=1;
				LeftRotate(self,x->pParent);
				w=x->pParent->pRight;
			}
			if ( (!w->pRight->bRed) && (!w->pLeft->bRed) )
			{
				w->bRed=1;
				x=x->pParent;
			}
			else
			{
				if (!w->pRight->bRed)
				{
					w->pLeft->bRed=0;
					w->bRed=1;
					RightRotate(self,w);
					w=x->pParent->pRight;
				}
				w->bRed=x->pParent->bRed;
				x->pParent->bRed=0;
				w->pRight->bRed=0;
				LeftRotate(self,x->pParent);
				x=root; /* this is to exit while loop */
			}
		}
		else
		{ /* the code below is has left and right switched from above */
			w=x->pParent->pLeft;
			if (w->bRed)
			{
				w->bRed=0;
				x->pParent->bRed=1;
				RightRotate(self,x->pParent);
				w=x->pParent->pLeft;
			}
			if ( (!w->pRight->bRed) && (!w->pLeft->bRed) )
			{
				w->bRed=1;
				x=x->pParent;
			}
			else
			{
				if (!w->pLeft->bRed)
				{
					w->pRight->bRed=0;
					w->bRed=1;
					LeftRotate(self,w);
					w=x->pParent->pLeft;
				}
				w->bRed=x->pParent->bRed;
				x->pParent->bRed=0;
				w->pLeft->bRed=0;
				RightRotate(self,x->pParent);
				x=root; /* this is to exit while loop */
			}
		}
	}
	x->bRed=0;
}

// Get Successor of x
static
std_map_node* TreeSuccessor(std_map* self,std_map_node* x)
{
	std_map_node* y;
	std_map_node* nil=self->m_pNil;
	std_map_node* root=self->m_pRoot;

	if (nil != (y = x->pRight))
	{
		/* assignment to y is intentional */
		while(y->pLeft != nil)
		{ /* returns the minium of the right subtree of x */
			y=y->pLeft;
		}
		return(y);
	}
	else
	{
		y=x->pParent;
		while(x == y->pRight)
		{
			/* sentinel used instead of checking for nil */
			x=y;
			y=y->pParent;
		}
		if (y == root)
			return(nil);
		return(y);
	}
}

static
std_map_node* TreePredecessor(std_map* self, std_map_node* x)
{
	std_map_node* y;
	std_map_node* nil=self->m_pNil;
	std_map_node* root=self->m_pRoot;

	if (nil != (y = x->pLeft))
	{ /* assignment to y is intentional */
		while(y->pRight != nil)
		{ /* returns the maximum of the left subtree of x */
			y=y->pRight;
		}
		return(y);
	}
	else
	{
		y=x->pParent;
		while(x == y->pLeft)
		{
			if (y == root)
				return(nil);
			x=y;
			y=y->pParent;
		}
		return(y);
	}
}

static
HRESULT RBDelete(std_map* self, std_map_node* z)
{
	std_map_node* y;
	std_map_node* x;
	std_map_node* nil=self->m_pNil;
	std_map_node* root=self->m_pRoot;

	y= ((z->pLeft == nil) || (z->pRight == nil)) \
		? z : TreeSuccessor(self,z);
	x= (y->pLeft == nil) ? y->pRight : y->pLeft;
	if (root == (x->pParent = y->pParent))
	{ /* assignment of y->p to x->p is intentional */
		root->pLeft=x;
	}
	else
	{
		if (y == y->pParent->pLeft)
		{
			y->pParent->pLeft=x;
		} else {
			y->pParent->pRight=x;
		}
	}
	if (y != z)
	{ /* y should not be nil in this case */

		/* y is the node to splice out and x is its child */

		if (!(y->bRed)) RBDeleteFixUp(self,x);

		KeyDtor(self, z->pKey);
		DataDtor(self, z->pData);
		y->pLeft=z->pLeft;
		y->pRight=z->pRight;
		y->pParent=z->pParent;
		y->bRed=z->bRed;
		z->pLeft->pParent=z->pRight->pParent=y;
		if (z == z->pParent->pLeft)
		{
			z->pParent->pLeft=y;
		}
		else
		{
			z->pParent->pRight=y;
		}
		GaloisFree(z);
	}
	else
	{
		KeyDtor(self, y->pKey);
		DataDtor(self, y->pData);
		if (!(y->bRed)) RBDeleteFixUp(self,x);
		GaloisFree(y);
	}
	return S_OK;
}
