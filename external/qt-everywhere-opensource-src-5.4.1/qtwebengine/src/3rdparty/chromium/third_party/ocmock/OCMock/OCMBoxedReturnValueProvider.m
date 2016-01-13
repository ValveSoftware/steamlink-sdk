//---------------------------------------------------------------------------------------
//  $Id$
//  Copyright (c) 2009 by Mulle Kybernetik. See License file for details.
//---------------------------------------------------------------------------------------

#import "OCMBoxedReturnValueProvider.h"


@implementation OCMBoxedReturnValueProvider

- (void)handleInvocation:(NSInvocation *)anInvocation
{
	const char* returnType = [[anInvocation methodSignature] methodReturnType];
	const char* valueType = [(NSValue *)returnValue objCType];
	// ARM64 uses 'B' for BOOLS in method signatures but 'c' in NSValue; that case should match.
	if(strcmp(returnType, valueType) != 0 && !(strcmp(returnType, "B") == 0 && strcmp(valueType, "c") == 0))
		@throw [NSException exceptionWithName:NSInvalidArgumentException reason:@"Return value does not match method signature." userInfo:nil];
	void *buffer = malloc([[anInvocation methodSignature] methodReturnLength]);
	[returnValue getValue:buffer];
	[anInvocation setReturnValue:buffer];
	free(buffer);
}

@end
