#ifndef AVM_H
#define AVM_H
#include <cstdint>
#include <vector>
#include <array>
#include <string>
#include <codecvt> 
#include <locale>
#include <functional>
#include "binary_stream.h"

typedef uintptr_t Atom;


namespace avm
{
    struct ScriptObject;
    struct ClassClosure;
    struct MethodInfo;
    struct MethodEnv;
    struct VTable;
    struct Traits;


    typedef ScriptObject* (*CreateInstanceProc)(ClassClosure *cls);

#ifdef WIN32
    typedef uintptr_t (__fastcall *MethodInvoke_t)(MethodEnv *, uint32_t, uintptr_t *);
#else
    typedef uintptr_t (*MethodInvoke_t)(MethodEnv *, uint32_t, uintptr_t*);
#endif

    const Atom TRUE = (1 << 3 | 5);
    const Atom FALSE = 5;

    enum TraitKind {
        TRAIT_Slot      = 0x00,
        TRAIT_Method    = 0x01,
        TRAIT_Getter    = 0x02,
        TRAIT_Setter    = 0x03,
        TRAIT_Class     = 0x04,
        TRAIT_Const     = 0x06,
        TRAIT_COUNT     = TRAIT_Const+1,
        TRAIT_mask      = 15
    };

    const int ATTR_final       = 0x10; // 1=final, 0=virtual
    const int ATTR_override    = 0x20; // 1=override, 0=new
    const int ATTR_metadata    = 0x40; // 1=has metadata, 0=no metadata



    class MyTrait
    {
    public:
        MyTrait() { }

        avm::TraitKind kind;
        int type_id = 0;
        int id = 0;
        int temp;

        int name_index = 0;

        std::string name;
    };


    struct MyTraits
    {
        inline void add_trait(MyTrait &t)
        {
            traits.push_back(t);
        }

        bool has_trait(const std::string &name)
        {
            for (auto &trait : traits)
            {
                if (trait.name == name)
                {
                    return true;
                }
            }
            return false;
        }

        std::vector<MyTrait> get_slots()
        {
            std::vector<MyTrait> r;
            for (auto &trait : traits)
            {
                if (trait.kind == avm::TRAIT_Slot)
                {
                    r.push_back(trait);
                }
            }
            return r;
        }

        std::vector<MyTrait> traits;
    };



    struct GC
    {
        uint8_t pad[0x2278];
        uintptr_t *zct_bottom;
        uintptr_t *zct_top;
        uintptr_t *zct_max;
    };

    struct GCObject
    {

        uint32_t zct_index() const
        {
            return (reinterpret_cast<const uint32_t *>(this)[2] & 0x0FFFFF00) >> 0x8;
        }
    };

    struct BlockHeader
    {
        uint8_t bibopTag;
        uint8_t bitsShift;
        uint8_t containsPointer;
        uint8_t rcObject;
        uint32_t size;
        GC *gc;
        void *GCAllocBase;
        BlockHeader *next;
    };

    inline static BlockHeader *get_block_header(void *obj)
    {
        return reinterpret_cast<BlockHeader *>(uintptr_t(obj) & (~(4096LL-1LL)));
    }

    struct String : public GCObject
    {
        uintptr_t vtable;
        uint32_t composite;
        uint32_t pad;

        union
        {
            int8_t *data;
            char16_t *data_w;
            uintptr_t offset;
        };
        String *extra;
        uint32_t size;
        uint32_t flags;

