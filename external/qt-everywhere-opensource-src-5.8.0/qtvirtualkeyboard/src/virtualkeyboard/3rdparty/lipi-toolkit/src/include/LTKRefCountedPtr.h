/*****************************************************************************************
* Copyright (c) 2006 Hewlett-Packard Development Company, L.P.
* Permission is hereby granted, free of charge, to any person obtaining a copy of
* this software and associated documentation files (the "Software"), to deal in
* the Software without restriction, including without limitation the rights to use,
* copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the
* Software, and to permit persons to whom the Software is furnished to do so,
* subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
* INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
* PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
* HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
* CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE
* OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. 
*****************************************************************************************/

/************************************************************************
 * FILE DESCR: Definitions of Agglomerative Hierarchical Clustering module
 *
 * CONTENTS:   
 *		
 *
 * AUTHOR:     Bharath A
 *
 * DATE:       December 14, 2007
 * CHANGE HISTORY:
 * Author		Date			Description of change
 ************************************************************************/
 
#ifndef __LTKREFCOUNTEDPTR_H
#define __LTKREFCOUNTEDPTR_H

template <class TargetClass>
class LTKRefCountedPtr
{
      
private:

//instance of this counter is maintained for each object
struct SharedCounter 
{
    TargetClass* objPtr; //target object
    int refCount;        //count of current references to target object
    
    SharedCounter(TargetClass* ptr = NULL)
    {
        if(ptr!=NULL)
        {
          objPtr = ptr;
          refCount = 1;  
        }
        else
        {
          objPtr = NULL;    
        }            
    
    }

}* m_sharedCounterPtr; //pointer to common counter instance
    
    
public:
/**********************************************************************************
* AUTHOR		: Bharath A
* DATE			: 14-DEC-2007
* NAME			: LTKRefCountedPtr
* DESCRIPTION	: Constructor to create a shared counter instance and point to a new object
* ARGUMENTS		: newObj - new object that needs to be tracked
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/

    //"explicit" keyword required to avoid construction from pointers of arbitrary type
    explicit LTKRefCountedPtr(TargetClass* newObj = NULL) 
    {
        if(newObj!=NULL)
        {
             //creating the shared counter for the new object
             m_sharedCounterPtr = new SharedCounter(newObj);
             
        }
        else
        {
            m_sharedCounterPtr = NULL;    
        }
    }
    
/**********************************************************************************
* AUTHOR		: Bharath A
* DATE			: 14-DEC-2007
* NAME			: LTKRefCountedPtr
* DESCRIPTION	: Copy constructor
* ARGUMENTS		: inObj - input object to get constructed from
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/    
     LTKRefCountedPtr(const LTKRefCountedPtr& inObj)
    {
         m_sharedCounterPtr = inObj.m_sharedCounterPtr;
        
        if(m_sharedCounterPtr!=NULL)
        {
           //incrementing the number of current references to the input object
           m_sharedCounterPtr->refCount = m_sharedCounterPtr->refCount + 1;
        }
 
     }

/**********************************************************************************
* AUTHOR		: Bharath A
* DATE			: 14-DEC-2007
* NAME			: operator=
* DESCRIPTION	: Assignment operator overloading
* ARGUMENTS		: inObj - input object to get assigned
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/    
    LTKRefCountedPtr& operator=(const LTKRefCountedPtr& inObj)
    {
        if (this != &inObj)
        {
           if(m_sharedCounterPtr!=NULL)
           {
               //decrementing the number of references to the object poninted to
               m_sharedCounterPtr->refCount = m_sharedCounterPtr->refCount - 1;
               
                //if there are no references to the object, delete the object
                //and the shared counter
                if(m_sharedCounterPtr->refCount == 0)
                {
                    delete m_sharedCounterPtr->objPtr;
                    
                    delete m_sharedCounterPtr;
                }
                
                m_sharedCounterPtr = NULL;
           }
       
           //start pointing to the new object's shared counter
           m_sharedCounterPtr = inObj.m_sharedCounterPtr;
           
           if(m_sharedCounterPtr!=NULL)
           {
              //incrementing the number of current references to the new object
              m_sharedCounterPtr->refCount = m_sharedCounterPtr->refCount + 1;
           }
                
        }
        return *this;
    }
    
/**********************************************************************************
* AUTHOR		: Bharath A
* DATE			: 14-DEC-2007
* NAME			: operator*
* DESCRIPTION	: Dereferencing operator overloading
* ARGUMENTS		: 
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/        
    TargetClass& operator*()  const
    {
       return *m_sharedCounterPtr->objPtr;
    }
    
/**********************************************************************************
* AUTHOR		: Bharath A
* DATE			: 14-DEC-2007
* NAME			: operator->
* DESCRIPTION	: Arrow operator overloading
* ARGUMENTS		: 
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/       
    inline TargetClass* operator->() const
    {
      return m_sharedCounterPtr->objPtr;
    }

/**********************************************************************************
* AUTHOR		: Bharath A
* DATE			: 14-DEC-2007
* NAME			: ~LTKRefCountedPtr
* DESCRIPTION	: Destructor
* ARGUMENTS		: 
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/      
     ~LTKRefCountedPtr()
    {
        if(m_sharedCounterPtr!=NULL)
        {
            //decrementing the nubmer of current references to the pointed to
            m_sharedCounterPtr->refCount = m_sharedCounterPtr->refCount - 1;
            
            //if there are no references to the object, delete the object
            if(m_sharedCounterPtr->refCount == 0)
            {
                delete m_sharedCounterPtr->objPtr;
                delete m_sharedCounterPtr;
            }
            
            m_sharedCounterPtr = NULL;
        }
    }
        
 };

#endif

