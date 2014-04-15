#include <stdio.h>
#include <cutils/log.h>
#include <cutils/properties.h>
#include <dlfcn.h>
#include <fcntl.h>

namespace houdini {
    bool hookCheckMethod(void *fnPtr);
    void dvmHookPlatformInvoke(void* pEnv, void* clazz, int argInfo, int argc, const int* argv, const char* shorty, void* func, void* pReturn);
    void* hookDlopen(const char* filename, int flag, bool* useHoudini);
    void* hookDlsym(bool useHoudini, void* handle, const char* symbol);
    void  hookCreateActivity(bool useHoudini, void* createActivityFunc, void* activity, void*houdiniActivity, void* savedState, size_t savedStateSize);
    int hookJniOnload(bool useHoudini, void* func, void* jniVm, void* arg);

    /*
     * Type definition for Houdini
     */
    typedef int(*dvm2hdInit_ptr_t)(void*);
    typedef void*(*dvm2hdDlopen_ptr_t)(const char*, int);
    typedef void*(*dvm2hdDlsym_ptr_t)(void*,const char*);
    typedef int(*dvm2hdNativeMethodHelper_ptr_t)(bool, void*, unsigned char,
               void*, unsigned int, const unsigned char*, const void **);
    typedef bool(*dvm2hdNeeded_ptr_t)(void*);
    typedef int (*HOUDINI_CREATE_ACTIVITY)(void *funcPt,void *x86Code,void *houdiniCode,void *rawSavedState,size_t rawSavedSize);

