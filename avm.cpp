#include "avm.h"
#include "binary_stream.h"
#include "utils.h"

avm::ClassClosure * avm::AbcEnv::finddef(const std::string &name)
{
	return finddef([name] (avm::ClassClosure *closure)
	{
		return closure->get_name() == name;
	});
}

avm::ClassClosure * avm::AbcEnv::finddef(std::function<bool(avm::ClassClosure *)> pred)
{
	std::vector<avm::ClassClosure *> results;
	for (size_t i = 0; i < finddef_table->capacity; i++)
	{
		avm::ScriptObject *obj = finddef_table->data[i];

		if (!obj)
		{
			continue;
		}

		auto *closure = obj->get_at<avm::ClassClosure *>(0x20);

		if (reinterpret_cast<uintptr_t>(closure) <= 0x200000001 || (reinterpret_cast<uintptr_t>(closure) & 7) != 0)
		{
			continue;
		}

		if (pred(closure))
		{
			return closure;
		}
	}
	return nullptr;
}

avm::MyTraits avm::Traits::parse_traits(avm::PoolObject *custom_pool)
{
	BinaryStream s { traits_pos };
	custom_pool = custom_pool ? custom_pool : pool;
	MyTraits traits;

	/* auto qname = */ s.read_u32();
	/* auto qname = */ s.read_u32();

	auto flags = s.read_u32();

	if ((flags & 0x8) != 0)
	{
		s.read_u32();
	}

	auto interface_count = s.read_u32();
	for (uint32_t i = 0; i < interface_count; i++)
	{
		s.read_u32();
	}

	/* auto iinit = */ s.read_u32();

	uint32_t trait_count = s.read_u32();
	for (uint32_t j = 0; j < trait_count; j++)
	{
		MyTrait trait;

		uint32_t name = s.read_u32();
		unsigned char tag = s.read<uint8_t>();
		auto kind = avm::TraitKind(tag & 0xf);

		avm::Multiname *mn = custom_pool->get_multiname(name);
		trait.name_index = name;
		trait.name = (mn) ? mn->get_name() : "";
		trait.kind = kind;

		switch(kind)
		{
			case avm::TRAIT_Slot:
			case avm::TRAIT_Const:
			{
				/* uint32_t slot_id	= */ s.read_u32();
				uint32_t type_name  = s.read_u32();
				uint32_t vindex	 = s.read_u32(); // references one of the tables in the constant pool, depending on the value of vkind
				trait.id = vindex;
				trait.type_id = type_name;

				if (vindex)
				{
					/*uint8_t vkind = */ s.read<uint8_t>(); // ignored by the avm
				}

				break;
			}
			case avm::TRAIT_Class:
			{
				/* uint32_t slot_id = */ s.read_u32();
				uint32_t class_index = s.read_u32(); //  is an index that points into the class array of the abcFile entry
				trait.id = class_index;
				break;
			}
			case avm::TRAIT_Method:
			case avm::TRAIT_Getter:
			case avm::TRAIT_Setter:
			{
				// The disp_id field is a compiler assigned integer that is used by the AVM2 to optimize the resolution of
				// virtual function calls. An overridden method must have the same disp_id as that of the method in the 
				// base class. A value of zero disables this optimization.
				/*uint32_t disp_id = */s.read_u32();
				uint32_t method_index = s.read_u32(); // is an index that points into the method array of the abcFile e
				trait.id = method_index;
				trait.temp = name;
				break;
			}
			default:
			{
				utils::log("Invalid trait\n");
				break;
			}
		}

		if (tag & avm::ATTR_metadata)
		{
			uint32_t metadata_count  = s.read_u32();
			for (uint32_t i = 0; i < metadata_count; i++)
			{
				/*uint32_t index = */ s.read_u32();
			}
		}

		traits.add_trait(trait);
	}
	return traits;
}