        enum {
            TSTR_WIDTH_MASK         = 0x00000001,   // string width (right-aligned for fast access)
            TSTR_TYPE_MASK          = 0x00000006,   // type index, 2 bits
            TSTR_TYPE_SHIFT         = 1,
            // If TSTR_7BIT_FLAG is set, the string has width of k8, with no characters having the high bit set.
            // Thus the string is both 7-bit ascii and "utf8-compatible" as-is; knowing this can produce
            // huge speedups in code that uses utf8 conversion heavily (in conjunction with "ascii" strings, of course).
            // Note that this bit is set lazily (and currently, only by StUTF8String), thus, if this bit is clear,
            // the string might still be 7-bit-ascii... we just haven't checked yet.
            TSTR_7BIT_FLAG          = 0x00000008,
            TSTR_7BIT_SHIFT         = 3,
            TSTR_INTERNED_FLAG      = 0x00000010,   // this string is interned
            TSTR_NOINT_FLAG         = 0x00000020,   // set in getIntAtom() if the string is not an 28-bit integer
            TSTR_NOUINT_FLAG        = 0x00000040,   // set in parseIndex() if the string is not an unsigned integer
            TSTR_UINT28_FLAG        = 0x00000080,   // set if m_index contains value for getIntAtom()
            TSTR_UINT32_FLAG        = 0x00000100,   // set if m_index contains value for parseIndex()
            TSTR_CHARSLEFT_MASK     = 0xFFFFFE00,   // characters left in buffer field (for inplace concat)
            TSTR_CHARSLEFT_SHIFT    = 9
        };

        enum Type
        {
            kDynamic    = 0,    // buffer is on the heap
            kStatic     = 1,    // buffer is static
            kDependent  = 2     // string points into master string
        };
        /// String width constants.
        enum Width
        {
            kAuto   = -1,   // only used in APIs
            k8      = 0,    // chosen such that i<<k8 == i*sizeof(uint8_t)
            k16     = 1     // chosen such that i<<k16 == i*sizeof(uint16_t)
        };

        /// Is this string empty?
        inline bool        isEmpty() const { return size == 0; }
        /// Return the width constant.
        inline Width       getWidth() const { return Width(flags & TSTR_WIDTH_MASK); }
        /// Return the string type.
        inline int32_t     getType() const { return ((flags & TSTR_TYPE_MASK) >> TSTR_TYPE_SHIFT); }
        /// Return true iff getType() == kDependent.
        inline bool        isDependent() const { return (flags & (kDependent << TSTR_TYPE_SHIFT)) != 0; }
        /// Return true iff getType() == kStatic.
        inline bool        isStatic() const { return (flags & (kStatic << TSTR_TYPE_SHIFT)) != 0; }
        /// Is this an interned string?
        inline bool        isInterned() const { return (flags & TSTR_INTERNED_FLAG) != 0; }

        std::string read(uintptr_t offset = 0)
        {
            if (isDependent())
                return extra->read(offset);

            //p8 = self->m_extra.master->m_buffer.p8 + self->m_buffer.offset_bytes;

            if (getWidth() == Width::k16)
            {
                std::wstring_convert<std::codecvt_utf8_utf16<char16_t>,char16_t> convert;
                return convert.to_bytes(
                        reinterpret_cast<char16_t *>(&data[offset]),
                        reinterpret_cast<char16_t *>(&data[size * 2]));
            }

            return std::string(&data[offset], &data[size]);
        }

        wchar_t operator[](size_t index)
        {
            return 0;
        }
    };

    struct Namespace
    {
        uintptr_t vtable;
        uintptr_t pad;
        uintptr_t m_prefix;
        uintptr_t uri_and_type;

        enum NamespaceType
        {
            NS_Public = 0,
            NS_Protected = 1,
            NS_PackageInternal = 2,
            NS_Private = 3,
            NS_Explicit = 4,
            NS_StaticProtected = 5
        };

        std::string get_uri()
        {
            auto *s = reinterpret_cast<String *>(uri_and_type & ~7);
            return s ? s->read() : "";
        }

        inline NamespaceType type()
        {
            return (NamespaceType)(uri_and_type & 7);
        }
    };

    struct Multiname
    {
        String *name;
        Namespace *ns;
        uint32_t flags;
        uint32_t next_type; // for param type

        std::string get_name()
        {
            if (!(flags & 8) && name) // RTNAME
                return name->read();
            return "";
        }
    };

    struct Builtins
    {
        uint8_t pad[0x18];
        avm::ClassClosure *classes[1];
    };

