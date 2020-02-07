#ifndef _BUS_H_
#define _BUS_H_
#ifdef __cplusplus
extern "C" {
#endif
#include "BusSetup.h"
#include <stdint.h>

typedef struct BUS_API BusGUID_t {
    uint64_t low, high;
} BusGUID;

// Error
typedef enum BusResult_enum {
    BUS_SUCCESS = 0,
    BUS_SYSTEM_ERROR = 1,
    BUS_GUID_NOT_FOUND = 2,
    BUS_IMPL_NOT_FOUND = 3,
    BUS_LIBRARY_NOT_FOUND = 4
} BusResult;
BUS_API const char* BusGetErrorString(BusResult res);

// Context
typedef struct BusCtx_t* BusContext;
BUS_API BusResult BusInit(BusContext* ctx, int multiThread);
BUS_API BusResult BusUninit(BusContext ctx);

// Object
typedef struct BusObject_t* BusObject;
BUS_API BusResult BusObjectCreate(BusContext ctx, BusGUID classID,
                                  BusObject* obj);
BUS_API BusResult BusObjectDestory(BusContext ctx, BusObject obj);

// Trait
typedef struct BUS_API BusTrait_t {
    BusObject obj;
    const void* vtable;
} BusTrait;

#define BUS_CALL(TRAIT, FUNC, ...) \
    TRAIT.vtable.FUNC(TRAIT.obj, TRAIT.vtable.FUNC##ID, __VA_ARGS__)

#define BUS_TRAIT_BEG(TRAIT) typedef struct BUS_API Bus##TRAIT##TraitVTable_t

#define BUS_TRAIT_END(TRAIT)                     \
    Bus##TRAIT##TraitVTable;                     \
    typedef struct BUS_API Bus##TRAIT##Trait_t { \
        BusObject obj;                           \
        const Bus##TRAIT##TraitVTable* vtable;   \
    } Bus##TRAIT##Trait

#define BUS_TRAIT_GUID(TRAIT) Bus##TRAIT##Trait::GUID
#define BUS_TRAIT_GUID_DESC static const BusGUID GUID =
#define BUS_TRAIT_GENERIC_PARAM BusObject obj, uint64_t fid
#define BUS_TRAIT_FUNC(RET, SYM, ...)                               \
    typedef RET (*SYM##Func)(BUS_TRAIT_GENERIC_PARAM, __VA_ARGS__); \
    SYM##Func SYM;                                                  \
    uint64_t SYM##ID

// Builtin Trait
BUS_TRAIT_BEG(Drop) {
    BUS_TRAIT_GUID_DESC{ 0x8FCE5F3ECE614334, 0xAAEDB00F457678A5 };
    BUS_TRAIT_FUNC(void, drop);
}
BUS_TRAIT_END(Drop);

BUS_TRAIT_BEG(Clone) {
    BUS_TRAIT_GUID_DESC{ 0x94FB1A00BDB34752, 0xB287CF1DBD2897BB };
    BUS_TRAIT_FUNC(BusObject, clone);
}
BUS_TRAIT_END(Clone);

BUS_TRAIT_BEG(DataDeleter) {
    BUS_TRAIT_GUID_DESC{ 0x7ACE6C2CC4134B4C, 0xB21582321A7734ED };
    BUS_TRAIT_FUNC(void, free, const void* ptr);
}
BUS_TRAIT_END(DataDeleter);

typedef struct BusReadOnlyData_t {
    const void* data;
    BusDataDeleterTrait free;
} BusReadOnlyData;

BUS_API void BusFreeData(BusReadOnlyData* data);

BUS_TRAIT_BEG(Display) {
    BUS_TRAIT_GUID_DESC{ 0x695DCDCB84F14B6D, 0x97255E4EC605F1FA };
    BUS_TRAIT_FUNC(BusReadOnlyData, display);
}
BUS_TRAIT_END(Display);

BUS_TRAIT_BEG(Serialize) {
    BUS_TRAIT_GUID_DESC{ 0x6AFE2650765C4C86, 0xB38E2A0236D47AFF };
    BUS_TRAIT_FUNC(BusReadOnlyData, serialize);
}
BUS_TRAIT_END(Serialize);

BUS_TRAIT_BEG(Hash) {
    BUS_TRAIT_GUID_DESC{ 0xAEA8007A5028483B, 0xA9063C8C4D95B83A };
    BUS_TRAIT_FUNC(uint64_t, hash);
}
BUS_TRAIT_END(Hash);

BUS_TRAIT_BEG(Equal) {
    BUS_TRAIT_GUID_DESC{ 0xC4AACF15C6AA4E0C, 0x97787897AD7A43F1 };
    BUS_TRAIT_FUNC(int, equal, BusObject rhs);
}
BUS_TRAIT_END(Equal);

// Trait API
BUS_TRAIT_BEG(Select) {
    BUS_TRAIT_GUID_DESC{ 0x64AEA631683B40CD, 0xBB9AB42F8482B0DA };
    BUS_TRAIT_FUNC(void, setTarget, BusGUID traitGUID);
    BUS_TRAIT_FUNC(void, addPotential, BusGUID pluginGUID, BusGUID implGUID,
                   BusGUID classGUID);
    BUS_TRAIT_FUNC(BusGUID, best);
}
BUS_TRAIT_END(Select);
BUS_API BusResult BusInstantiateOneImpl(BusContext ctx, BusGUID traitID,
                                        BusTrait* trait);
BUS_API BusResult BusSelectAndInstantiateOneImpl(BusContext ctx,
                                                 BusGUID traitID,
                                                 BusSelectTrait selector,
                                                 BusTrait* trait);
BUS_API BusResult BusViewAs(BusContext ctx, BusGUID traitID, BusTrait* trait);
BUS_API BusResult BusHasImplFor(BusContext ctx, BusGUID traitID,
                                BusGUID classID);

// Plugin API
typedef struct BusPluginInfo_t {
    BusGUID id;
    const char* symbol;
    const char* desc;
} BusPluginInfo;

typedef BusResult (*BusCallBack)(BusGUID implID, BusGUID classID);

BUS_TRAIT_BEG(Factory) {
    BUS_TRAIT_GUID_DESC{ 0xAEE0881D549F4919, 0x9142B8CB9793581E };
}
BUS_TRAIT_END(Factory);

BUS_TRAIT_BEG(Plugin) {
    BUS_TRAIT_GUID_DESC{ 0x6F81306CC9CB4512, 0xBAA8AA978FF3F188 };
    BUS_TRAIT_FUNC(void, getInfo, BusPluginInfo* info);
    BUS_TRAIT_FUNC(void, registerAllImpl, BusCallback implCallback);
}
BUS_TRAIT_END(Plugin);

BUS_API BusResult BusLoadBuiltinPlugin(BusContext ctx, BusPluginTrait plugin);
BUS_API BusResult BusLoadPluginFromSharedLibrary(BusContext ctx,
                                                 const char* path);

// Exception API

// Logging API

#ifdef __cplusplus
}
#endif  //__cplusplus
#endif  // _BUS_H_
