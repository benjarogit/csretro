// Test fixture for the address resolver. Built as a shared library so the
// resolver can be exercised with a dynamic symbol, a static symbol and a byte
// pattern, without loading the retail engine.

extern "C"
{
    static int ResolverTest_Static(int value)
    {
        return value * 3;
    }

    static int ResolverTest_PatternTarget(int value)
    {
        // The immediate stays in the instruction stream and is unique here.
        volatile unsigned marker = 0xC5E7A123u;

        return value + static_cast<int>(marker);
    }

    __attribute__((visibility("default"))) int ResolverTest_Exported(int value)
    {
        return value + 1;
    }

    __attribute__((visibility("default"))) void* ResolverTest_GetStaticAddress()
    {
        return reinterpret_cast<void*>(&ResolverTest_Static);
    }

    __attribute__((visibility("default"))) void* ResolverTest_GetPatternTargetAddress()
    {
        return reinterpret_cast<void*>(&ResolverTest_PatternTarget);
    }
}