    struct Toplevel
    {
        uintptr_t *cpp_vtable;
        uint8_t pad0[0x48 - 0x8];
        Builtins *builtins;

        avm::ClassClosure *get_builtin(uint32_t index)
        {
            return builtins->classes[index];
        }
    };

    struct AvmCore
    {
        uint8_t pad0[0x110];
        uintptr_t exec;
    };

    struct PoolObject
    {
        uintptr_t cpp_vtable;
        AvmCore *core;
        uint8_t pad0[0x80 - 0x10];
        uint32_t ns_count;
        uint32_t padns;
        uint8_t pad1[0x98 - 0x88];
        uintptr_t precomp_mn_size;
        uint8_t pad2[0xe8 - 0xa0];
        Multiname *precomp_mn;

        uint8_t pad3[0x130 - 0xf0];

        uint8_t *abc_strings_start;
        uint8_t *abc_strings_end;
        uint8_t *abc_strings;

        uint8_t pad4[0x180 - 0x148];

        MethodInfo **method_list;

        uint8_t pad5[0x190 - 0x188];

        int32_t *method_name_indices; // add 4

        MethodInfo *get_method(uint32_t id)
        {
            return method_list[id + 2];
        }

        Multiname *get_multiname(uint32_t id)
        {
            return (id < precomp_mn_size) ? &precomp_mn[id + 1] : nullptr;
        }

        Multiname *find_multiname(const std::string &name)
        {
            for (uintptr_t i = 0; i < precomp_mn_size; i++)
            {
                if (precomp_mn[i+1].get_name() == name)
                {
                    return &precomp_mn[i+1];
                }
            }
            return nullptr;
        }

        std::string get_method_name(uint32_t method_index)
        {
            int32_t name_index = method_name_indices[1 + method_index];
            if (name_index < 0)
            {
                Multiname *mn = get_multiname(-name_index);
                return (mn) ? mn->get_name() : "";
            }
            else
            {
                //if (hasString(index))
                //name = this->getString(index);
            }
            return "";
        }
    };

    struct ScopeOrTraits
    {
        uintptr_t ptr;

        bool is_traits() const { return (ptr & 1) == 0; }

        Traits *traits() const { return reinterpret_cast<Traits *>(ptr); }
    };

    struct MethodInfo
    {
        uintptr_t vtable;
        MethodInvoke_t method_proc;
        MethodInvoke_t invoker;
        uint8_t pad0[8];
        ScopeOrTraits declarer;
        ScopeOrTraits activation;
        PoolObject *pool;
        uint8_t *abc_info;
        int32_t id;
        uint32_t padid;
        uint8_t *abc_code;
        uint8_t pad2[0x60 - 0x50];
        uint32_t flags;

        std::vector<uint32_t> get_params()
        {
            std::vector<uint32_t> result;

            BinaryStream str(abc_info);
            uint32_t param_count = str.read_u32();
            /*uint32_t ret_type =*/ str.read_u32();

            for (uint32_t i = 0; i < param_count; i++)
            {
                result.push_back(str.read_u32());
            }
            return result;
        }

        std::string name()
        {
            return pool->get_method_name(id);
        }

        inline bool compiled() {
            return ((flags >> 21) & 1) == 1;
        }
    };

    struct AbcEnv
    {
        struct FinddefTable
        {
            uintptr_t pad0;
            uint32_t capacity;
            uint32_t pad1;
            avm::ScriptObject *data[1];

            avm::ScriptObject *operator[](size_t pos)
            {
                return data[pos];
            }

        };

        avm::ClassClosure * finddef(const std::string &name);
        avm::ClassClosure * finddef(std::function<bool(avm::ClassClosure *)> pred);

        uintptr_t cpp_vtable;
        PoolObject *pool;
        uint8_t pad0[0x28 - 0x10];
        FinddefTable *finddef_table;
        MethodEnv *methods[1];
    };