    dvm2hdInit_ptr_t               dvm2hdInit;
    dvm2hdDlopen_ptr_t             dvm2hdDlopen;
    dvm2hdDlsym_ptr_t              dvm2hdDlsym;
    dvm2hdNativeMethodHelper_ptr_t dvm2hdNativeMethodHelper;
    dvm2hdNeeded_ptr_t             dvm2hdNeeded;
    HOUDINI_CREATE_ACTIVITY        houdiniCreateActivity;
    bool                           libhoudiniInited=false;

#define HOUDINI_PATH       "/system/lib/libhoudini.so"
#define HOUDINI_BUILD_PROP "dalvik.vm.houdini"

/*
 * Get the shorty string for a method.
 */
struct DexProto {
    const void* dexFile;     /* file the idx refers to */
    uint32_t protoIdx;                /* index into proto_ids table of dexFile */
};
struct fake_Method {
    void*    clazz;
    uint32_t accessFlags;
    uint16_t methodIndex;
    uint16_t registersSize;  /* ins + locals */
    uint16_t outsSize;
    uint16_t insSize;
    const char*     name;
    DexProto        prototype;
    const char*     shorty;
};

const char* dvmGetMethodShorty(const struct fake_Method* meth)
{
    return meth->shorty;
}

void* hookDlopen(const char* filename, int flag, bool* useHoudini) {
    void *native_handle;

    native_handle = dlopen(filename, flag);
    if (native_handle) {
        *useHoudini = false;
        return native_handle;
    }

    void *handle;

    struct dvm2hdEnv {
        void *logger;
        void *getShorty;
    } env;

    char propBuf[PROPERTY_VALUE_MAX];
    property_get(HOUDINI_BUILD_PROP, propBuf, "");
    //setting HOUDINI_BUILD_PROP to "on" will enable houdini
    if(strncmp(propBuf, "on", sizeof(propBuf)) && strncmp(propBuf, "", sizeof(propBuf))) {
        return NULL;
    }

    env.logger = (void*)__android_log_print;
    env.getShorty = (void*)dvmGetMethodShorty;
    if (!libhoudiniInited) {
        //TODO: hard code the path currently
        handle = dlopen(HOUDINI_PATH,RTLD_NOW);
        if (handle == NULL) {
            return NULL;
        }
        dvm2hdInit = (dvm2hdInit_ptr_t)dlsym(handle, "dvm2hdInit");
        if (dvm2hdInit == NULL) {
            ALOGE("Cannot find symbol dvm2hdInit, please check the libhoudini library is correct: %s!\n", dlerror());
            return NULL;
        }
        if (!dvm2hdInit((void*)&env)) {
            ALOGE("libhoudini init failed!\n");
            return NULL;
        }

        dvm2hdDlopen = (dvm2hdDlopen_ptr_t)dlsym(handle, "dvm2hdDlopen");
        dvm2hdDlsym = (dvm2hdDlsym_ptr_t)dlsym(handle, "dvm2hdDlsym");
        dvm2hdNeeded = (dvm2hdNeeded_ptr_t)dlsym(handle, "dvm2hdNeeded");
        dvm2hdNativeMethodHelper = (dvm2hdNativeMethodHelper_ptr_t)dlsym(handle, "dvm2hdNativeMethodHelper");
        houdiniCreateActivity = (HOUDINI_CREATE_ACTIVITY)dlsym(handle, "androidrt2hdCreateActivity");
        if (!dvm2hdDlopen || !dvm2hdDlsym || !dvm2hdNeeded || !dvm2hdNativeMethodHelper || !houdiniCreateActivity) {
            ALOGE("The library symbol is missing, please check the libhoudini library is correct: %s!\n", dlerror());
            return NULL;
        }
        libhoudiniInited = true;
    }

    *useHoudini = true;
    return dvm2hdDlopen(filename, flag);
}

bool hookCheckMethod(void *fnPtr) {
    if (libhoudiniInited)
        return dvm2hdNeeded(fnPtr);
    else
        return false;
}

void dvmHookPlatformInvoke(void* pEnv, void* clazz, int argInfo, int argc, const int* argv, const char* shorty, void* func, void* pReturn) {
    const int kMaxArgs = ((argc >= 0) ? argc : 0)+2;    /* +1 for env, maybe +1 for clazz */
    unsigned char types[kMaxArgs+1];
    void* values[kMaxArgs];
    char retType;
    char sigByte;
    int dstArg;

    if (!libhoudiniInited) {
        ALOGE("dvmHookPlatformInvoke() called but houdini not inited");
        return;
    }

    types[0] = 'L';
    values[0] = &pEnv;

    types[1] = 'L';
    if (clazz != NULL) {
        values[1] = &clazz;
    } else {
        values[1] = (void*)argv++;
    }
    dstArg = 2;

    /*
     * Scan the types out of the short signature.  Use them to fill out the
     * "types" array.  Store the start address of the argument in "values".
     */
    retType = *shorty;
    while ((sigByte = *++shorty) != '\0') {
        types[dstArg] = sigByte;
        values[dstArg++] = (void*)argv++;
        if (sigByte == 'D' || sigByte == 'J') {
            argv++;
        }
    }
    types[dstArg] = '\0';

    dvm2hdNativeMethodHelper(false, func, retType, pReturn, dstArg, types, (const void**)values);
}

void* hookDlsym(bool useHoudini, void* handle, const char* symbol) {
    if (libhoudiniInited && useHoudini)
        return dvm2hdDlsym(handle, symbol);
    else
        return dlsym(handle, symbol);
}

void hookCreateActivity(bool useHoudini, void* createActivityFunc, void* activity, void*houdiniActivity, void* savedState, size_t savedStateSize) {
    if (libhoudiniInited && useHoudini) {
        houdiniCreateActivity(createActivityFunc, activity, houdiniActivity, savedState, savedStateSize);
    }
    else {
        void (*f)(void*, void*, size_t) = (void (*)(void*, void*, size_t))createActivityFunc;
        (*f)(activity, savedState, savedStateSize);
    }
}

int hookJniOnload(bool useHoudini, void* func, void* jniVm, void* arg) {
    if (libhoudiniInited && useHoudini) {
        const void* argv[] = {jniVm, NULL};//{gDvm.vmList, NULL};
        int version;
        dvm2hdNativeMethodHelper(true, (void*)func, 'I', (void*)&version, 2, NULL, argv);
        return version;
    }
    else {
        int (*f)(void*, void*) = (int (*)(void*, void*))func;
        return (*f)(jniVm, NULL);
    }
}

}
