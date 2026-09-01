// CreateInterface from GoldSrc body + InitializeInterface for VGUI factories.
#include "interface.h"

void *InitializeInterface(char const *interfaceName, CreateInterfaceFn *factoryList, int numFactories)
{
	if (!interfaceName || !factoryList)
		return nullptr;
	for (int i = 0; i < numFactories; i++)
	{
		if (!factoryList[i])
			continue;
		void *retval = factoryList[i](interfaceName, nullptr);
		if (retval)
			return retval;
	}
	return nullptr;
}