    struct Scope
    {
        uintptr_t cpp_vtable;
        VTable *vtable;
        AbcEnv *abc_env;
        uintptr_t scope_traits;
        Namespace * default_xml_namespace;
        uintptr_t scopes[1];
        //Atom                            GC_ATOMS_SMALL(_scopes[1], "getSize()");
    };

    struct MethodEnv 
    {
        uintptr_t vtable;
        MethodInvoke_t method_proc;
        MethodInfo *method_info;
        Scope *scope;

        uintptr_t invoke(int argc, uintptr_t *argv)
        {
            return method_proc(this, argc, argv);
        }
    };

    enum SlotStorageType
    {
        // we rely on these 4 being first, so we can do <= SST_scriptobject in isAtomOrRCObjectSlot.
        // SST_atom is most frequently encountered, so value of zero is best
        SST_atom,
        SST_string,
        SST_namespace,
        SST_scriptobject,

        SST_int32,
        SST_uint32,
        SST_bool32,
        SST_double,
        /* insert new values above this line, but make sure that there are no more than 16 entries in total (excluding SST_MAX_VALUE) */
        SST_MAX_VALUE
    };

    struct SlotInfo
    {
        Traits *type;
        uint64_t offset_and_sst;

        inline SlotStorageType sst() const
        {
            return SlotStorageType(offset_and_sst & 0xf);
        }

        inline uint32_t offset() const
        {
            return (offset_and_sst >> 4) << 2;
        }
    };

    struct TraitsBindings
    {
        uintptr_t vtable;
        uintptr_t pad;
        Traits *owner;
        TraitsBindings *base;
        uintptr_t m_bindings;
        uint32_t slot_count;
        uint32_t method_count;
        uint32_t slot_size;
        uint32_t types_valid;

        inline SlotInfo *slots()
        {
            return (SlotInfo *)(this+1);
        }
    };

    struct Traits
    {
        void *cpp_vtable;
        AvmCore *core;
        Traits *base;
        uint8_t pad0[0x78 - 0x18];
        PoolObject *pool;
        Traits *itraits;
        Namespace *ns;
        String *_name;
        Namespace *protected_ns;
        MethodInfo *init;
        uintptr_t create_class_closure;
        uint8_t *traits_pos;
        uint8_t *metadata_pos;
        uintptr_t slot_destroy_info;
        uintptr_t tbref;
        uintptr_t tmref;
        uintptr_t declaring_scope;
        uintptr_t pad1;
        uint16_t sizeofInstance;   // sizeof implementation class, e.g. ScriptObject, etc. < 64k. Not counting extra room for slots.
        uint16_t offsetofSlots;    // offset of first slot; 0 means "put the slots at the end of my immediate parent"
        uint32_t hashTableOffset;  // offset to our hashtable (or 0 if none)
        uint32_t totalSize;        // total size, including sizeofInstance + slots + hashtable
        uint8_t  builtinType;                // BuiltinType enumeration -- only need 5 bits but stored in uint8_t for faster access
        uint8_t pos_type;                  // TraitsPosType enumeration -- only need 3 bits but stored in uint8_t for faster access
        uint8_t bindingCapLog2;           // if nonzero, log2 of the cap needed for bindings
        uint8_t supertype_offset;         // if this traits is primary, == offset in primary_supertypes array; otherwise == offset of supertype_cache

        std::string name() const
        {
            return (_name) ? _name->read() : "";
        }

        MyTraits parse_traits(avm::PoolObject *custom_pool = nullptr);
    };

    struct VTable
    {
        uintptr_t vtable;
        Toplevel *toplevel;
        MethodEnv *einit;
        VTable *base;
        VTable *ivtable;
        Traits *traits;
        CreateInstanceProc create_instance_proc;
        uint8_t pad0[0x78 - 0x38];
        MethodEnv *methods[1];

        std::vector<MethodEnv *> get_methods()
        {
            std::vector<MethodEnv *> result;
            size_t vtable_size = get_block_header(this)->size;

            for (size_t i = 0; i < (vtable_size - 0x78) / 8; i++)
            {
                result.push_back(methods[i]);
            }

            return result;
        }
    };

    struct ScriptObject
    {
        typedef uintptr_t (*getAtomProperty_t)(ScriptObject *, uintptr_t name_atom);
        struct Vtable 
        {
            uint8_t pad0[0x30];
            getAtomProperty_t getAtomProperty;
        };

        Vtable *vt;;
        uintptr_t gc_info;
        VTable *vtable;
        ScriptObject *delegate;

        template<typename T>
        void write_at(uintptr_t offset, T val)
        {
            *reinterpret_cast<T *>(uintptr_t(this) + offset) = val;
        }

        inline AvmCore *core()
        {
            return vtable->traits->core;
        }

        inline AbcEnv *get_abcenv()
        {
            return vtable->einit->scope->abc_env;
        }

        std::string get_name() const
        {
            return vtable->traits->name();
        }

        uintptr_t call_method(uint32_t index, uint32_t argc, void *argv)
        {
            std::vector<uintptr_t> args { reinterpret_cast<uintptr_t>(this)};

            if (argc)
                args.insert(args.end(), reinterpret_cast<uintptr_t *>(argv), &reinterpret_cast<uintptr_t *>(argv)[argc]);

            avm::MethodEnv *env = vtable->methods[index];
            if (!env)
                return 0;
            return env->invoke(argc, args.data());
        }

        uintptr_t call(uint32_t index)
        {
            return call_method(index, 0, nullptr);
        }

        template<typename ... Ts>
        uintptr_t call(uint32_t index, Ts... args)
        {
            auto size = sizeof...(Ts);
            uintptr_t arg_buf[size] = { reinterpret_cast<uintptr_t>(args)...};
            return call_method(index, size, arg_buf);
        }

        template <typename T>
        void set_at(const T &value, uintptr_t offset)
        {
            *(T *)((uintptr_t)this + offset) = value;
        }

        template <typename T, typename ... Ts>
        void set_at(const T &value, uintptr_t offset, Ts ... offsets)
        {
            reinterpret_cast<ScriptObject **>((uintptr_t)this + offset)[0]->set_at<T>(value, offsets...);
        }

        template <typename T>
        T get_at(uintptr_t offset) const
        {
            return *(T *)((uintptr_t)this + offset);
        }

        template <typename T, typename ... Ts>
        T get_at(uintptr_t offset, Ts ... offsets) const
        {
            return reinterpret_cast<ScriptObject **>((uintptr_t)this + offset)[0]->get_at<T>(offsets...);
        }
    };

    struct ClassClosure : public ScriptObject
    {
        ScriptObject *prototype;
        CreateInstanceProc create_instance_proc;

        ScriptObject *construct_instance(Atom *argv, uint32_t argc)
        {
            std::vector<Atom> real_argv { reinterpret_cast<uintptr_t>(this) };

            if (argc)
            {
                real_argv.insert(real_argv.end(), argv, &argv[argc]);
            }

            ScriptObject *inst = this->vtable->ivtable->create_instance_proc(this);
            real_argv[0] = reinterpret_cast<Atom>(inst);
            this->vtable->ivtable->einit->invoke(argc, real_argv.data());
            return inst;
        }

        ScriptObject *construct()
        {
            return construct_instance(nullptr, 0);
        }

        template<typename ... Ts>
        ScriptObject *construct(Ts... args)
        {
            std::array<Atom, sizeof...(Ts)> argv = { { args ... } };
            return construct_instance(argv.data(), argv.size());
        }
    };

    struct Array : public ScriptObject
    {
        uintptr_t *data;
        uint32_t size;
        uint32_t pad;

        uintptr_t &operator[](size_t index)
        {
            return data[index+2];
        }

        inline void set_data(uintptr_t *new_data, size_t size)
        {
            for (size_t i = 0; i < size; i++)
                data[i+2] = new_data[i];
        }
    };

    template<typename T>
    inline static T remove_kind(T val)
    {
        return T(reinterpret_cast<uintptr_t>(val) & ~7);
    }
};


#endif // AVM_H
